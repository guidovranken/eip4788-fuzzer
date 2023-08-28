namespace harness {
    namespace differential {
        inline void Run(const uint8_t* data, size_t size) {
            Native_Eip4788_Reset();
            const uint8_t** data_ = &data;
            Storage storage;

            while ( true ) {
                const auto input = Input::Extract(data_, size, storage);
                if ( input == std::nullopt ) return;

                ExecutionResult cpp, native;

                /* Run the C++ implementation */
                {
                    auto inp = *input;
                    const auto ret = Eip4788::run(inp, storage);
                    const auto hash = storage.Hash();
                    cpp = {.ret = ret, .hash = hash};
                }

                /* Run the canonical bytecode implementation */
                {
                    auto jsonStr = input->Json(storage).dump();
                    const auto inp = util::ToGoSlice(
                            jsonStr.data(),
                            jsonStr.size());
                    Native_Eip4788_Run(inp);
                    const auto res = util::load(Native_Eip4788_Result());
                    native = ExecutionResult::FromJson(res);
                }

                assert(cpp == native);

#if defined(FUZZER_WITH_PYTHON)
                /* Run the Python implementation */
                ExecutionResult py;
                {
                    const auto jsonStr = input->Json(storage).dump();
                    PyObject *pArgs, *pValue;

                    pArgs = PyTuple_New(1);
                    pValue = PyBytes_FromStringAndSize(jsonStr.c_str(), jsonStr.size());
                    PyTuple_SetItem(pArgs, 0, pValue);

                    pValue = PyObject_CallObject(
                            static_cast<PyObject*>(python_FuzzerRunOne),
                            pArgs);

                    assert(pValue != nullptr);

                    assert(PyBytes_Check(pValue));
                    {
                        /* Retrieve output */

                        uint8_t* output;
                        Py_ssize_t outputSize;
                        assert(PyBytes_AsStringAndSize(pValue, (char**)&output, &outputSize) != -1);
                        auto j = nlohmann::json::parse(
                                std::string(output, output + outputSize));
                        const auto storage = Storage::FromJson(j["Storage"]);
                        j["Hash"] = storage.Hash();
                        py = ExecutionResult::FromJson(j);
                    }

                    Py_DECREF(pValue);
                    Py_DECREF(pArgs);
                }
                assert(py == native);
#endif
            }
        }
    }
}
