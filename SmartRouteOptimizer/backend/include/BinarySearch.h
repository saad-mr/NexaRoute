#pragma once

#include <cstddef>

template <typename Collection, typename Key, typename KeySelector>
int binarySearchIndex(
    const Collection& collection,
    const Key& target,
    KeySelector selectKey
) {
    int left = 0;
    int right = static_cast<int>(collection.size()) - 1;

    while (left <= right) {
        const int middle = left + (right - left) / 2;
        const auto& middleKey = selectKey(collection[static_cast<std::size_t>(middle)]);

        if (middleKey == target) return middle;
        if (middleKey < target) left = middle + 1;
        else right = middle - 1;
    }
    return -1;
}
