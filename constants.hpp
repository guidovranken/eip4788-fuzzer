namespace constants {
    using namespace intx;

    constexpr uint256 HISTORICAL_ROOTS_MODULUS = 98304;
    constexpr uint256 SYSTEM_ADDRESS = 0xfffffffffffffffffffffffffffffffffffffffe_u256;
    constexpr uint64_t ShanghaiTimestamp = 1681338455ULL;
    constexpr uint64_t FORK_TIMESTAMP = ShanghaiTimestamp;
    /* Shanghai is needed for the PUSH0 opcode */
    static_assert(FORK_TIMESTAMP >= ShanghaiTimestamp);
    constexpr uint64_t LondonBlock = 12965000;
    constexpr uint256 AddressMask = 0xffffffffffffffffffffffffffffffffffffffff_u256;
}
