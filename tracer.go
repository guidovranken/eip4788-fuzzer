package main

import (
    "github.com/ethereum/go-ethereum/core/vm"
    "github.com/ethereum/go-ethereum/common"
    "math/big"
    "golang.org/x/exp/slices"
)

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
    if depth != 1 {
        panic("Call depth should always be 1")
    }

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
