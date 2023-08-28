# eip4788-fuzzer

[EIP-4788](https://github.com/ethereum/EIPs/pull/7456) contract fuzzer for Ethereum Foundation.

TODO sort out licensing of dependencies when making this public.

## Fuzzers

### Differential

Runs two implementations of EIP-4788:

- C++ implementation in [eip4788.hpp]
- Bytecode implementation using the Geth API

The EIP-4788 implementations are called with the following parameters, which are pseudo-randomized by the fuzzer:

- Caller address
- Calldata (`0..65535` bytes large)
- Timestamp (`>= 1681338455`)
- Blocknumber (`>= 12965000`)
- Storage state

After running, the state of both implementations is compared across these metrics:

- Reverted or succeeded
- Return data
- Storage state

In the interest of efficiency, both storage states are not compared verbatim, but rather hashed (using [xxHash](https://github.com/Cyan4973/xxHash)) individually and then the hash values are compared.

If the post-run state differs across implementations for any randomized pre-run state, the fuzzer crashes, which indicates a bug.

### Differential with Python

Same as the regular differential fuzzer, but also runs and compares the output of the Python reference implementation in addition to the C++ and bytecode implementations.

### Invariants

Runs only the C++ EIP-4788 implementation and tests a variety of invariants at every iteration.

Because this doesn't require setting up an EVM at every fuzzer iteration, this is extremely performant.

Assuming the C++ and bytecode implementations are equivalent (which is what the differential fuzzer tests), then an invariant violation in the C++ implementation implies an invariant violation in the bytecode.

## Assumptions

- Block timestamp is 64 bits. Any overflows or other bugs arising from a timestamp `>= 2**64` are not covered.
- The EIP-4788 contract is only ever called with a block timestamp `>= 1681338455`, which is the Shanghai fork. Shanghai is needed for the `PUSH0` opcode used by the the EIP-4788 contract. Block timestamps lower than that value are not covered.
- Geth is free of bugs that affect the part of the Geth API used by the harness.
- Block number is >= 12965000 (London hard fork).

## Invariants

- `set()` never reverts
- `set()` always returns empty data
- `get()` always reverts if input is not 32 bytes
- `get()` always returns 32 bytes if it didn't revert
- `get()` never reverts for a timestamp which was `set()` immediately prior
- If `get()` doesn't revert, it must always be for a timestamp which was previously `set()`
- `set()` always adds exactly 0 or 2 keys to the storage
- `get()` never adds to the storage
- In both `set()` and `get()`, `timestamp_idx` is always `< root_idx`
- Neither addition in `set()` and `get()` overflows
- Calling `BEACON_ROOTS_ADDRESS` only executes opcodes that are in the eip-4788.md assembly listing
- Call depth is always 1 at every executed opcode (e.g. the EIP-4788 contract never calls or creates another contract)
- The EIP-4788 contract never modifies storage of an address other than its own
- `set()` and `get()` only access keys which are `< HISTORICAL_ROOTS_MODULUS * 2`
- The EIP-4788 contract always finishes (no infinite loops) (this is implicit by the fuzzer's timeout function and calling the contract with infinite gas)

## Building

Run `build.sh`. You must have `clang++` and `git`. Tested on Linux x64.
