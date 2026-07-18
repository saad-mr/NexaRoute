#pragma once

#include <cstddef>

template <typename T>
class Queue {
public:
    explicit Queue(std::size_t initialCapacity = 8)
        : data_(nullptr), capacity_(initialCapacity == 0 ? 1 : initialCapacity),
          frontIndex_(0), rearIndex_(0), count_(0) {
        data_ = new T[capacity_];
    }

    ~Queue() {
        delete[] data_;
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    void enqueue(const T& value) {
        if (count_ == capacity_) {
            grow();
        }
        data_[rearIndex_] = value;
        rearIndex_ = (rearIndex_ + 1) % capacity_;
        ++count_;
    }

    bool dequeue(T& value) {
        if (empty()) {
            return false;
        }
        value = data_[frontIndex_];
        frontIndex_ = (frontIndex_ + 1) % capacity_;
        --count_;
        return true;
    }

    bool peek(T& value) const {
        if (empty()) {
            return false;
        }
        value = data_[frontIndex_];
        return true;
    }

    bool empty() const {
        return count_ == 0;
    }

    std::size_t size() const {
        return count_;
    }

private:
    T* data_;
    std::size_t capacity_;
    std::size_t frontIndex_;
    std::size_t rearIndex_;
    std::size_t count_;

    void grow() {
        const std::size_t newCapacity = capacity_ * 2;
        T* replacement = new T[newCapacity];
        for (std::size_t index = 0; index < count_; ++index) {
            replacement[index] = data_[(frontIndex_ + index) % capacity_];
        }

        delete[] data_;
        data_ = replacement;
        capacity_ = newCapacity;
        frontIndex_ = 0;
        rearIndex_ = count_;
    }
};
