all: fuzzer

eip4788.a: eip4788.go
	go build -o eip4788.a -buildmode=c-archive eip4788.go
xxhash.o : xxhash.c xxhash.h
	clang -c -Ofast xxhash.c -o xxhash.o
fuzzer: harness.cpp eip4788.a xxhash.o
	clang++ -Ofast -g -Wall -Wextra -Werror -std=c++20 -fsanitize=fuzzer -I xxhash/ -I intx/include/ harness.cpp eip4788.a xxhash.o -o fuzzer
