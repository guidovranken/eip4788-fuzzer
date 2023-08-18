all: fuzzer-differential fuzzer-invariants test

eip4788.a: eip4788.go
	go build -o eip4788.a -buildmode=c-archive eip4788.go
xxhash.o : xxhash.c xxhash.h
	clang -c -Ofast xxhash.c -o xxhash.o
fuzzer-differential: harness.cpp eip4788.a xxhash.o
	clang++ -DFUZZER_DIFFERENTIAL -g -Wall -Wextra -Werror -std=c++20 -fsanitize=fuzzer -I xxhash/ -I intx/include/ harness.cpp eip4788.a xxhash.o -o fuzzer-differential
fuzzer-invariants: harness.cpp xxhash.o
	clang++ -DFUZZER_INVARIANTS -Ofast -g -Wall -Wextra -Werror -std=c++20 -fsanitize=fuzzer -I xxhash/ -I intx/include/ harness.cpp xxhash.o -o fuzzer-invariants
test: harness.cpp eip4788.a xxhash.o
	clang++ -g -Wall -Wextra -std=c++20 -I xxhash/ -I intx/include/ harness.cpp eip4788.a xxhash.o -lpthread -o test
