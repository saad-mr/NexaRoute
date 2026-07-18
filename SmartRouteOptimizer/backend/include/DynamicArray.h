#pragma once

#include <cstddef>
#include <utility>

template <typename T>
class DynamicArray {
public:
    explicit DynamicArray(std::size_t initialCapacity = 8)
        : data_(nullptr), size_(0), capacity_(initialCapacity == 0 ? 1 : initialCapacity) {
        data_ = new T[capacity_];
    }

    ~DynamicArray() {
        delete[] data_;
    }

    DynamicArray(const DynamicArray& other)
        : data_(new T[other.capacity_]), size_(other.size_), capacity_(other.capacity_) {
        for (std::size_t index = 0; index < size_; ++index) {
            data_[index] = other.data_[index];
        }
    }

    DynamicArray(DynamicArray&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    DynamicArray& operator=(const DynamicArray& other) {
        if (this == &other) {
            return *this;
        }

        T* replacement = new T[other.capacity_];
        for (std::size_t index = 0; index < other.size_; ++index) {
            replacement[index] = other.data_[index];
        }

        delete[] data_;
        data_ = replacement;
        size_ = other.size_;
        capacity_ = other.capacity_;
        return *this;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        delete[] data_;
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        return *this;
    }

    void pushBack(const T& value) {
        ensureCapacity(size_ + 1);
        data_[size_++] = value;
    }

    void pushBack(T&& value) {
        ensureCapacity(size_ + 1);
        data_[size_++] = std::move(value);
    }

    bool popBack(T& removedValue) {
        if (empty()) {
            return false;
        }
        removedValue = data_[--size_];
        return true;
    }

    void clear() {
        size_ = 0;
    }

    void reserve(std::size_t requestedCapacity) {
        if (requestedCapacity > capacity_) {
            resizeStorage(requestedCapacity);
        }
    }

    std::size_t size() const {
        return size_;
    }

    std::size_t capacity() const {
        return capacity_;
    }

    bool empty() const {
        return size_ == 0;
    }

    T& operator[](std::size_t index) {
        return data_[index];
    }

    const T& operator[](std::size_t index) const {
        return data_[index];
    }

    T& back() {
        return data_[size_ - 1];
    }

    const T& back() const {
        return data_[size_ - 1];
    }

private:
    T* data_;
    std::size_t size_;
    std::size_t capacity_;

    void ensureCapacity(std::size_t requiredCapacity) {
        if (requiredCapacity <= capacity_) {
            return;
        }
        const std::size_t grownCapacity = capacity_ < 2 ? 2 : capacity_ * 2;
        resizeStorage(grownCapacity > requiredCapacity ? grownCapacity : requiredCapacity);
    }

    void resizeStorage(std::size_t newCapacity) {
        T* replacement = new T[newCapacity];
        for (std::size_t index = 0; index < size_; ++index) {
            replacement[index] = std::move(data_[index]);
        }
        delete[] data_;
        data_ = replacement;
        capacity_ = newCapacity;
    }
};
