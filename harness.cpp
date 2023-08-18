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
    constexpr uint64_t FORK_TIMESTAMP = ShanghaiTimestamp;
    /* Shanghai is needed for the PUSH0 opcode */
    static_assert(FORK_TIMESTAMP >= ShanghaiTimestamp);
    constexpr uint256 AddressMask = 0xffffffffffffffffffffffffffffffffffffffff_u256;
}

namespace util {
    static Buffer trim32(Buffer v) {
        v.resize(32, 0);
        return v;
    }

    constexpr uint256 load(const uint8_t* data) {
        return intx::be::unsafe::load<uint256>(data);
    }

    static uint256 load(const Buffer& v) {
        return load(trim32(v).data());
    }

#define ADVANCE(s) *data += s; remaining -= s;
    template <class T>
    static std::optional<T> extract(const uint8_t** data, size_t& remaining) {
        T ret;

        if ( remaining < sizeof(ret) ) {
            return std::nullopt;
        }

        std::array<uint8_t, sizeof(T)> bytes;
        memcpy(bytes.data(), *data, bytes.size());
        std::reverse(bytes.begin(), bytes.end());
        memcpy(&ret, bytes.data(), sizeof(T));

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

    static void hash(XXH64_state_t*h, const Buffer& data) {
        assert(XXH64_update(h, data.data(), data.size()) != XXH_ERROR);
    }

    static Buffer unhex(const std::string& data) {
        std::vector<uint8_t> ret;
        boost::algorithm::unhex(data, std::back_inserter(ret));
        return ret;
    }

#if !defined(FUZZER_INVARIANTS)
    constexpr GoSlice ToGoSlice(void* data, const size_t size) {
        return GoSlice{
            .data = data,
            .len = static_cast<GoInt>(size),
            .cap = static_cast<GoInt>(size)};
    }

    static std::string load(char* s) {
        const std::string ret(s);
        free(s);
        return ret;
    }
#endif
}

/* Mock EVM storage */
class Storage {
    private:
        std::map<uint256, uint256> storage;
    public:
        uint256 Get(const uint256& address) const {
            if ( storage.count(address) ) {
                return storage.at(address);
            } else {
                return 0;
            }
        }

        void Set(const uint256& address, const uint256& v) {
            storage[address] = v;
        }

        /* Use xxHash to hash the storage */
        uint64_t Hash(void) const {
            auto h = XXH64_createState();
            assert(XXH64_reset(h, 0) != XXH_ERROR);

            for (const auto& kv: storage) {
                util::hash(h, util::save(kv.first));
                util::hash(h, util::save(kv.second));
            }

            const auto hash = XXH64_digest(h);
            XXH64_freeState(h);
            return hash;
        }

        const std::map<uint256, uint256>& MapRef(void) const {
            return storage;
        }
};

class Input {
    public:
        uint256 caller;
        Buffer calldata;
        uint64_t timestamp;
        uint64_t gas;

        /* Deserialize variables from the fuzzer input */
        static std::optional<Input> Extract(
                const uint8_t** data,
                size_t& remaining,
                Storage& storage,
                const bool fill_storage = true) {
#define EXTRACT(var, T) \
            const auto var = extract<T>(data, remaining); \
            if ( var == std::nullopt ) return std::nullopt;
#define EXTRACT2(var, T) \
            EXTRACT(var, T); \
            ret.var = *var;

            using namespace util;

            Input ret;

            EXTRACT2(caller, uint256);
            /* An address has only 20 bytes, so remove the upper 12 */
            ret.caller &= constants::AddressMask;

            EXTRACT2(calldata, Buffer);

            if ( fill_storage == true ) {
                /* Set zero or more storage entries */
                while ( true ) {
                    EXTRACT(continue_, bool);
                    if ( *continue_ == false ) {
                        break;
                    }

                    EXTRACT(address, uint256);
                    EXTRACT(v, uint256);

                    storage.Set(*address, *v);
                }
            }

            EXTRACT2(timestamp, uint64_t);
            if ( ret.timestamp < constants::FORK_TIMESTAMP ) {
                ret.timestamp = constants::FORK_TIMESTAMP;
            }

            EXTRACT2(gas, uint64_t);

            return ret;
#undef EXTRACT
#undef EXTRACT2
        }

        nlohmann::json Json(Storage& storage) const {
            nlohmann::json ret;

            ret["caller"] = util::save(caller);
            ret["calldata"] = calldata;

            nlohmann::json storage_;
            for (const auto& kv : storage.MapRef()) {
                storage_[intx::hex(kv.first)] = intx::hex(kv.second);
            }
            ret["storage"] = storage_;

            ret["timestamp"] = timestamp;
            ret["gas"] = gas;

            return ret;
        }
};

class Eip4788 {
    public:
        struct ReturnValue {
            bool reverted;
            Buffer data;

            static ReturnValue revert(void) {
                return ReturnValue{
                    .reverted = true,
                    .data = {},
                };
            }

            static ReturnValue value(const Buffer& data) {
                return ReturnValue{
                    .reverted = false,
                    .data = data,
                };
            }

            static ReturnValue value(const uint256& data) {
                return value(util::save(data));
            }

            bool operator==(const ReturnValue& rhs) const {
                return reverted == rhs.reverted && data == rhs.data;
            }
        };

        static ReturnValue run(const Input& input, Storage& storage) {
            if ( input.caller == constants::SYSTEM_ADDRESS ) {
                return set(input, storage);
            } else {
                return get(input, storage);
            }
        }

        static ReturnValue get(const Input& input, const Storage& storage) {
            if ( input.calldata.size() != 32 ) {
                return ReturnValue::revert();
            }

            const auto calldata_u256 = util::load(input.calldata);

            const auto timestamp_idx =
                calldata_u256 %
                constants::HISTORICAL_ROOTS_MODULUS;
            const auto timestamp = storage.Get(timestamp_idx);

            if ( timestamp != calldata_u256 ) {
                return ReturnValue::revert();
            }

            const auto root_idx = timestamp_idx + constants::HISTORICAL_ROOTS_MODULUS;

            assert(timestamp_idx != root_idx);

            const auto root = storage.Get(root_idx);

            return ReturnValue::value(root);
        }

        static ReturnValue set(const Input& input, Storage& storage) {
            const auto timestamp_idx =
                uint256(input.timestamp) %
                constants::HISTORICAL_ROOTS_MODULUS;
            const auto root_idx = timestamp_idx + constants::HISTORICAL_ROOTS_MODULUS;

            assert(timestamp_idx != root_idx);

            storage.Set(timestamp_idx, input.timestamp);
            storage.Set(root_idx, util::load(input.calldata));
            return ReturnValue::value(Buffer{});
        }
};

struct ExecutionResult {
    Eip4788::ReturnValue ret;
    uint64_t hash;

    bool operator==(const ExecutionResult& rhs) const {
        return ret == rhs.ret && hash == rhs.hash;
    }

    static ExecutionResult FromJson(const nlohmann::json& j) {
        return ExecutionResult{
            .ret = Eip4788::ReturnValue{
                .reverted = j["Ret"]["Reverted"].get<bool>(),
                .data = util::unhex(j["Ret"]["Data"].get<std::string>()),
            },
            .hash = j["Hash"].get<uint64_t>()
        };
    }

    static ExecutionResult FromJson(const std::string& s) {
        return FromJson(nlohmann::json::parse(s));
    }
};

#if defined(FUZZER_DIFFERENTIAL)
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    Native_Eip4788_Reset();
    const uint8_t** data_ = &data;
    Storage storage;

    while ( true ) {
        const auto input = Input::Extract(data_, size, storage);
        if ( input == std::nullopt ) return 0;

        ExecutionResult cpp, native;

        /* Run the C++ implementation */
        {
            auto inp = *input;
            const auto ret = Eip4788::run(inp, storage);
            /* TODO enable this when Geth storage hashing is working */
            //const auto hash = inp.storage.Hash();
            //cpp = {.ret = ret, .hash = hash};
            cpp = {.ret = ret, .hash = 0};
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
    }

    return 0;
}
#elif defined(FUZZER_INVARIANTS)
class InvariantFuzzer {
    private:
        struct SetInvariants {
            /* set() must never revert */
            static void never_revert(
                    const Eip4788::ReturnValue& ret) {
                assert(ret.reverted == false);
            }

            /* set() must always return empty array */
            static void always_return_empty(
                    const Eip4788::ReturnValue& ret) {
                assert(ret.data.empty());
            }
        };

        static void set_invariants(
                const Eip4788::ReturnValue& ret) {
            SetInvariants::never_revert(ret);
            SetInvariants::always_return_empty(ret);
        }

        struct GetInvariants {
            /* get() must always revert if input is not 32 bytes */
            static void revert_if_not_32(
                    const Input& input,
                    const Eip4788::ReturnValue& ret) {
                if ( input.calldata.size() != 32 ) {
                    assert(ret.reverted == true);
                }
            }

            /* get() must always return 32 bytes if it didn't revert */
            static void return_32_if_not_revert(
                    const Eip4788::ReturnValue& ret) {
                if ( ret.reverted == false ) {
                    assert(ret.data.size() == 32);
                }
            }

            static void symmetry(
                    const Input& input,
                    const std::optional<Input>& prev_input,
                    const Eip4788::ReturnValue& ret) {
                /* If there was a function call before the current one */
                if ( !prev_input ) {
                    return;
                }

                /* and the current call to get() was well-formed */
                if ( input.calldata.size() != 32 ) {
                    return;
                }

                /* and the previous call was to set() */
                if ( prev_input->caller != constants::SYSTEM_ADDRESS ) {
                    return;
                }

                /* and the timestamp during set() equals the current calldata */
                if ( uint256(prev_input->timestamp) != util::load(input.calldata) ) {
                    return;
                }

                /* then get() shouldn't revert */
                assert(ret.reverted == false);

                /* and get() should return set()'s (trimmed, zero-padded) calldata */
                assert(ret.data == util::trim32(prev_input->calldata));

                /* Invariant in pseudocode:
                 *
                 * For any 64-bit TS:
                 *
                 * set(timestamp = TS);
                 * assert(get(calldata = TS) == TS);
                 */
            }

            static void integrity(
                    const Input& input,
                    const Eip4788::ReturnValue& ret,
                    const std::map<uint256, uint256>& timestamp_calldata_map) {
                if ( ret.reverted == true ) {
                    return;
                }

                const auto get_param = util::load(input.calldata);

                if ( timestamp_calldata_map.count(get_param) == 0 ) {
                    return;
                }

                const auto get_res = util::load(ret.data);
                assert(timestamp_calldata_map.at(get_param) == get_res);
            }
        };

        static void get_invariants(
                const Input& input,
                const std::optional<Input>& prev_input,
                const Eip4788::ReturnValue& ret,
                const std::map<uint256, uint256>& timestamp_calldata_map) {
            GetInvariants::revert_if_not_32(input, ret);
            GetInvariants::return_32_if_not_revert(ret);
            GetInvariants::symmetry(input, prev_input, ret);
            GetInvariants::integrity(input, ret, timestamp_calldata_map);
        }

    public:
        static void Run(const uint8_t* data, size_t size) {
            Storage storage;
            const uint8_t** data_ = &data;
            std::optional<Input> prev_input;
            std::map<uint256, uint256> timestamp_calldata_map;

            while ( true ) {
                const auto input = Input::Extract(data_, size, storage, false);
                if ( input == std::nullopt ) return;

                const auto inp = *input;
                const auto prev_storage_size = storage.MapRef().size();
                const auto ret = Eip4788::run(inp, storage);
                const auto cur_storage_size = storage.MapRef().size();

                if ( input->caller == constants::SYSTEM_ADDRESS ) {
                    timestamp_calldata_map[input->timestamp] =
                        util::load(util::trim32(input->calldata));

                    /* Each call to set() should add either 0 or 2 entries
                     * to the storage
                     */
                    assert(
                            cur_storage_size == prev_storage_size ||
                            cur_storage_size == prev_storage_size + 2);

                    set_invariants(ret);
                } else {
                    /* Each call to get() should leave the storage unchanged */
                    assert(cur_storage_size == prev_storage_size);

                    get_invariants(
                            inp,
                            prev_input,
                            ret,
                            timestamp_calldata_map);
                }

                prev_input = input;
            }
        }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    InvariantFuzzer::Run(data, size);
    return 0;
}
#else
int main(void)
{
    Native_Eip4788_Reset();
    Storage storage;

    ExecutionResult cpp, native;

    {
        Input input;
        input.caller = constants::SYSTEM_ADDRESS;
        input.calldata = {};
        input.timestamp = constants::ShanghaiTimestamp;
        input.gas = 0;

        Eip4788::run(input, storage);

        auto jsonStr = input.Json(storage).dump();
        const auto inp = util::ToGoSlice(
                jsonStr.data(),
                jsonStr.size());
        Native_Eip4788_Run(inp);
    }

    {
        Input input;
        input.caller = 1234;
        input.calldata = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x64, 0x37, 0x30, 0x57};
        input.timestamp = constants::ShanghaiTimestamp;
        input.gas = 0;

        const auto ret = Eip4788::run(input, storage);
        cpp = {.ret = ret, .hash = 0};

        auto jsonStr = input.Json(storage).dump();
        const auto inp = util::ToGoSlice(
                jsonStr.data(),
                jsonStr.size());
        Native_Eip4788_Run(inp);
        const auto res = util::load(Native_Eip4788_Result());
        native = ExecutionResult::FromJson(res);
    }

    assert(cpp == native);
}
#endif
