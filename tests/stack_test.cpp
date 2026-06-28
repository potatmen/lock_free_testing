#include "../blocking_stack.hpp"
#include "../lf_leaking_seq_cst.hpp"
#include "../lf_leaking_acq_rel.hpp"
#include "../lf_reclaim_seq_cst.hpp"
#include "../lf_reclaim_acq_rel.hpp"
#include "../lf_reclaim_ar_padded.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <numeric>
#include <thread>
#include <vector>

template <typename Stack> void run_test(const char *name) {
  const int P = std::thread::hardware_concurrency();
  const int K = 10000;
  const int TOTAL = P * K;

  Stack s;
  std::vector<int> popped(TOTAL);
  std::atomic<int> pop_idx{0};
  std::atomic<int> remaining{TOTAL};

  std::vector<std::thread> threads;

  for (int p = 0; p < P; ++p)
    threads.emplace_back([&, p] {
      for (int i = 0; i < K; ++i)
        s.push(p * K + i);
    });

  for (int c = 0; c < P; ++c)
    threads.emplace_back([&] {
      while (remaining.load() > 0)
        if (auto val = s.pop()) {
          popped[pop_idx.fetch_add(1)] = val.value();
          remaining.fetch_sub(1);
        }
    });

  for (auto &t : threads)
    t.join();

  std::sort(popped.begin(), popped.end());

  std::vector<int> expected(TOTAL);
  std::iota(expected.begin(), expected.end(), 0);

  assert(popped == expected);
  std::printf("PASS [%s]: %d threads x %d values\n", name, P, TOTAL);
}

int main() {
  run_test<leaking_sc::LockFreeStack<int>>("leaking_sc");
  run_test<leaking_ar::LockFreeStack<int>>("leaking_ar");
  run_test<reclaim_sc::LockFreeStack<int>>("reclaim_sc");
  run_test<reclaim_ar::LockFreeStack<int>>("reclaim_ar");
  run_test<reclaim_ar_padded::LockFreeStack<int>>("reclaim_ar_padded");
  run_test<BlockingStack<int>>("blocking");
}
