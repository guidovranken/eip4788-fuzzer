#include <cstdint>
#include <optional>
#include <cassert>

#include <intx/intx.hpp>
#include "json.hpp"

extern "C" {
    #include "xxhash.h"
    #include "eip4788.h"
}

#ifdef NDEBUG
# error "NDEBUG must not be set (asserts must be functional)"
#endif

using Buffer = std::vector<uint8_t>;
using uint256 = intx::uint256;

namespace constants {
    using namespace intx;
    constexpr uint256 HISTORICAL_ROOTS_MODULUS = 98304;
    constexpr uint256 SYSTEM_ADDRESS = 0xfffffffffffffffffffffffffffffffffffffffe_u256;
    constexpr uint64_t ShanghaiTimestamp = 1681338455ULL;
}

namespace util {
    static uint256 load(const uint8_t* data) {
        return intx::be::unsafe::load<uint256>(data);
    }

    static uint256 load(const Buffer& v) {
        return load(v.data());
    }

#define ADVANCE(s) *data += s; remaining -= s;
    template <class T>
    static std::optional<T> extract(const uint8_t** data, size_t& remaining) {
        T ret;

        if ( remaining < sizeof(ret) ) {
            return std::nullopt;
        }

        memcpy(&ret, data, sizeof(ret));

        ADVANCE(sizeof(ret));

        return ret;
    }

    template <>
    std::optional<bool> extract(const uint8_t** data, size_t& remaining) {
        const auto ret = extract<uint16_t>(data, remaining);
        if ( ret == std::nullopt ) return std::nullopt;

        return (*ret % 2) ? true : false;
    }

    template <>
    std::optional<uint256> extract(const uint8_t** data, size_t& remaining) {
        if ( remaining < 32 ) return std::nullopt;

        const auto ret = load(*data);

        ADVANCE(32);

        return ret;
    }

    template <>
    std::optional<Buffer> extract(const uint8_t** data, size_t& remaining) {
        const auto size = extract<uint16_t>(data, remaining);
        if ( size == std::nullopt ) return std::nullopt;

        if ( remaining < *size ) return std::nullopt;

        Buffer ret(*size);
        memcpy(ret.data(), *data, *size);

        ADVANCE(*size);

        return ret;
    }
#undef ADVANCE

    static Buffer save(const uint256& v) {
        uint8_t bytes[32];
        intx::be::unsafe::store(bytes, v);
        return {bytes, bytes + 32};
    }
}

/* Mock EVM storage */
class Storage {
    public:
        uint256 Get(const uint256& address) const {
            (void)address;
            return {};
        }

        void Set(const uint256& address, const uint256& v) {
            (void)address;
            (void)v;
        }

        /* Use xxHash to hash the storage */
        uint64_t Hash(void) const {
            auto h = XXH64_createState();
            assert(XXH64_reset(h, 0) != XXH_ERROR);

            /* TODO iterate over storage */
            //assert(XXH64_update(h, data, size) != XXH_ERROR);

            const auto hash = XXH64_digest(h);
            XXH64_freeState(h);
            return hash;
        }
};

class Input {
    public:
        uint256 caller;
        Buffer calldata;
        Storage storage;
        uint64_t timestamp;
        uint64_t gas;

        /* Deserialize variables from the fuzzer input */
        static std::optional<Input> Extract(const uint8_t** data, size_t& remaining) {
#define EXTRACT(var, T) \
            const auto var = extract<T>(data, remaining); \
            if ( var == std::nullopt ) return std::nullopt;
#define EXTRACT2(var, T) \
            EXTRACT(var, T); \
            ret.var = *var;

            using namespace util;

            Input ret;

            EXTRACT2(caller, uint256);
            EXTRACT2(calldata, Buffer);

            /* Set zero or more storage entries */
            while ( true ) {
                EXTRACT(continue_, bool);
                if ( *continue_ == false ) {
                    break;
                }

                EXTRACT(address, uint256);
                EXTRACT(v, uint256);

                ret.storage.Set(*address, *v);
            }

            EXTRACT2(timestamp, uint64_t);
            if ( ret.timestamp < constants::ShanghaiTimestamp ) {
                /* Shanghai is needed for the PUSH0 opcode */
                ret.timestamp = constants::ShanghaiTimestamp;
            }

            EXTRACT2(gas, uint64_t);

            return ret;
#undef EXTRACT
#undef EXTRACT2
        }

        nlohmann::json Json(void) const {
            nlohmann::json ret;
            ret["caller"] = util::save(caller);
            ret["calldata"] = calldata;
            /* TODO storage */
            ret["timestamp"] = timestamp;
            ret["gas"] = gas;
            return ret;
        }
};

class Eip4788 {
    public:
        struct ReturnValue {
            bool reverted;
            Buffer v;

            static ReturnValue revert(void) {
                return ReturnValue{
                    .reverted = true,
                    .v = {},
                };
            }

            static ReturnValue value(const Buffer& v) {
                return ReturnValue{
                    .reverted = false,
                    .v = v,
                };
            }

            static ReturnValue value(const uint256& v) {
                return value(util::save(v));
            }

            bool operator==(const ReturnValue& rhs) const {
                return reverted == rhs.reverted && v == rhs.v;
            }
        };

        static ReturnValue run(Input& input) {
            if ( input.caller == constants::SYSTEM_ADDRESS ) {
                return set(input);
            } else {
                return get(input);
            }
        }

        static ReturnValue get(const Input& input) {
            if ( input.calldata.size() != 32 ) {
                return ReturnValue::revert();
            }

            const auto timestamp_idx =
                util::load(input.calldata) %
                constants::HISTORICAL_ROOTS_MODULUS;
            const auto timestamp = input.storage.Get(timestamp_idx);

            if ( timestamp != timestamp_idx ) {
                return ReturnValue::revert();
            }

            const auto root_idx = timestamp_idx + constants::HISTORICAL_ROOTS_MODULUS;
            const auto root = input.storage.Get(root_idx);

            return ReturnValue::value(root);
        }

        static ReturnValue set(Input& input) {
            const auto timestamp_idx =
                uint256(input.timestamp) %
                constants::HISTORICAL_ROOTS_MODULUS;
            const auto root_idx = timestamp_idx + constants::HISTORICAL_ROOTS_MODULUS;

            input.storage.Set(timestamp_idx, input.timestamp);
            (void)root_idx;
            //input.storage.Set(root_idx, input.calldata);
            return ReturnValue::value(Buffer{});
        }
};

struct ExecutionResult {
    Eip4788::ReturnValue ret;
    uint64_t hash;

    bool operator==(const ExecutionResult& rhs) const {
        return ret == rhs.ret && hash == rhs.hash;
    }
};

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
    const auto input = Input::Extract(&data, size);
    if ( input == std::nullopt ) return 0;

    ExecutionResult cpp, native;

    /* Run the C++ implementation */
    {
        auto inp = *input;
        const auto ret = Eip4788::run(inp);
        const auto hash = inp.storage.Hash();
        cpp = {.ret = ret, .hash = hash};
    }

    /* Run the canonical bytecode implementation */
    {
        auto jsonStr = input->Json().dump();
        const GoSlice inp = {
            .data = jsonStr.data(),
            .len = static_cast<GoInt>(jsonStr.size()),
            .cap = static_cast<GoInt>(jsonStr.size())};
        Native_Eip4788(inp);
        /* TODO get result */
    }

    /* TODO compare return value and storage hash */
    //assert(cpp == native);

    return 0;
}
