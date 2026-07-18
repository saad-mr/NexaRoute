#pragma once

#include <algorithm>
#include <cstddef>

template <typename T>
class AVLTree {
private:
    struct Node {
        T value;
        Node* left;
        Node* right;
        int height;

        explicit Node(const T& item)
            : value(item), left(nullptr), right(nullptr), height(1) {}
    };

public:
    AVLTree() : root_(nullptr), size_(0) {}

    ~AVLTree() {
        clear(root_);
    }

    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    void insert(const T& value) {
        bool inserted = false;
        root_ = insert(root_, value, inserted);
        if (inserted) ++size_;
    }

    const T* find(const T& value) const {
        const Node* current = root_;
        while (current != nullptr) {
            if (value < current->value) current = current->left;
            else if (current->value < value) current = current->right;
            else return &current->value;
        }
        return nullptr;
    }

    template <typename Visitor>
    void inOrder(Visitor visitor) const {
        inOrder(root_, visitor);
    }

    std::size_t size() const { return size_; }

private:
    Node* root_;
    std::size_t size_;

    static int height(Node* node) { return node == nullptr ? 0 : node->height; }
    static int balance(Node* node) { return node == nullptr ? 0 : height(node->left) - height(node->right); }

    static Node* rotateRight(Node* top) {
        Node* left = top->left;
        Node* transferred = left->right;
        left->right = top;
        top->left = transferred;
        top->height = 1 + std::max(height(top->left), height(top->right));
        left->height = 1 + std::max(height(left->left), height(left->right));
        return left;
    }

    static Node* rotateLeft(Node* top) {
        Node* right = top->right;
        Node* transferred = right->left;
        right->left = top;
        top->right = transferred;
        top->height = 1 + std::max(height(top->left), height(top->right));
        right->height = 1 + std::max(height(right->left), height(right->right));
        return right;
    }

    static Node* insert(Node* node, const T& value, bool& inserted) {
        if (node == nullptr) {
            inserted = true;
            return new Node(value);
        }

        if (value < node->value) node->left = insert(node->left, value, inserted);
        else if (node->value < value) node->right = insert(node->right, value, inserted);
        else return node;

        node->height = 1 + std::max(height(node->left), height(node->right));
        const int nodeBalance = balance(node);

        if (nodeBalance > 1 && value < node->left->value) return rotateRight(node);
        if (nodeBalance < -1 && node->right->value < value) return rotateLeft(node);
        if (nodeBalance > 1 && node->left->value < value) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }
        if (nodeBalance < -1 && value < node->right->value) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }
        return node;
    }

    template <typename Visitor>
    static void inOrder(const Node* node, Visitor visitor) {
        if (node == nullptr) return;
        inOrder(node->left, visitor);
        visitor(node->value);
        inOrder(node->right, visitor);
    }

    static void clear(Node* node) {
        if (node == nullptr) return;
        clear(node->left);
        clear(node->right);
        delete node;
    }
};
