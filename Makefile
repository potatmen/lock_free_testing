CXX      := clang++
CXXFLAGS := -std=c++17 -O2 -Wall
TSAN     := -fsanitize=thread

BENCH_FLAGS := $(shell pkg-config --cflags --libs benchmark)

LFS_HDR  := lock_free_stack_seq_cst.hpp
BLK_HDR  := blocking_stack.hpp

.PHONY: all test tsan bench clean

all: test bench

test: tests/lock_free_stack_test tests/blocking_stack_test

tsan: tests/lock_free_stack_test_tsan tests/blocking_stack_test_tsan

bench: tests/bench

tests/lock_free_stack_test: tests/lock_free_stack_test.cpp $(LFS_HDR)
	$(CXX) $(CXXFLAGS) $< -o $@

tests/lock_free_stack_test_tsan: tests/lock_free_stack_test.cpp $(LFS_HDR)
	$(CXX) $(CXXFLAGS) $(TSAN) $< -o $@

tests/blocking_stack_test: tests/blocking_stack_test.cpp $(BLK_HDR)
	$(CXX) $(CXXFLAGS) $< -o $@

tests/blocking_stack_test_tsan: tests/blocking_stack_test.cpp $(BLK_HDR)
	$(CXX) $(CXXFLAGS) $(TSAN) $< -o $@

tests/bench: tests/bench.cpp $(LFS_HDR) $(BLK_HDR)
	$(CXX) $(CXXFLAGS) $< $(BENCH_FLAGS) -o $@

clean:
	rm -f tests/lock_free_stack_test tests/lock_free_stack_test_tsan \
	      tests/blocking_stack_test tests/blocking_stack_test_tsan \
	      tests/bench
