#ifndef TERNARY_H
#define TERNARY_H

#include <array>
#include <utility>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

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

    enum Event { Open, Value, Close, Null };
    using Order = std::array<int, 4>;

    static size_t ChildIndex(const Node<T>* node) {
        for (size_t i = 0; i < 3; ++i) {
            if (node->parent->children[i] == node) {
                return i;
            }
        }
        return 3; 
    }

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

            Node<T>* parent = current->parent;
            if (parent && parent != stop) {
                parent->children[ChildIndex(current)] = nullptr;
            }
            delete current;
            ++deleted;
            current = parent;
        }
        return deleted;
    }

    template <typename Pointer>
    static Pointer NextPreorder(Pointer node, Pointer boundary) noexcept {
        for (size_t i = 0; i < 3; ++i) {
            if (node->children[i]) {
                return node->children[i];
            }
        }
        while (node != boundary) {
            const size_t next = ChildIndex(node) + 1;
            node = node->parent;
            for (size_t i = next; i < 3; ++i) {
                if (node->children[i]) {
                    return node->children[i];
                }
            }
        }
        return nullptr;
    }

    template <typename Pointer>
    static Node<T>* FindNode(Node<T>* subtree, const T& value) {
        for (Node<T>* node = subtree; node; node = NextPreorder(node, subtree)) {
            if (node->value == value) {
                return node;
            }
        }
        return nullptr;
    }

    static Order ParseOrder(std::string_view text) {
        if (text.size() != 4) {
            throw std::invalid_argument("Order must be a permutation of K123");
        }
        Order order{};
        bool seen[4] = {false, false, false, false};
        for (size_t i = 0; i < 4; ++i) {
            const char c = text[i];
            const int token = c == 'K' ? 0 : (c >= '1' && c <= '3' ? c - '0' : -1);
            if (token < 0 || seen[token]) {
                throw std::invalid_argument("Order must be a permutation of K123");
            }
            seen[token] = true;
            order[i] = token;
        }
        return order;
    }

    template <typename Pointer, typename Visitor>
    static bool Walk(Pointer subtree, const Order& order, Visitor&& visitor) {
        if (!subtree) {
            return visitor(subtree, Null);
        }
        Pointer node = subtree;
        int stage = -1;
        while (true) {
            if (stage == -1) {
                if (!visitor(node, Open)) {
                    return false;
                }
                stage = 0;
            } else if (stage == 4) {
                if (!visitor(node, Close)) {
                    return false;
                }
                if (node == subtree) {
                    return true;
                }
                const int token = static_cast<int>(ChildIndex(node)) + 1;
                node = node->parent;
                stage = 0;
                while (order[stage] != token) ++stage;
                ++stage;
            } else {
                const int token = order[stage++];
                if (token == 0) {
                    if (!visitor(node, Value)) return false;
                } else {
                    Pointer child = node->children[token - 1];
                    if (child) {
                        node = child;
                        stage = -1;
                    } else if (!visitor(child, Null)) {
                        return false;
                    }
                }
            }
        }
    }

    template <typename Transform>
    static Node<T>* Clone(const Node<T>* source, Transform&& transform, size_t& count) {
        count = 0;
        if (!source) {
            return nullptr;
        }
        Node<T>* copy = new Node<T>(transform(source->value));
        count = 1;
        const Node<T>* from = source;
        Node<T>* to = copy;
        size_t child = 0;
        try {
            while (true) {
                if (child < 3) {
                    const size_t i = child++;
                    if (from->children[i]) {
                        const Node<T>* sourceChild = from->children[i];
                        Node<T>* copyChild = new Node<T>(transform(sourceChild->value), to);
                        to->children[i] = copyChild;
                        from = sourceChild;
                        to = copyChild;
                        child = 0;
                        ++count;
                    }
                } else {
                    if (from == source) {
                        break;
                    }
                    child = ChildIndex(from) + 1;
                    from = from->parent;
                    to = to->parent;
                }
            }
        } catch (...) {
            Destroy(copy);
            throw;
        }
        return copy;
    }

    static TernaryTree CopySubtree(const Node<T>* source) {
        TernaryTree result;
        result.root = Clone(source, [](const T& value) -> const T& { return value; }, result.size);
        return result;
    }

    template <typename Pointer>
    static Pointer Resolve(Pointer start, std::string_view path, bool relative = false) {
        for (char c : path) {
            if ((c < '1' || c > '3') && !(relative && c == '^')) {
                throw std::invalid_argument("Path accepts 1, 2, 3; relative path also accepts ^");
            }
        }
        if (!start){
            throw std::out_of_range("Starting node does not exist");
        }
        for (char c : path) {
            start = ((c == '^') ? start->parent : start->children[c - '1']);
            if (!start) {
                throw std::out_of_range("Path does not exist");
            }
        }
        return start;
    }


    static size_t Slot(size_t childNumber) {
        if (childNumber < 1 || childNumber > 3) {
            throw std::out_of_range("Child number must be 1, 2 or 3");
        }
        return childNumber - 1;
    }

    template <typename U>
    T& SetRootImpl(U&& value) {
        if (root) {
            throw std::logic_error("Root already exists");
        }
        root = new Node<T>(std::forward<U>(value));
        size = 1;
        return root->value;
    }

    template <typename U>
    T& InsertImpl(std::string_view parentPath, size_t childNumber, U&& value) {
        const size_t slot = Slot(childNumber);
        Node<T>* parent = Resolve(root, parentPath);
        if (parent->children[slot]) {
            throw std::logic_error("Child slot is occupied");
        }
        Node<T>* child = new Node<T>(std::forward<U>(value), parent);
        parent->children[slot] = child;
        ++size;
        return child->value;
    }

    size_t RemoveNode(Node<T>* node) {
        if (!node){
            return 0;
        }
        if (node == root){
            root = nullptr;
        }
        else{
            node->parent->children[ChildIndex(node)] = nullptr;
        }
        const size_t removed = Destroy(node);
        size -= removed;
        return removed;
    }

    static bool Equal(const Node<T>* first, const Node<T>* second) {
        const Node<T>* a = first;
        const Node<T>* b = second;
        while (a && b) {
            if (a->value != b->value) {
                return false;
            }
            for (size_t i = 0; i < 3; ++i) {
                if ((a->children[i] == nullptr) != (b->children[i] == nullptr)) {
                    return false;
                }
            }
            a = NextPreorder(a, first);
            b = NextPreorder(b, second);
        }
        return !a && !b;
    }

    void AppendCompact(const T& value) {
        if (!root) {
            SetRoot(value);
            return;
        }
        size_t index = (size - 1) / 3;
        size_t path[std::numeric_limits<size_t>::digits];
        size_t depth = 0;
        while (index != 0) {
            path[depth++] = (index - 1) % 3;
            index = (index - 1) / 3;
        }
        Node<T>* parent = root;
        while (depth != 0) parent = parent->children[path[--depth]];
        const size_t slot = (size - 1) % 3;
        parent->children[slot] = new Node<T>(value, parent);
        ++size;
    }

public:
    TernaryTree() : root(nullptr), size(0) {}

    TernaryTree(const T& rootValue) {
        root = new Node<T>(rootValue);
        size = 1;
    }

    TernaryTree(const TernaryTree& other) {
        root = Clone(other.root, [](const T& value) -> const T& { return value; }, size);
    }

    TernaryTree(TernaryTree&& other) {
        root = other.root;
        size = other.size;
        other.root = nullptr;
        other.size = 0;
    }
    
    TernaryTree& operator=(TernaryTree other){
        Swap(other);
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

    T& GetRoot() { return At(""); }
    const T& GetRoot() const { return At(""); }

    T& Insert(std::string_view parentPath, size_t childNumber, const T& value) {
        return InsertImpl(parentPath, childNumber, value);
    }

    T& At(std::string_view path) { return Resolve(root, path)->value; }
    const T& At(std::string_view path) const { return Resolve(static_cast<const Node<T>*>(root), path)->value; }

    T* Find(const T& value) {
        Node<T>* found = FindNode(root, value);
        return found ? &found->value : nullptr;
    }

    const T* Find(const T& value) const {
        const Node<T>* found = FindNode(static_cast<const Node<T>*>(root), value);
        return found ? &found->value : nullptr;
    }

    bool Contains(const T& value) const { return Find(value) != nullptr; }

    size_t RemoveSubtree(std::string_view path) {
        if (path.empty() && !root) {
            return 0;
        }
        return RemoveNode(Resolve(root, path));
    }

    bool Remove(const T& value) { return RemoveNode(FindNode(root, value)) != 0; }

    template <typename Function>
    void Traverse(Function&& function, std::string_view order = "K123") {
        Walk(root, ParseOrder(order), [&](Node<T>* node, Event event) {
            if (event == Value) function(node->value);
            return true;
        });
    }

    template <typename Function>
    void Traverse(Function&& function, std::string_view order = "K123") const {
        Walk(static_cast<const Node<T>*>(root), ParseOrder(order),
             [&](const Node<T>* node, Event event) {
                 if (event == Value) function(node->value);
                 return true;
             });
    }

    template <typename Function>
    TernaryTree Map(Function&& function) const {
        TernaryTree result;
        result.root = Clone(root, std::forward<Function>(function), result.size);
        return result;
    }

    template <typename Accumulator, typename Function>
    Accumulator Reduce(Accumulator initial, Function&& function, std::string_view order = "K123") const {
        Traverse([&](const T& value) { initial = function(std::move(initial), value); }, order);
        return initial;
    }

    template <typename Predicate>
    TernaryTree Where(Predicate&& predicate) const {
        TernaryTree result;
        Traverse([&](const T& value) { if (predicate(value)) result.AppendCompact(value);});
        return result;
    }

    bool operator==(const TernaryTree& other) const {
        return size == other.size && Equal(root, other.root);
    }

    bool operator!=(const TernaryTree& other) const { return !(*this == other); }

    void Merge(std::string_view parentPath, size_t childNumber, const TernaryTree& other) {
        const size_t slot = Slot(childNumber);
        Node<T>* parent = Resolve(root, parentPath);
        if (parent->children[slot]) {
            throw std::logic_error("Child slot is occupied");
        }
        TernaryTree copy(other);
        if (!copy.root) {
            return;
        }
        parent->children[slot] = copy.root;
        copy.root->parent = parent;
        size += copy.size;
        copy.root = nullptr;
        copy.size = 0;
    }
};

#endif
