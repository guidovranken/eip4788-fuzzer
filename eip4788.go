package main

import (
    "github.com/ethereum/go-ethereum/core/vm/runtime"
    st "github.com/ethereum/go-ethereum/core/state"
    "github.com/ethereum/go-ethereum/core/rawdb"
    "github.com/ethereum/go-ethereum/common"
    "github.com/ethereum/go-ethereum/params"
    "math/big"
    "encoding/json"
)

import "C"

type Input struct {
    Caller []byte
    CallData []byte
    Storage map[string]string
    Timestamp uint64
    Gas uint64
}

var eip4788_contract_code = []byte{
    0x60, 0x58, 0x80, 0x60, 0x09, 0x5f, 0x39, 0x5f, 0xf3, 0x33, 0x73, 0xff,
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
    state, _ = st.New(common.Hash{}, st.NewDatabase(rawdb.NewMemoryDatabase()), nil)
    state.SetCode(BEACON_ROOTS_ADDRESS, eip4788_contract_code)
    snapshot = state.Snapshot()
}

//export Native_Eip4788
func Native_Eip4788(data []byte) int {
    var input Input
    err := json.Unmarshal(data, &input)
    if err != nil {
        panic("Cannot load JSON")
    }

    state.RevertToSnapshot(snapshot)
    snapshot = state.Snapshot()

    caller := common.BytesToAddress(input.Caller)

    for key, value := range input.Storage {
        state.SetState(
            BEACON_ROOTS_ADDRESS,
            common.HexToHash(key),
            common.HexToHash(value))
    }

    _, _, err = runtime.Execute(
        eip4788_contract_code,
        input.CallData,
        &runtime.Config{
            Origin: caller,
            State: state,
            ChainConfig: params.MainnetChainConfig,
            GasLimit: input.Gas,
            BlockNumber: new(big.Int).SetUint64(12965000),
            Time: input.Timestamp,
        },
    )

    if err == nil {
        return 0
    } else {
        return 1
    }
}

func main() { }
