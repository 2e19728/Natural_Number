# Quick build without CMake, using the same flags the CMake target exposes.
# For installing, packaging and CI use CMake (see README).
#
#	make test       build + run the self-checking suite
#	make bench      build the throughput benchmark
#	make example    build + run the example
#	make clean

CXX      ?= g++
CXXFLAGS ?= -O3 -march=native -std=c++20 -masm=intel
CPPFLAGS ?= -Iinclude

ASM   := src/mul_basecase.s src/mul_ntt.s src/mul_ntt_avx512.s
HDRS  := $(wildcard include/natural/*.h)
BUILD := build

all: $(BUILD)/natural_test

$(BUILD):
	mkdir -p $@

$(BUILD)/natural_test: tests/test_natural.cpp $(ASM) $(HDRS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) tests/test_natural.cpp $(ASM) -o $@

$(BUILD)/natural_bench: benchmarks/bench_natural.cpp $(ASM) $(HDRS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) benchmarks/bench_natural.cpp $(ASM) -o $@

$(BUILD)/natural_example: examples/square.cpp $(ASM) $(HDRS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) examples/square.cpp $(ASM) -o $@

test: $(BUILD)/natural_test
	$(BUILD)/natural_test

bench: $(BUILD)/natural_bench
	$(BUILD)/natural_bench

example: $(BUILD)/natural_example
	$(BUILD)/natural_example

clean:
	rm -rf $(BUILD)

.PHONY: all test bench example clean
