#pragma once

#include <cstddef>
#include <utility>

template <typename T>
class DoublyLinkedList {
private:
    struct Node {
        T value;
        Node* previous;
        Node* next;

        explicit Node(const T& item)
            : value(item), previous(nullptr), next(nullptr) {}
    };

public:
    DoublyLinkedList() : head_(nullptr), tail_(nullptr), size_(0) {}

    ~DoublyLinkedList() {
        clear();
    }

    DoublyLinkedList(const DoublyLinkedList&) = delete;
    DoublyLinkedList& operator=(const DoublyLinkedList&) = delete;

    DoublyLinkedList(DoublyLinkedList&& other) noexcept
        : head_(other.head_), tail_(other.tail_), size_(other.size_) {
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.size_ = 0;
    }

    DoublyLinkedList& operator=(DoublyLinkedList&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        clear();
        head_ = other.head_;
        tail_ = other.tail_;
        size_ = other.size_;
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.size_ = 0;
        return *this;
    }

    void pushBack(const T& value) {
        Node* node = new Node(value);
        node->previous = tail_;
        if (tail_ == nullptr) {
            head_ = tail_ = node;
        } else {
            tail_->next = node;
            tail_ = node;
        }
        ++size_;
    }

    template <typename Visitor>
    void forEachForward(Visitor visitor) const {
        for (const Node* current = head_; current != nullptr; current = current->next) {
            visitor(current->value);
        }
    }

    template <typename Visitor>
    void forEachBackward(Visitor visitor) const {
        for (const Node* current = tail_; current != nullptr; current = current->previous) {
            visitor(current->value);
        }
    }

    const T* back() const {
        return tail_ == nullptr ? nullptr : &tail_->value;
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
