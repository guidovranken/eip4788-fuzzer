package main

import (
    "github.com/ethereum/go-ethereum/core/vm"
    "github.com/ethereum/go-ethereum/core/vm/runtime"
    st "github.com/ethereum/go-ethereum/core/state"
    "github.com/ethereum/go-ethereum/core/rawdb"
    "github.com/ethereum/go-ethereum/common"
    "github.com/ethereum/go-ethereum/params"
    "math/big"
    "encoding/json"
    "encoding/hex"
    //"fmt"
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

var BEACON_ROOTS_ADDRESS = common.HexToAddress("0x89e64Be8700cC37EB34f9209c96466DEEDc0d8a6")

var state* st.StateDB
var snapshot int

func init() {
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
    snapshot = state.Snapshot()
    //state.RevertToSnapshot(snapshot)
    //snapshot = state.Snapshot()
    //fmt.Println("snapshot", snapshot)
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
        },
    )

    if returndata == nil {
        returndata = []byte{}
    }

    var res ExecutionResult
    res.Ret.Reverted = err == vm.ErrExecutionReverted
    res.Ret.Data = hex.EncodeToString(returndata)

    result, err = json.Marshal(&res)
    if err != nil {
        panic("Cannot save JSON")
    }
}

func main() { }
