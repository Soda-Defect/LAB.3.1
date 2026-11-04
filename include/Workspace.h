#pragma once

#include "TernaryTree.h"
#include "ValueTypes.h"
#include <deque>
#include <variant>

namespace lab {
enum class DataType { Integer, Real, Complex, String, Function, Student, Teacher };
enum class Storage { Traversal, Format, ParentList };
template<class T> struct TreePair { using ValueType = T; TernaryTree<T> trees[2]; };

struct ViewNode {
    std::size_t id{}, parent{}, slot{};
    std::string path, value;
};

// Независимый от Qt слой: валидация ввода и работа с двумя деревьями.
class Workspace {
public:
    Workspace();
    DataType type() const { return type_; }
    void setType(DataType type);
    int active() const { return active_; }
    void setActive(int index);
    std::size_t size() const;
    std::size_t otherSize() const;
    void clear();
    void setRoot(const std::string& value);
    void insert(const std::string& parent, int slot, const std::string& value);
    void setValue(const std::string& path, const std::string& value);
    std::string at(const std::string& path, const std::string& relative = "", bool useRelative = false) const;
    bool contains(const std::string& value) const;
    bool removeValue(const std::string& value);
    std::size_t removeSubtree(const std::string& path);
    void extract(const std::string& query, bool byValue);
    void merge(const std::string& parent, int slot);
    void copyToOther();
    void swapTrees();
    bool containsOther() const;
    bool equal() const;
    std::string traverse(const std::string& order) const;
    std::string serialize(Storage mode, const std::string& order, const std::string& format) const;
    void deserialize(const std::string& text, Storage mode, const std::string& order, const std::string& format);
    std::deque<ViewNode> snapshot(std::size_t limit = 400) const;
    std::string hint() const;
    std::deque<std::string> mapNames() const;
    std::deque<std::string> filterNames() const;
    void mapToOther(int operation, const std::string& parameter);
    void filterToOther(int operation, const std::string& parameter);
    std::string reduce(const std::string& parameter, const std::string& order) const;
    void generate(std::size_t count, int minimum, int maximum, unsigned seed);

private:
    using Data = std::variant<TreePair<int>, TreePair<double>, TreePair<std::complex<double>>,
                              TreePair<std::string>, TreePair<Function>, TreePair<Student>, TreePair<Teacher>>;
    Data data_;
    DataType type_ = DataType::Integer;
    int active_ = 0;
};
} // namespace lab
