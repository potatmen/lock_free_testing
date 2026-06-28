#include "../blocking_stack.hpp"
#include "../lf_leaking_seq_cst.hpp"
#include "../lf_leaking_acq_rel.hpp"
#include "../lf_reclaim_seq_cst.hpp"
#include "../lf_reclaim_acq_rel.hpp"
#include "../lf_reclaim_ar_padded.hpp"
#include <benchmark/benchmark.h>
#include <thread>

static void BM_Leaking_SC(benchmark::State &state) {
  static leaking_sc::LockFreeStack<int> s;
  for (auto _ : state) {
    s.push(state.thread_index());
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_Leaking_SC)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16);

static void BM_Leaking_AR(benchmark::State &state) {
  static leaking_ar::LockFreeStack<int> s;
  for (auto _ : state) {
    s.push(state.thread_index());
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_Leaking_AR)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16);

static void BM_Reclaim_SC(benchmark::State &state) {
  static reclaim_sc::LockFreeStack<int> s;
  for (auto _ : state) {
    s.push(state.thread_index());
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_Reclaim_SC)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16);

static void BM_Reclaim_AR(benchmark::State &state) {
  static reclaim_ar::LockFreeStack<int> s;
  for (auto _ : state) {
    s.push(state.thread_index());
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_Reclaim_AR)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16);

static void BM_Reclaim_AR_Padded(benchmark::State &state) {
  static reclaim_ar_padded::LockFreeStack<int> s;
  for (auto _ : state) {
    s.push(state.thread_index());
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_Reclaim_AR_Padded)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16);

static void BM_Blocking(benchmark::State &state) {
  static BlockingStack<int> s;
  for (auto _ : state) {
    s.push(state.thread_index());
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_Blocking)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16);

BENCHMARK_MAIN();
