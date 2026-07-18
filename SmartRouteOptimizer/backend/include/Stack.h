#pragma once

#include "DynamicArray.h"

#include <cstddef>

template <typename T>
class Stack {
public:
    void push(const T& value) {
        values_.pushBack(value);
    }

    bool pop(T& value) {
        return values_.popBack(value);
    }

    bool peek(T& value) const {
        if (values_.empty()) {
            return false;
        }
        value = values_.back();
        return true;
    }

    bool empty() const {
        return values_.empty();
    }

    std::size_t size() const {
        return values_.size();
    }

private:
    DynamicArray<T> values_;
};
