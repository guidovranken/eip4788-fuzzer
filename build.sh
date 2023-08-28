#!/bin/bash

set -eu

# Get Go
wget 'https://go.dev/dl/go1.21.0.linux-amd64.tar.gz'
tar zxf go1.21.0.linux-amd64.tar.gz
cd go/src/
export GOROOT=$(realpath ../)
export GOPATH=$GOROOT/packages
mkdir $GOPATH
export PATH=$GOROOT/bin:$PATH
export PATH=$GOROOT/packages/bin:$PATH
cd ../../

# Get Geth
git clone https://github.com/ethereum/go-ethereum
cd go-ethereum/
git checkout 386cba15b5ee56908e3b33f7ee52a4c8486d5d8f

git clone --depth 1 git@github.com:guidovranken/eip4788-fuzzer.git
# Patch Geth
git apply eip4788-fuzzer/geth.patch
cd eip4788-fuzzer/

# Install CPython
git clone --depth 1 --branch v3.11.5 https://github.com/python/cpython.git
mkdir cpython-install/
cd cpython/
./configure --prefix=$(realpath ../cpython-install)
make -j$(nproc)
make install
cd ../

make
