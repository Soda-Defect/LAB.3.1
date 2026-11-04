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
#include <istream>
#include <locale>
#include <sstream>
#include <iomanip>
#include <memory>
#include <unordered_map>

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
    static Pointer FindNode(Pointer subtree, const T& value) {
        for (Pointer node = subtree; node; node = NextPreorder(node, subtree)) {
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
            if (!(a->value == b->value)) {
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

    static std::string EncodeValue(const T& value) {
        if constexpr (std::is_same_v<T, std::string>) {
            return value;
        } else {
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << std::setprecision(std::numeric_limits<long double>::max_digits10)
                   << value;
            if (!stream) throw std::invalid_argument("Cannot encode value");
            return stream.str();
        }
    }

    static T DecodeValue(std::string_view text) {
        if constexpr (std::is_same_v<T, std::string>) {
            return std::string(text);
        } else if constexpr (std::is_same_v<T, char> ||
                             std::is_same_v<T, signed char> ||
                             std::is_same_v<T, unsigned char>) {
            if (text.size() != 1) throw std::invalid_argument("Invalid character value");
            return static_cast<T>(text[0]);
        } else {
            std::istringstream stream{std::string(text)};
            stream.imbue(std::locale::classic());
            T value{};
            if (!(stream >> value)) throw std::invalid_argument("Cannot decode value");
            stream >> std::ws;
            if (!stream.eof()) throw std::invalid_argument("Unexpected characters in value");
            return value;
        }
    }

    static std::string_view ReadPayload(std::string_view text, size_t& pos) {
        size_t length = 0;
        const size_t begin = pos;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            const size_t digit = static_cast<size_t>(text[pos++] - '0');
            if (length > (std::numeric_limits<size_t>::max() - digit) / 10) {
                throw std::invalid_argument("Value length overflow");
            }
            length = length * 10 + digit;
        }
        if (pos == begin || pos == text.size() || text[pos++] != ':') {
            throw std::invalid_argument("Expected length:value");
        }
        if (length > text.size() - pos) throw std::invalid_argument("Truncated value");
        const std::string_view payload = text.substr(pos, length);
        pos += length;
        return payload;
    }

    struct FormatSpec {
        Order order{};
        std::array<std::string, 5> literals{};
    };

    static FormatSpec ParseFormat(std::string_view format) {
        FormatSpec result;
        bool seen[4] = {false, false, false, false};
        size_t field = 0;
        for (size_t pos = 0; pos < format.size();) {
            const char c = format[pos];
            if (c == '\\') {
                if (++pos == format.size()) {
                    throw std::invalid_argument("Incomplete format escape");
                }
                result.literals[field].push_back(format[pos++]);
                continue;
            }
            int token = -1;
            size_t width = 1;
            if (c == 'K') {
                token = 0;
            } else if (c >= '1' && c <= '3') {
                token = c - '0';
            } else if (static_cast<unsigned char>(c) == 0xD0 &&
                       pos + 1 < format.size() &&
                       static_cast<unsigned char>(format[pos + 1]) == 0x9A) {
                token = 0; 
                width = 2;
            }
            if (token < 0) {
                result.literals[field].push_back(c);
            } else {
                if (field == 4 || seen[token]) {
                    throw std::invalid_argument("Format repeats a tree field");
                }
                seen[token] = true;
                result.order[field++] = token;
            }
            pos += width;
        }
        if (field != 4) {
            throw std::invalid_argument("Format must contain K, 1, 2 and 3 exactly once");
        }
        if ((!result.literals[0].empty() && result.literals[0][0] == '#') || (result.literals[0].empty() && result.order[0] != 0)) {
            throw std::invalid_argument("Ambiguous format: use an opening delimiter or start with K");
        }
        return result;
    }

    static void ReadLiteral(std::string_view text, size_t& pos, std::string_view literal) {
        if (literal.size() > text.size() - pos ||
            text.compare(pos, literal.size(), literal) != 0) {
            throw std::invalid_argument("Text does not match the formatting string");
        }
        pos += literal.size();
    }

    static size_t ReadListNumber(std::string_view text, size_t& pos) {
        const size_t begin = pos;
        size_t result = 0;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            const size_t digit = static_cast<size_t>(text[pos++] - '0');
            if (result > (std::numeric_limits<size_t>::max() - digit) / 10) {
                throw std::invalid_argument("List number overflow");
            }
            result = result * 10 + digit;
        }
        if (pos == begin) {
            throw std::invalid_argument("Expected an unsigned list number");
        }
        return result;
    }

    static void ReadListSpace(std::string_view text, size_t& pos, bool required) {
        const size_t begin = pos;
        while (pos < text.size()) {
            const char c = text[pos];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\v' && c != '\f') {
                break;
            }
            ++pos;
        }
        if (required && pos == begin) {
            throw std::invalid_argument("Expected whitespace between list fields");
        }
    }

    struct ParentRecord {
        std::unique_ptr<Node<T>> node;
        size_t parent = 0;
        size_t childNumber = 0;
    };

    struct ParseFrame {
        ParseFrame* parent;
        size_t slot;
        size_t stage = 0;
        Node<T>* node = nullptr;
        Node<T>* pending[3] = {nullptr, nullptr, nullptr};

        ParseFrame(ParseFrame* parentFrame = nullptr, size_t childSlot = 0) : parent(parentFrame), slot(childSlot) {}

        ~ParseFrame() {
            Destroy(node);
            for (Node<T>* child : pending) {
                Destroy(child);
            }
        }

        void StoreChild(size_t childSlot, Node<T>* child) noexcept {
            if (node) {
                node->children[childSlot] = child;
                if (child) {
                    child->parent = node;
                }
            } else {
                pending[childSlot] = child;
            }
        }

        void StoreNode(Node<T>* newNode) {
            node = newNode;
            for (size_t i = 0; i < 3; ++i) {
                node->children[i] = pending[i];
                if (pending[i]) {
                    pending[i]->parent = node;
                }
                pending[i] = nullptr;
            }
        }
    };

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

    bool Empty() const { return root == nullptr; }

    size_t Size() const { return size; }

    T& GetRoot() { return At(""); }
    const T& GetRoot() const { return At(""); }

    T& SetRoot(const T& value) { return SetRootImpl(value); }

    T& SetRoot(T&& value) { return SetRootImpl(std::move(value)); }

    T& Insert(std::string_view parentPath, size_t childNumber, const T& value) {
        return InsertImpl(parentPath, childNumber, value);
    }

    T& At(std::string_view path) { return Resolve(root, path)->value; }
    const T& At(std::string_view path) const { return Resolve(static_cast<const Node<T>*>(root), path)->value; }

    T& AtRelative(std::string_view startPath, std::string_view relativePath) {
        return Resolve(Resolve(root, startPath), relativePath, true)->value;
    }

    const T& AtRelative(std::string_view startPath, std::string_view relativePath) const {
        const Node<T>* start = Resolve(static_cast<const Node<T>*>(root), startPath);
        return Resolve(start, relativePath, true)->value;
    }

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

    TernaryTree ExtractSubtree(const T& value) const {
        const Node<T>* found = FindNode(static_cast<const Node<T>*>(root), value);
        if (!found) {
            throw std::out_of_range("Value does not exist");
        }
        return CopySubtree(found);
    }

    TernaryTree ExtractSubtreeAt(std::string_view path) const {
        return CopySubtree(Resolve(static_cast<const Node<T>*>(root), path));
    }

    bool ContainsSubtree(const TernaryTree& subtree) const {
        if (subtree.Empty()) {
            return true;
        }
        if (subtree.size > size) {
            return false;
        }
        const Node<T>* boundary = root;
        for (const Node<T>* node = root; node; node = NextPreorder(node, boundary)) {
            if (Equal(node, subtree.root)) return true;
        }
        return false;
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

    template <typename Encoder>
    std::string ToStringWith(Encoder&& encoder, std::string_view order = "K123") const {
        std::string result;
        Walk(static_cast<const Node<T>*>(root), ParseOrder(order),
             [&](const Node<T>* node, Event event) {
                 if (event == Open) result.push_back('(');
                 else if (event == Close) result.push_back(')');
                 else if (event == Null) result.push_back('#');
                 else {
                     const std::string payload = encoder(node->value);
                     result += std::to_string(payload.size());
                     result.push_back(':');
                     result += payload;
                 }
                 return true;
             });
        return result;
    }

    std::string ToString(std::string_view order = "K123") const {
        return ToStringWith([](const T& value) { return EncodeValue(value); }, order);
    }


    template <typename Decoder>
    static TernaryTree FromStringWith(std::string_view text, Decoder&& decoder, std::string_view order = "K123") {
        const Order parsedOrder = ParseOrder(order);
        TernaryTree result;
        if (text == "#") return result;
        if (text.empty() || text[0] != '(') {
            throw std::invalid_argument("Expected a node or #");
        }
        size_t pos = 1;
        ParseFrame* frame = new ParseFrame;
        try {
            while (frame) {
                if (frame->stage == 4) {
                    if (pos == text.size() || text[pos++] != ')') {
                        throw std::invalid_argument("Expected closing parenthesis");
                    }
                    Node<T>* completed = frame->node;
                    frame->node = nullptr;
                    const size_t slot = frame->slot;
                    ParseFrame* parent = frame->parent;
                    delete frame;
                    frame = parent;
                    if (frame) frame->StoreChild(slot, completed);
                    else result.root = completed;
                } else {
                    const int token = parsedOrder[frame->stage++];
                    if (token == 0) {
                        const std::string_view payload = ReadPayload(text, pos);
                        frame->StoreNode(new Node<T>(decoder(payload)));
                        ++result.size;
                    } else {
                        if (pos == text.size()) throw std::invalid_argument("Truncated tree");
                        const char c = text[pos++];
                        if (c == '(') frame = new ParseFrame(frame, token - 1);
                        else if (c != '#') throw std::invalid_argument("Expected a child or #");
                    }
                }
            }
            if (pos != text.size()) throw std::invalid_argument("Unexpected trailing characters");
        } catch (...) {
            while (frame) {
                ParseFrame* parent = frame->parent;
                delete frame;
                frame = parent;
            }
            throw;
        }
        return result;
    }

    static TernaryTree FromString(std::string_view text, std::string_view order = "K123") { 
        return FromStringWith(text, [](std::string_view payload) { return DecodeValue(payload); }, order); 
    }

    template <typename Encoder>
    std::string ToStringFormatWith(std::string_view format, Encoder&& encoder) const {
        const FormatSpec spec = ParseFormat(format);
        if (!root) return "#";
        std::string result;
        const Node<T>* node = root;
        size_t stage = 0;
        while (true) {
            result += spec.literals[stage];
            if (stage == 4) {
                if (node == root) break;
                const int token = static_cast<int>(ChildIndex(node)) + 1;
                node = node->parent;
                stage = 0;
                while (spec.order[stage] != token) ++stage;
                ++stage;
            } else {
                const int token = spec.order[stage++];
                if (token == 0) {
                    const std::string payload = encoder(node->value);
                    result += std::to_string(payload.size());
                    result.push_back(':');
                    result += payload;
                } else if (node->children[token - 1]) {
                    node = node->children[token - 1];
                    stage = 0;
                } else {
                    result.push_back('#');
                }
            }
        }
        return result;
    }

    std::string ToStringFormat(std::string_view format) const {
        return ToStringFormatWith(format, [](const T& value) { return EncodeValue(value); });
    }

    template <typename Decoder>
    static TernaryTree FromStringFormatWith(std::string_view text, std::string_view format,
                                            Decoder&& decoder) {
        const FormatSpec spec = ParseFormat(format);
        TernaryTree result;
        if (text == "#") return result;
        size_t pos = 0;
        ParseFrame* frame = new ParseFrame;
        try {
            while (frame) {
                ReadLiteral(text, pos, spec.literals[frame->stage]);
                if (frame->stage == 4) {
                    Node<T>* completed = frame->node;
                    frame->node = nullptr;
                    const size_t slot = frame->slot;
                    ParseFrame* parent = frame->parent;
                    delete frame;
                    frame = parent;
                    if (frame) frame->StoreChild(slot, completed);
                    else result.root = completed;
                } else {
                    const int token = spec.order[frame->stage++];
                    if (token == 0) {
                        const std::string_view payload = ReadPayload(text, pos);
                        frame->StoreNode(new Node<T>(decoder(payload)));
                        ++result.size;
                    } else {
                        if (pos == text.size()) throw std::invalid_argument("Truncated formatted tree");
                        if (text[pos] == '#') ++pos;
                        else frame = new ParseFrame(frame, static_cast<size_t>(token - 1));
                    }
                }
            }
            if (pos != text.size()) throw std::invalid_argument("Unexpected trailing characters");
        } catch (...) {
            while (frame) {
                ParseFrame* parent = frame->parent;
                delete frame;
                frame = parent;
            }
            throw;
        }
        return result;
    }


    static TernaryTree FromStringFormat(std::string_view text, std::string_view format) {
        return FromStringFormatWith(text, format, [](std::string_view payload) { return DecodeValue(payload); });
    }

    template <typename Encoder>
    std::string ToParentListWith(Encoder&& encoder) const {
        std::string result = std::to_string(size) + '\n';
        std::unordered_map<const Node<T>*, size_t> identifiers;
        identifiers.reserve(size);
        const Node<T>* boundary = root;
        size_t id = 0;
        for (const Node<T>* node = root; node; node = NextPreorder(node, boundary)) {
            ++id;
            const size_t parentId = node->parent ? identifiers.at(node->parent) : 0;
            const size_t childNumber = node->parent ? ChildIndex(node) + 1 : 0;
            const std::string payload = encoder(node->value);
            result += std::to_string(id) + ' ' + std::to_string(parentId) + ' ' +
                      std::to_string(childNumber) + ' ' + std::to_string(payload.size()) + ':';
            result += payload;
            result.push_back('\n');
            identifiers.emplace(node, id);
        }
        return result;
    }

    std::string ToParentList() const { return ToParentListWith([](const T& value) { return EncodeValue(value); }); };

    template <typename Decoder>
    static TernaryTree FromParentListWith(std::string_view text, Decoder&& decoder) {
        TernaryTree result;
        size_t pos = 0;
        ReadListSpace(text, pos, false);
        const size_t count = ReadListNumber(text, pos);
        if (count == 0) {
            ReadListSpace(text, pos, false);
            if (pos != text.size()) throw std::invalid_argument("Unexpected empty-list data");
            return result;
        }
        if (count > text.size() || count == std::numeric_limits<size_t>::max()) {
            throw std::invalid_argument("Node count does not match the list");
        }
        ReadListSpace(text, pos, true);
        auto records = std::make_unique<ParentRecord[]>(count + 1);
        size_t rootId = 0;
        for (size_t row = 0; row < count; ++row) {
            const size_t id = ReadListNumber(text, pos);
            ReadListSpace(text, pos, true);
            const size_t parentId = ReadListNumber(text, pos);
            ReadListSpace(text, pos, true);
            const size_t childNumber = ReadListNumber(text, pos);
            ReadListSpace(text, pos, true);
            const std::string_view payload = ReadPayload(text, pos);
            if (id == 0 || id > count || parentId > count || records[id].node) {
                throw std::invalid_argument("Invalid or duplicate node ID");
            }
            if (parentId == 0) {
                if (childNumber != 0 || rootId != 0) {
                    throw std::invalid_argument("List must have exactly one root with position 0");
                }
                rootId = id;
            } else if (parentId == id || childNumber < 1 || childNumber > 3) {
                throw std::invalid_argument("Invalid parent or child position");
            }
            records[id].node.reset(new Node<T>(decoder(payload)));
            records[id].parent = parentId;
            records[id].childNumber = childNumber;
            ReadListSpace(text, pos, row + 1 < count);
        }
        if (pos != text.size() || rootId == 0) {
            throw std::invalid_argument("Missing root or unexpected trailing list data");
        }
        for (size_t id = 1; id <= count; ++id) {
            const size_t parentId = records[id].parent;
            if (parentId == 0) continue;
            Node<T>* parent = records[parentId].node.get();
            const size_t slot = records[id].childNumber - 1;
            if (parent->children[slot]) throw std::invalid_argument("Two nodes occupy one child slot");
            parent->children[slot] = records[id].node.get();
            records[id].node->parent = parent;
        }
        const Node<T>* boundary = records[rootId].node.get();
        size_t visited = 0;
        for (const Node<T>* node = boundary; node; node = NextPreorder(node, boundary)) {
            if (++visited > count) throw std::invalid_argument("Cycle in parent list");
        }
        if (visited != count) throw std::invalid_argument("Parent list is disconnected or contains a cycle");
        result.root = records[rootId].node.get();
        result.size = count;
        for (size_t id = 1; id <= count; ++id) records[id].node.release();
        return result;
    }

    static TernaryTree FromParentList(std::string_view text) {
        return FromParentListWith(text, [](std::string_view payload) { return DecodeValue(payload); });
    }
};

#endif
