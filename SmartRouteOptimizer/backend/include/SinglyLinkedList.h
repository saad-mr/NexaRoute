#pragma once

#include <cstddef>
#include <utility>

template <typename T>
class SinglyLinkedList {
private:
    struct Node {
        T value;
        Node* next;

        explicit Node(const T& item) : value(item), next(nullptr) {}
        explicit Node(T&& item) : value(std::move(item)), next(nullptr) {}
    };

public:
    SinglyLinkedList() : head_(nullptr), tail_(nullptr), size_(0) {}

    ~SinglyLinkedList() {
        clear();
    }

    SinglyLinkedList(const SinglyLinkedList&) = delete;
    SinglyLinkedList& operator=(const SinglyLinkedList&) = delete;

    T* insertBack(T&& value) {
        Node* node = new Node(std::move(value));
        if (tail_ == nullptr) {
            head_ = tail_ = node;
        } else {
            tail_->next = node;
            tail_ = node;
        }
        ++size_;
        return &node->value;
    }

    T* insertBack(const T& value) {
        Node* node = new Node(value);
        if (tail_ == nullptr) {
            head_ = tail_ = node;
        } else {
            tail_->next = node;
            tail_ = node;
        }
        ++size_;
        return &node->value;
    }

    template <typename Predicate>
    T* find(Predicate predicate) {
        for (Node* current = head_; current != nullptr; current = current->next) {
            if (predicate(current->value)) {
                return &current->value;
            }
        }
        return nullptr;
    }

    template <typename Predicate>
    const T* find(Predicate predicate) const {
        for (const Node* current = head_; current != nullptr; current = current->next) {
            if (predicate(current->value)) {
                return &current->value;
            }
        }
        return nullptr;
    }

    template <typename Visitor>
    void forEach(Visitor visitor) {
        for (Node* current = head_; current != nullptr; current = current->next) {
            visitor(current->value);
        }
    }

    template <typename Visitor>
    void forEach(Visitor visitor) const {
        for (const Node* current = head_; current != nullptr; current = current->next) {
            visitor(current->value);
        }
    }

    std::size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    void clear() {
        Node* current = head_;
        while (current != nullptr) {
            Node* removed = current;
            current = current->next;
            delete removed;
        }
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
    }

private:
    Node* head_;
    Node* tail_;
    std::size_t size_;
};
