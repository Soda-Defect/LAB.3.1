#ifndef TERNARY_H
#define TERNARY_H

#include <utility>
#include <cstddef>
#include <stdexcept>

template <typename T> class TernaryTree;

template <typename T>
class Node {
private:
    T value;
    Node* parent;
    Node* children[3];  
public:
    Node(const T& newValue, Node* parentNode = nullptr) : value(newValue), parent(parentNode), children{nullptr, nullptr, nullptr} {}

    Node(T&& newValue, Node* parentNode = nullptr) : value(std::move(newValue)), parent(parentNode), children{nullptr, nullptr, nullptr} {}
    
    friend class TernaryTree<T>;
};

template <typename T>
class TernaryTree {
private:
    Node<T> *root;
    size_t size;

    static size_t Destroy(Node<T>* subtree){
        if (!subtree) {
            return 0;
        }

        Node<T>* const stop = subtree->parent;
        Node<T>* current = subtree;
        size_t deleted = 0;

        while (current != stop) {
            Node<T>* child = nullptr;
            for (int i = 0; i < 3; ++i) {
                if (current->children[i]) {
                    child = current->children[i];
                    break;
                }
            }
            if (child) {
                current = child;
                continue;
            }

            Node<T>* const parent = current->parent;
            if (parent && parent != stop) {
                for (int i = 0; i < 3; ++i) {
                    if (parent->children[i] == current) {
                        parent->children[i] = nullptr;
                        break;
                    }
                }
            }
            delete current;
            ++deleted;
            current = parent;
        }
        return deleted;
    }

    static Node<T>* Clone(const Node<T>* source, Node<T>* parent){
        if(!source){
            return nullptr;
        }
        Node<T>* copy = new Node(source->value, parent);
        try{
            for(int i = 0; i < 3; i++){
                copy->children[i] = Clone(source->children[i], copy);
            }
        } catch(...){
            copy->parent = nullptr;
            Destroy(copy);
            throw;
        }
        return copy;
    }

    static Node* FindNode(Node* node, const T& value) {
        if (!node) {
            return nullptr;
        }
        if (node->value == value) {
            return node;
        }
        for (int i = 0; i < 3; ++i) {
            if (Node* found = FindNode(node->children[i], value)) {
                return found;
            }
        }
        return nullptr;
    }

    static const Node* FindNode(const Node* node, const T& value) {
        if (!node) {
            return nullptr;
        }
        if (node->value == value) {
            return node;
        }
        for (int i = 0; i < 3; ++i) {
            if (const Node* found = FindNode(node->children[i], value)) {
                return found;
            }
        }
        return nullptr;
    }

public:
    TernaryTree() : root(nullptr), size(0) {}
    TernaryTree(const T& rootValue) {
        root = new Node<T>(rootValue);
        size = 1;
    }
    TernaryTree(const TernaryTree& other) {
        root = Clone(other.root, nullptr);
        size = other.size;
    }
    TernaryTree(TernaryTree&& other) {
        root = std::move(other.root);
        size = other.size;
        other.root = nullptr;
        other.size = 0;
    }
    TernaryTree& operator=(TernaryTree other){
        Swap(other);
        return *this;
    }
    TernaryTree& operator=(TernaryTree&& other){
        if(this != &other){
            Clear();
            root = other.root;
            size = other.size;
            other.root = nullptr;
            other.size = 0;
        }
        return *this;
    }
    ~TernaryTree() {
        Clear();
    }

    void Swap(TernaryTree& other){
        Node<T>* tmpRoot = root;
        root = other.root;
        other.root = tmpRoot;

        size_t tmpSize = size;
        size = other.size;
        other.size = tmpSize;
    }

    static void Swap(TernaryTree& first, TernaryTree& second){
        first.Swap(second);
    }

    void Clear() {
        Destroy(root);
        root = nullptr;
        size = 0;
    }

    bool Empty() { return root == nullptr; }
    size_t Size() { return size; }

    T* Find(const T& value) {
        Node* found = FindNode(root, value);
        return found ? &found->value : nullptr;
    }

    const T* Find(const T& value) const {
        const Node* found = FindNode(root, value);
        return found ? &found->value : nullptr;
    }
};

#endif
