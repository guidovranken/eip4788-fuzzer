all: fuzzer-differential fuzzer-invariants

eip4788.a: eip4788.go tracer.go
	go build -o eip4788.a -buildmode=c-archive eip4788.go tracer.go
xxhash.o : xxhash.c xxhash.h
	clang -c -Ofast xxhash.c -o xxhash.o
fuzzer-differential: harness.cpp constants.hpp eip4788.hpp harness-differential.hpp harness-invariants.hpp invariants.hpp json.hpp structs.hpp util.hpp eip4788.a xxhash.o
	clang++ -DFUZZER_DIFFERENTIAL -Ofast -g -Wall -Wextra -Werror -std=c++20 -fsanitize=fuzzer -I xxhash/ -I intx/include/ harness.cpp eip4788.a xxhash.o -o fuzzer-differential
fuzzer-invariants: harness.cpp constants.hpp eip4788.hpp harness-differential.hpp harness-invariants.hpp invariants.hpp json.hpp structs.hpp util.hpp xxhash.o
	clang++ -DFUZZER_INVARIANTS -Ofast -g -Wall -Wextra -Werror -std=c++20 -fsanitize=fuzzer -I xxhash/ -I intx/include/ harness.cpp xxhash.o -o fuzzer-invariants
