#pragma once

#include <cstddef>

template <typename T>
class CircularQueue {
public:
    explicit CircularQueue(std::size_t capacity)
        : data_(new T[capacity == 0 ? 1 : capacity]),
          capacity_(capacity == 0 ? 1 : capacity), front_(0), rear_(0), count_(0) {}

    ~CircularQueue() {
        delete[] data_;
    }

    CircularQueue(const CircularQueue&) = delete;
    CircularQueue& operator=(const CircularQueue&) = delete;

    bool enqueue(const T& value) {
        if (full()) {
            return false;
        }
        data_[rear_] = value;
        rear_ = (rear_ + 1) % capacity_;
        ++count_;
        return true;
    }

    bool dequeue(T& value) {
        if (empty()) {
            return false;
        }
        value = data_[front_];
        front_ = (front_ + 1) % capacity_;
        --count_;
        return true;
    }

    bool empty() const { return count_ == 0; }
    bool full() const { return count_ == capacity_; }
    std::size_t size() const { return count_; }
    std::size_t capacity() const { return capacity_; }

private:
    T* data_;
    std::size_t capacity_;
    std::size_t front_;
    std::size_t rear_;
    std::size_t count_;
};
