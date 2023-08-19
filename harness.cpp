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
