/* Mock EVM storage */
class Storage {
    private:
        std::map<uint256, uint256> storage;
        constexpr void bounds_check(const uint256& address) const {
            assert(
                    address <
                    constants::HISTORICAL_ROOTS_MODULUS * 2);
        }
    public:
        uint256 Get(
                const uint256& address,
                const bool check_bounds = false) const {
            if ( check_bounds ) {
                bounds_check(address);
            }

            if ( storage.count(address) ) {
                return storage.at(address);
            } else {
                return 0;
            }
        }

        void Set(
                const uint256& address,
                const uint256& v,
                const bool check_bounds = false) {
            if ( check_bounds ) {
                bounds_check(address);
            }
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

        static Storage FromJson(const nlohmann::json& j) {
            Storage ret;

            for (const auto& kv : j.items()) {
                ret.Set(
                    intx::from_string<uint256>(kv.key()),
                    intx::from_string<uint256>(kv.value()));
            }

            return ret;
        }
};

class Input {
    public:
        uint256 caller;
        Buffer calldata;
        uint64_t timestamp;
        uint64_t blocknumber;

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

            EXTRACT2(blocknumber, uint64_t);
            if ( ret.blocknumber < constants::LondonBlock ) {
                ret.blocknumber = constants::LondonBlock;
            }

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
            ret["blocknumber"] = blocknumber;

            return ret;
        }
};

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

struct ExecutionResult {
    ReturnValue ret;
    uint64_t hash;

    bool operator==(const ExecutionResult& rhs) const {
        return ret == rhs.ret && hash == rhs.hash;
    }

    static ExecutionResult FromJson(const nlohmann::json& j) {
        return ExecutionResult{
            .ret = ReturnValue{
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
