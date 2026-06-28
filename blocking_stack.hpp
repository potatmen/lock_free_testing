#pragma once
#include <mutex>
#include <optional>
#include <stack>

template <typename T> class BlockingStack {
  std::stack<T> st;
  size_t maxSize;
  std::mutex mtx;

public:
  BlockingStack(const size_t &maxSize) : maxSize(maxSize) {}

  bool push(T value) {
    std::lock_guard lc(mtx);
    if (st.size() == maxSize)
      return false;
    st.push(std::move(value));
    return true;
  }

  std::optional<T> pop() {
    std::lock_guard lck(mtx);
    if (st.empty())
      return std::nullopt;
    T result = std::move(st.top());
    st.pop();
    return result;
  }

  ~BlockingStack() {
    while (auto element = pop())
      ;
  }
};
