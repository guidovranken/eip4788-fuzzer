#include <cstdint>
#include <cassert>
#include <cstdlib>
#include <optional>
#include <map>
#include <iostream>

#include <boost/algorithm/hex.hpp>
#include <intx/intx.hpp>
#include "json.hpp"

extern "C" {
    #include "xxhash.h"
    #include "eip4788.h"
}

#if defined(FUZZER_WITH_PYTHON)
# define PY_SSIZE_T_CLEAN
# include <Python.h>
# include <libgen.h>
void* python_FuzzerRunOne = nullptr;
#endif

using Buffer = std::vector<uint8_t>;
using uint256 = intx::uint256;

#include "constants.hpp"
#include "util.hpp"
#include "structs.hpp"
#include "eip4788.hpp"
#include "invariants.hpp"
#include "harness-differential.hpp"
#include "harness-invariants.hpp"

#ifdef NDEBUG
# error "NDEBUG must not be set (asserts must be functional)"
#endif

#if defined(FUZZER_WITH_PYTHON)
static std::string ToAbsolutePath(const std::string argv0, const std::string relativePath) {
    char absoluteRootPath[PATH_MAX+1];
    char argv0Copy[argv0.size()+1];
    memcpy(argv0Copy, argv0.c_str(), argv0.size()+1);
    if ( realpath(dirname(argv0Copy), absoluteRootPath) == nullptr ) {
        printf("Fatal error: Cannot resolve absolute root path\n");
        abort();
    }

    return std::string(std::string(absoluteRootPath) + "/" + relativePath);
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
    (void)argc;

    const std::string argv0 = (*argv)[0];

    const std::string absoluteCPythonInstallPath = ToAbsolutePath(argv0, "cpython-install");
    const std::string absoluteScriptPath = ToAbsolutePath(argv0, "eip4788.py");

    std::vector<uint8_t> program;

    {
        if ( setenv("PYTHONHOME", absoluteCPythonInstallPath.c_str(), 1) != 0 ) {
            printf("Fatal error: Cannot set PYTHONHOME\n");
            abort();
        }
    }

    FILE* fp = fopen(absoluteScriptPath.c_str(), "rb");
    if ( fp == nullptr ) {
        printf("Fatal error: Cannot open script: %s\n", absoluteScriptPath.c_str());
        abort();
    }

    fseek (fp, 0, SEEK_END);
    long length = ftell(fp);
    if ( length < 1 ) {
        printf("Fatal error: Cannot retrieve script file size\n");
        abort();
    }
    fseek (fp, 0, SEEK_SET);
    program.resize(length);
    if ( fread(program.data(), 1, length, fp) != static_cast<size_t>(length) ) {
        printf("Fatal error: Cannot read script\n");
        abort();
    }
    fclose(fp);

    std::string code = std::string(program.data(), program.data() + program.size());

    Py_Initialize();

    {
        std::string setArgv0;
        setArgv0 += "import sys";
        setArgv0 += "\n";
        setArgv0 += "sys.argv[0] = '" + absoluteScriptPath + "'\n";
        if ( PyRun_SimpleString(setArgv0.c_str()) != 0 ) {
            printf("Fatal: Cannot set argv[0]\n");
            PyErr_PrintEx(1);
            abort();
        }
    }

    {
        std::string setPYTHONPATH;
        setPYTHONPATH += "import sys";
        setPYTHONPATH += "\n";
        setPYTHONPATH += "sys.path.append('" + absoluteScriptPath + "')\n";
        setPYTHONPATH += "\n";
        if ( PyRun_SimpleString(setPYTHONPATH.c_str()) != 0 ) {
            printf("Fatal: Cannot set PYTHONPATH\n");
            PyErr_PrintEx(1);
            abort();
        }
    }

    PyObject *pValue, *pModule, *pLocal;

    pModule = PyModule_New("fuzzermod");
    PyModule_AddStringConstant(pModule, "__file__", "");
    pLocal = PyModule_GetDict(pModule);
    pValue = PyRun_String(code.c_str(), Py_file_input, pLocal, pLocal);

    if ( pValue == nullptr ) {
        printf("Fatal: Cannot create Python function from string\n");
        PyErr_PrintEx(1);
        abort();
    }
    Py_DECREF(pValue);

    python_FuzzerRunOne = PyObject_GetAttrString(pModule, "FuzzerRunOne");

    if (
            python_FuzzerRunOne == nullptr ||
            !PyCallable_Check(static_cast<PyObject*>(python_FuzzerRunOne))) {
        printf("Fatal: FuzzerRunOne not defined or not callable\n");
        abort();
    }

    return 0;
}
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
#if defined(FUZZER_DIFFERENTIAL)
    harness::differential::Run(data, size);
#elif defined(FUZZER_INVARIANTS)
    harness::invariants::Run(data, size);
#else
# error "No harness specified"
#endif
    return 0;
}
