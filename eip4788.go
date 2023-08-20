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
    Gas uint64
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

//export Native_Eip4788_Result
func Native_Eip4788_Result() *C.char {
    return C.CString(string(result))
}

//export Native_Eip4788_Reset
func Native_Eip4788_Reset() {
    state, _ = st.New(common.Hash{}, st.NewDatabase(rawdb.NewMemoryDatabase()), nil)
    state.SetCode(BEACON_ROOTS_ADDRESS, eip4788_contract_code)
}

var opcode_whitelist []vm.OpCode

type Tracer struct {}

func (l *Tracer) CaptureStart(
    env *vm.EVM,
    from common.Address,
    to common.Address,
    create bool,
    input []byte,
    gas uint64,
    value *big.Int) {}
func (l *Tracer) CaptureState(pc uint64,
    op vm.OpCode,
    gas,
    cost uint64,
    scope *vm.ScopeContext,
    rData []byte,
    depth int,
    err error) {
    if slices.Contains(opcode_whitelist, op) == false {
        panic("Executed opcode that is not in EIP-4788")
    }
}
func (l *Tracer) CaptureFault(pc uint64,
    op vm.OpCode,
    gas,
    cost uint64,
    scope *vm.ScopeContext,
    depth int,
    err error) {}
func (l *Tracer) CaptureEnd(output []byte,
    gasUsed uint64,
    err error) {}
func (l *Tracer) CaptureEnter(
    typ vm.OpCode,
    from common.Address,
    to common.Address,
    input []byte,
    gas uint64,
    value *big.Int) {}
func (l *Tracer) CaptureExit(output []byte,
    gasUsed uint64,
    err error) {}
func (l *Tracer) CaptureTxStart(gasLimit uint64) {}
func (l *Tracer) CaptureTxEnd(restGas uint64) {}

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
            BlockNumber: new(big.Int).SetUint64(12965000),
            Time: input.Timestamp,
            EVMConfig: vm.Config{
                Tracer: &Tracer{},
            },
        },
    )

    if returndata == nil {
        returndata = []byte{}
    }

    /* Storage must be hashed in order (sorted by key) */
    var storageKeys []common.Hash
    /* Extract the storage keys */
    for key := range state.GetStateObjects()[BEACON_ROOTS_ADDRESS].GetDirtyStorage() {
        storageKeys = append(storageKeys, key)
    }
    /* Sort the storage keys */
    sort.Slice(storageKeys, func(i, j int) bool {
        return common.Hash.Cmp(storageKeys[i], storageKeys[j]) < 0
    })

    h := xxhash.New()
    /* Hash the storage keys and values */
    for _, k := range storageKeys {
        v := state.GetState(BEACON_ROOTS_ADDRESS, k)
        h.Write(k.Bytes())
        h.Write(v.Bytes())
    }

    var res ExecutionResult
    res.Ret.Reverted = err == vm.ErrExecutionReverted
    res.Ret.Data = hex.EncodeToString(returndata)
    res.Hash = h.Sum64()

    result, err = json.Marshal(&res)
    if err != nil {
        panic("Cannot save JSON")
    }
}

func main() { }
