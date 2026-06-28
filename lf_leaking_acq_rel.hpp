#pragma once
#include <atomic>
#include <optional>

namespace leaking_ar {
template <typename T> class LockFreeStack {
  struct Node {
    T value;
    Node *next;
  };
  std::atomic<Node *> head{nullptr};

public:
  void push(T value) {
    Node *newNode = new Node{std::move(value), nullptr};
    while (!head.compare_exchange_weak(newNode->next, newNode,
                                       std::memory_order_release,
                                       std::memory_order_relaxed))
      ;
  }

  std::optional<T> pop() {
    Node *to_delete = head.load(std::memory_order_acquire);
    while (!head.compare_exchange_weak(
        to_delete, to_delete ? to_delete->next : nullptr,
        std::memory_order_acquire, std::memory_order_acquire))
      ;
    if (!to_delete)
      return std::nullopt;
    return to_delete->value;
  }
};
} // namespace leaking_ar
