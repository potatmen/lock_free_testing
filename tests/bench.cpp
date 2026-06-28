#include "../blocking_stack.hpp"
#include "../lock_free_stack_seq_cst.hpp"
#include <benchmark/benchmark.h>
#include <thread>

constexpr size_t size = 1e5;

static void BM_LockFree_Single(benchmark::State &state) {
  LockFreeStack<int> s;
  for (auto _ : state) {
    s.push(42);
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_LockFree_Single);

static void BM_Blocking_Single(benchmark::State &state) {
  BlockingStack<int> s(size);
  for (auto _ : state) {
    s.push(42);
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_Blocking_Single);

static void BM_LockFree_Concurrent(benchmark::State &state) {
  static LockFreeStack<int> s;
  for (auto _ : state) {
    s.push(state.thread_index());
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_LockFree_Concurrent)
    ->ThreadRange(1, std::thread::hardware_concurrency());

static void BM_Blocking_Concurrent(benchmark::State &state) {
  static BlockingStack<int> s(size);
  for (auto _ : state) {
    s.push(state.thread_index());
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_Blocking_Concurrent)
    ->ThreadRange(1, std::thread::hardware_concurrency());

BENCHMARK_MAIN();
