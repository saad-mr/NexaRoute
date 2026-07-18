#pragma once

#include <cstddef>
#include <string>

template <typename Value>
class HashTable {
private:
    struct Node {
        std::string key;
        Value value;
        Node* next;

        Node(const std::string& itemKey, const Value& itemValue, Node* nextNode)
            : key(itemKey), value(itemValue), next(nextNode) {}
    };

public:
    explicit HashTable(std::size_t bucketCount = 101)
        : buckets_(new Node*[bucketCount]), bucketCount_(bucketCount), size_(0) {
        for (std::size_t index = 0; index < bucketCount_; ++index) {
            buckets_[index] = nullptr;
        }
    }

    ~HashTable() {
        clear();
        delete[] buckets_;
    }

    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    bool insert(const std::string& key, const Value& value) {
        const std::size_t bucket = hash(key);
        for (Node* current = buckets_[bucket]; current != nullptr; current = current->next) {
            if (current->key == key) {
                return false;
            }
        }
        buckets_[bucket] = new Node(key, value, buckets_[bucket]);
        ++size_;
        return true;
    }

    Value* find(const std::string& key) {
        const std::size_t bucket = hash(key);
        for (Node* current = buckets_[bucket]; current != nullptr; current = current->next) {
            if (current->key == key) {
                return &current->value;
            }
        }
        return nullptr;
    }

    const Value* find(const std::string& key) const {
        const std::size_t bucket = hash(key);
        for (const Node* current = buckets_[bucket]; current != nullptr; current = current->next) {
            if (current->key == key) {
                return &current->value;
            }
        }
        return nullptr;
    }

    std::size_t size() const { return size_; }

    void clear() {
        for (std::size_t index = 0; index < bucketCount_; ++index) {
            Node* current = buckets_[index];
            while (current != nullptr) {
                Node* removed = current;
                current = current->next;
                delete removed;
            }
            buckets_[index] = nullptr;
        }
        size_ = 0;
    }

private:
    Node** buckets_;
    std::size_t bucketCount_;
    std::size_t size_;

    std::size_t hash(const std::string& key) const {
        unsigned long value = 5381;
        for (const unsigned char character : key) {
            value = ((value << 5U) + value) + character;
        }
        return static_cast<std::size_t>(value % bucketCount_);
    }
};
