#pragma once
#include <mutex>
#include <optional>

template <typename T> class BlockingStack {
  struct Node {
    T value;
    Node *next;
  };
  Node *head{nullptr};
  std::mutex mtx;

public:
  void push(T value) {
    Node *newNode = new Node{std::move(value), head};
    std::lock_guard lc(mtx);
    newNode->next = head;
    head = newNode;
  }

  std::optional<T> pop() {
    std::lock_guard lck(mtx);
    if (!head)
      return std::nullopt;
    Node *to_delete = head;
    head = head->next;
    T result = std::move(to_delete->value);
    delete to_delete;
    return result;
  }

  ~BlockingStack() {
    while (auto v = pop()) {}
  }
};
