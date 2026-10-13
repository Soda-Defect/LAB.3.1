#ifndef TREENODE_H
#define TREENODE_H

template <typename T, size_t N>
struct Node {
    T data;
    Node* parent;
    Node* children[N];   

    Node(const T& value, Node* p = nullptr) : data(value), parent(p) {
        for (size_t i = 0; i < N; ++i) {
            children[i] = nullptr;
        }
    }
}

class TreeNode{
private:
    Node *root;
    size_t size;
public:
    TreeNode() : root(nullptr), size(0) {}

}
#endif
