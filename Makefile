CXX      := clang++
CXXFLAGS := -std=c++17 -O2 -Wall

BENCH_CFLAGS := -I/opt/homebrew/include
BENCH_LIBS   := -L/opt/homebrew/lib -lbenchmark

HDRS := lf_leaking_seq_cst.hpp lf_leaking_acq_rel.hpp lf_reclaim_seq_cst.hpp lf_reclaim_acq_rel.hpp lf_reclaim_ar_padded.hpp blocking_stack.hpp

.PHONY: all test bench run_test run_bench clean

all: test bench

test: tests/stack_test

bench: tests/bench

run_test: test
	tests/stack_test

run_bench: bench
	tests/bench

tests/stack_test: tests/stack_test.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $< -o $@

tests/bench: tests/bench.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(BENCH_CFLAGS) $< $(BENCH_LIBS) -o $@

clean:
	rm -f tests/stack_test tests/bench
