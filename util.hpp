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

    constexpr GoSlice ToGoSlice(void* data, const size_t size) {
        return GoSlice{
            .data = data,
            .len = static_cast<GoInt>(size),
            .cap = static_cast<GoInt>(size)};
    }

    std::string load(char* s) {
        const std::string ret(s);
        free(s);
        return ret;
    }
}

