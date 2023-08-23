package main

import (
    "github.com/ethereum/go-ethereum/core/vm"
    "github.com/ethereum/go-ethereum/core/vm/runtime"
    st "github.com/ethereum/go-ethereum/core/state"
    "github.com/ethereum/go-ethereum/core/rawdb"
    "github.com/ethereum/go-ethereum/common"
    "github.com/ethereum/go-ethereum/params"
    "github.com/cespare/xxhash/v2"
    "math/big"
    "encoding/json"
    "encoding/hex"
    "sort"
    "golang.org/x/exp/slices"
)

import "C"

type Input struct {
    Caller []byte
    CallData []byte
    Storage map[string]string
    Timestamp uint64
    BlockNumber uint64
}

type ReturnValue struct {
    Reverted bool
    Data string
}

type ExecutionResult struct {
    Ret ReturnValue
    Hash uint64
}

var eip4788_contract_code = []byte{
    /*0x60, 0x58, 0x80, 0x60, 0x09, 0x5f, 0x39, 0x5f, 0xf3, */0x33, 0x73, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x14, 0x60, 0x44, 0x57, 0x60,
    0x20, 0x36, 0x14, 0x60, 0x24, 0x57, 0x5f, 0x5f, 0xfd, 0x5b, 0x62, 0x01,
    0x80, 0x00, 0x5f, 0x35, 0x06, 0x80, 0x54, 0x5f, 0x35, 0x14, 0x60, 0x37,
    0x57, 0x5f, 0x5f, 0xfd, 0x5b, 0x62, 0x01, 0x80, 0x00, 0x01, 0x54, 0x5f,
    0x52, 0x60, 0x20, 0x5f, 0xf3, 0x5b, 0x62, 0x01, 0x80, 0x00, 0x42, 0x06,
    0x42, 0x81, 0x55, 0x5f, 0x35, 0x90, 0x62, 0x01, 0x80, 0x00, 0x01, 0x55,
    0x00}

var BEACON_ROOTS_ADDRESS = common.HexToAddress("0xBeAC00541d49391ED88ABF392bfC1F4dEa8c4143")

var state* st.StateDB

func init() {
    // Vim regex to convert assembly listing in eip-4788.md
    // to opcode whitelist:
    //
    // s#^\s*\(\w\+\)\(.*\)$#/* \1\2 */ vm.\U\1,#g
    whitelist := []vm.OpCode{
        /* push1 0x58 */ vm.PUSH1,
        /* dup1 */ vm.DUP1,
        /* push1 0x09 */ vm.PUSH1,
        /* push0 */ vm.PUSH0,
        /* codecopy */ vm.CODECOPY,
        /* push0 */ vm.PUSH0,
        /* return */ vm.RETURN,

        /* caller */ vm.CALLER,
        /* push20 0xfffffffffffffffffffffffffffffffffffffffe */ vm.PUSH20,
        /* eq */ vm.EQ,
        /* push1 0x44 */ vm.PUSH1,
        /* jumpi */ vm.JUMPI,

        /* push1 0x20 */ vm.PUSH1,
        /* calldatasize */ vm.CALLDATASIZE,
        /* eq */ vm.EQ,
        /* push1 0x24 */ vm.PUSH1,
        /* jumpi */ vm.JUMPI,

        /* push0 */ vm.PUSH0,
        /* push0 */ vm.PUSH0,
        /* revert */ vm.REVERT,

        /* jumpdest */ vm.JUMPDEST,
        /* push3 0x018000 */ vm.PUSH3,
        /* push0 */ vm.PUSH0,
        /* calldataload */ vm.CALLDATALOAD,
        /* mod */ vm.MOD,
        /* dup1 */ vm.DUP1,
        /* sload */ vm.SLOAD,
        /* push0 */ vm.PUSH0,
        /* calldataload */ vm.CALLDATALOAD,
        /* eq */ vm.EQ,
        /* push1 0x37 */ vm.PUSH1,
        /* jumpi */ vm.JUMPI,

        /* push0 */ vm.PUSH0,
        /* push0 */ vm.PUSH0,
        /* revert */ vm.REVERT,

        /* jumpdest */ vm.JUMPDEST,
        /* push3 0x018000 */ vm.PUSH3,
        /* add */ vm.ADD,
        /* sload */ vm.SLOAD,
        /* push0 */ vm.PUSH0,
        /* mstore */ vm.MSTORE,
        /* push1 0x20 */ vm.PUSH1,
        /* push0 */ vm.PUSH0,
        /* return */ vm.RETURN,

        /* jumpdest */ vm.JUMPDEST,
        /* push3 0x018000 */ vm.PUSH3,
        /* timestamp */ vm.TIMESTAMP,
        /* mod */ vm.MOD,
        /* timestamp */ vm.TIMESTAMP,
        /* dup2 */ vm.DUP2,
        /* sstore */ vm.SSTORE,
        /* push0 */ vm.PUSH0,
        /* calldataload */ vm.CALLDATALOAD,
        /* swap1 */ vm.SWAP1,
        /* push3 0x018000 */ vm.PUSH3,
        /* add */ vm.ADD,
        /* sstore */ vm.SSTORE,
        /* stop */ vm.STOP,
    }

    found := map[vm.OpCode]bool{}
    whitelist_uniq := []vm.OpCode{}
    for _, v := range whitelist {
        if !found[v] {
            found[v] = true
            whitelist_uniq = append(whitelist_uniq, v)
        }
    }

    opcode_whitelist = whitelist_uniq
}

var result []byte
var callers []common.Address

//export Native_Eip4788_Result
func Native_Eip4788_Result() *C.char {
    return C.CString(string(result))
}

//export Native_Eip4788_Reset
func Native_Eip4788_Reset() {
    state, _ = st.New(common.Hash{}, st.NewDatabase(rawdb.NewMemoryDatabase()), nil)
    state.SetCode(BEACON_ROOTS_ADDRESS, eip4788_contract_code)
    callers = []common.Address{}
}

var opcode_whitelist []vm.OpCode

func sortedStorageKeys(state *st.StateDB) []common.Hash {
    var storageKeys []common.Hash

    /* Retrieve the storage keys */
    for key := range state.GetStateObjects()[BEACON_ROOTS_ADDRESS].GetDirtyStorage() {
        storageKeys = append(storageKeys, key)
    }

    /* Sort the storage keys */
    sort.Slice(storageKeys, func(i, j int) bool {
        return common.Hash.Cmp(storageKeys[i], storageKeys[j]) < 0
    })

    return storageKeys
}

func hashStorage(state *st.StateDB) uint64 {
    h := xxhash.New()

    /* Hash the storage keys and values */
    for _, k := range sortedStorageKeys(state) {
        v := state.GetState(BEACON_ROOTS_ADDRESS, k)
        h.Write(k.Bytes())
        h.Write(v.Bytes())
    }

    return h.Sum64()
}

func getStorageAddresses(state *st.StateDB) []common.Address {
    var storageAddresses []common.Address

    for address := range state.GetStateObjects() {
        storageAddresses = append(storageAddresses, address)
    }

    return storageAddresses
}

func storageInvariants(storageAddresses, callers []common.Address) {
    /* Assert that the EIP-4788 only changes its own storage */

    /* Iterate through all the addresses whose storage is set */
    for _, address := range storageAddresses {
        /* The storage of BEACON_ROOTS_ADDRESS is hashed elsewhere and
         * compared to the hash of the alternative implementation; any
         * discrepancies will be detected there.
         */
        if common.Address.Cmp(address, BEACON_ROOTS_ADDRESS) == 0 {
            continue
        }

        isCaller := slices.Contains(callers, address)
        if isCaller {
            /* It is expected that runtime.Call() will create a storage object
             * for caller. However, it must be empty. If it is not empty,
             * this implies that the EIP-4788 invocation somehow altered
             * a storage object that is not its own.
             */
            if len(state.GetStateObjects()[address].GetDirtyStorage()) != 0 {
                panic("Caller storage should be empty")
            }
        } else {
            /* Any modifications to storage objects that do not belong to
             * BEACON_ROOTS_ADDRESS or any of the callers implies a bug.
             */
            panic("Contract altered storage at address other than itself")
        }
    }
}

//export Native_Eip4788_Run
func Native_Eip4788_Run(data []byte) {
    /* Reset result */
    result = []byte{}

    var input Input
    err := json.Unmarshal(data, &input)
    if err != nil {
        panic("Cannot load JSON")
    }

    caller := common.BytesToAddress(input.Caller)
    if slices.Contains(callers, caller) == false {
        callers = append(callers, caller)
    }

    for key, value := range input.Storage {
        state.SetState(
            BEACON_ROOTS_ADDRESS,
            common.HexToHash(key),
            common.HexToHash(value))
    }

    returndata, _, err := runtime.Call(
        BEACON_ROOTS_ADDRESS,
        input.CallData,
        &runtime.Config{
            Origin: caller,
            State: state,
            ChainConfig: params.MainnetChainConfig,
            //GasLimit: input.Gas,
            GasLimit: 0,
            BlockNumber: new(big.Int).SetUint64(input.BlockNumber),
            Time: input.Timestamp,
            EVMConfig: vm.Config{
                Tracer: &Tracer{},
            },
        },
    )

    storageInvariants(getStorageAddresses(state), callers)

    if returndata == nil {
        returndata = []byte{}
    }

    result, err = json.Marshal(&ExecutionResult{
        Ret : ReturnValue {
            Reverted: err == vm.ErrExecutionReverted,
            Data: hex.EncodeToString(returndata),
        },
        Hash : hashStorage(state),
    })
    if err != nil {
        panic("Cannot save JSON")
    }
}
func main() { }
