#pragma once
#include <atomic>
#include <optional>

namespace reclaim_ar_padded {
template <typename T> class LockFreeStack {
  struct Node {
    T value;
    Node *next;
  };
  alignas(64) std::atomic<Node *> head{nullptr};
  std::atomic<int> counter{0};
  std::atomic<Node *> delete_list{nullptr};

public:
  void push(T value) {
    Node *newNode = new Node{std::move(value), head.load(std::memory_order_acquire)};
    while (!head.compare_exchange_weak(newNode->next, newNode, std::memory_order_release, std::memory_order_relaxed))
      ;
  }

  std::optional<T> pop() {
    counter.fetch_add(1, std::memory_order_acq_rel);
    Node *to_delete = head.load(std::memory_order_acquire);
    while (!head.compare_exchange_weak(to_delete,
                                       to_delete ? to_delete->next : nullptr, std::memory_order_acquire, std::memory_order_acquire))
      ;
    std::optional<T> result = std::nullopt;
    if (to_delete) {
      result = to_delete->value;
      to_delete->next = delete_list.load(std::memory_order_relaxed);
      while (!delete_list.compare_exchange_weak(to_delete->next, to_delete, std::memory_order_release, std::memory_order_acquire))
        ;
    }
    if (counter.load(std::memory_order_acquire) == 1) {
      Node *deletion_chain = delete_list.exchange(nullptr, std::memory_order_acquire);
      while (deletion_chain) {
        Node *nxt = deletion_chain->next;
        delete deletion_chain;
        std::swap(nxt, deletion_chain);
      }
    }
    counter.fetch_sub(1, std::memory_order_release);
    return result;
  }

  ~LockFreeStack() {
    while (auto v = pop()) {}
    Node *deletion_chain = delete_list.exchange(nullptr);
    while (deletion_chain) {
      Node *nxt = deletion_chain->next;
      delete deletion_chain;
      std::swap(nxt, deletion_chain);
    }
  }
};
} // namespace reclaim_ar_padded
