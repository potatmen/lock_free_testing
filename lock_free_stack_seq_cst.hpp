#pragma once
#include <atomic>
#include <optional>

template <typename T> class LockFreeStack {
  struct Node {
    T value;
    Node *next;
  };
  std::atomic<Node *> head{nullptr};
  std::atomic<int> counter{0};
  std::atomic<Node *> delete_list{nullptr};

public:
  void push(T value) {
    Node *newNode = new Node{std::move(value), head.load()};
    while (!head.compare_exchange_weak(newNode->next, newNode))
      ;
  }

  std::optional<T> pop() {
    counter.fetch_add(1);
    Node *to_delete = head.load();
    while (!head.compare_exchange_weak(to_delete,
                                       to_delete ? to_delete->next : nullptr))
      ;
    std::optional<T> result = std::nullopt;
    if (to_delete) {
      result = to_delete->value;

      to_delete->next = delete_list.load();
      while (!delete_list.compare_exchange_weak(to_delete->next, to_delete))
        ;
    }
    if (counter.load() == 1) {
      Node *deletion_chain = delete_list.exchange(nullptr);
      while (deletion_chain) {
        Node *nxt = deletion_chain->next;
        delete deletion_chain;
        std::swap(nxt, deletion_chain);
      }
    }
    counter.fetch_sub(1);
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
