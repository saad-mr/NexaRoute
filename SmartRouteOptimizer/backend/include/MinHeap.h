#pragma once

#include "DynamicArray.h"

#include <cstddef>

template <typename T>
class MinHeap {
public:
    bool empty() const { return values_.empty(); }
    std::size_t size() const { return values_.size(); }

    void insert(const T& value) {
        values_.pushBack(value);
        bubbleUp(values_.size() - 1);
    }

    bool peek(T& value) const {
        if (empty()) return false;
        value = values_[0];
        return true;
    }

    bool removeMinimum(T& value) {
        if (empty()) return false;

        value = values_[0];
        T last;
        values_.popBack(last);
        if (!values_.empty()) {
            values_[0] = last;
            bubbleDown(0);
        }
        return true;
    }

private:
    DynamicArray<T> values_;

    static void swapValues(T& left, T& right) {
        T temporary = left;
        left = right;
        right = temporary;
    }

    void bubbleUp(std::size_t index) {
        while (index > 0) {
            const std::size_t parent = (index - 1) / 2;
            if (!(values_[index] < values_[parent])) {
                break;
            }
            swapValues(values_[index], values_[parent]);
            index = parent;
        }
    }

    void bubbleDown(std::size_t index) {
        while (true) {
            const std::size_t left = index * 2 + 1;
            const std::size_t right = index * 2 + 2;
            std::size_t smallest = index;

            if (left < values_.size() && values_[left] < values_[smallest]) smallest = left;
            if (right < values_.size() && values_[right] < values_[smallest]) smallest = right;
            if (smallest == index) break;

            swapValues(values_[index], values_[smallest]);
            index = smallest;
        }
    }
};
