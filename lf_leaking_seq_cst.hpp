#pragma once
#include <atomic>
#include <optional>

namespace leaking_sc {
template <typename T> class LockFreeStack {
  struct Node {
    T value;
    Node *next;
  };
  std::atomic<Node *> head{nullptr};

public:
  void push(T value) {
    Node *newNode = new Node{std::move(value), head.load()};
    while (!head.compare_exchange_weak(newNode->next, newNode))
      ;
  }

  std::optional<T> pop() {
    Node *to_delete = head.load();
    while (!head.compare_exchange_weak(to_delete,
                                       to_delete ? to_delete->next : nullptr))
      ;
    if (!to_delete)
      return std::nullopt;
    return to_delete->value;
  }
};
} // namespace leaking
