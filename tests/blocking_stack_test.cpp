#include "../blocking_stack.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <numeric>
#include <thread>
#include <vector>

int main() {
  const int P = std::thread::hardware_concurrency() * 2;
  const int K = 10000;
  const size_t MAX = static_cast<size_t>(P * K);

  BlockingStack<int> bs(MAX);
  std::vector<std::thread> producers;

  for (int p = 0; p < P; ++p) {
    producers.emplace_back([&bs, p] {
      for (int i = 0; i < K; ++i) {
        while (!bs.push(p * K + i))
          ;
      }
    });
  }

  for (auto &t : producers)
    t.join();

  std::vector<int> popped;
  while (auto val = bs.pop()) {
    popped.push_back(*val);
  }

  std::sort(popped.begin(), popped.end());

  std::vector<int> expected(P * K);
  std::iota(expected.begin(), expected.end(), 0);

  assert(popped == expected);
  std::printf("PASS: %d threads x %d values, %d total popped\n", P, K,
              (int)popped.size());
  return 0;
}
