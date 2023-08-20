# eip4788-fuzzer

EIP-4788 fuzzer for Ethereum.

TODO sort out licensing of dependencies when making this public.

## Assumptions

- Block timestamp is 64 bits. Any overflows or other bugs arising from a timestamp `>= 2**64` are not covered.
- Block timestamp `>= 1681338455` which is the Shanghai fork. Shanghai is needed for the `PUSH0` opcode which is present in the EIP-4788 contract. Block timestamps lower than that value are not covered.

## Invariants

- set() never reverts
- set() always returns empty data
- get() always reverts if input is not 32 bytes
- get() always returns 32 bytes if it didn't revert
- get() never reverts for a timestamp which was set() immediately prior
- If get() doesn't revert, it must always be for a timestamp which was previously set()
- set() always adds exactly 0 or 2 keys to the storage
- get() never adds to the storage
- In both set() and get(), `timestamp_idx` is always less than `root_idx`
- Calling `BEACON_ROOTS_ADDRESS` only executes opcodes that are in the eip-4788.md assembly listing
- set() and get() only access keys which are `< HISTORICAL_ROOTS_MODULUS * 2`
- The EIP-4788 contract always finishes (no infinite loops) (this is implicit by the fuzzer's timeout function and calling the contract with infinite gas)
