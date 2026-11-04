#include "Workspace.h"
#include <random>

namespace lab {
namespace {
template<class Pair> using ValueOf = typename std::decay_t<Pair>::ValueType;

template<class T> auto makeMap(int op, const std::string& text) {
    if (op < 0 || op > 2 || (std::is_same_v<T, Function> && op != 0))
        throw std::invalid_argument("Неизвестное преобразование.");
    T replacement{};
    int offset = 0;
    if (op == 0) replacement = Codec<T>::parse(text);
    if constexpr (std::is_same_v<T, Student> || std::is_same_v<T, Teacher>) {
        if (op == 1) offset = readNumber<int>(text);
        if (op == 2 && text.find('|') != std::string::npos)
            throw std::invalid_argument("Символ | разделяет поля записи.");
    } else if constexpr (!std::is_same_v<T, std::string> && !std::is_same_v<T, Function>) {
        if (op != 0) replacement = Codec<T>::parse(text);
    }
    return [=](const T& x) -> T {
        if (op == 0) return replacement;
        if constexpr (std::is_same_v<T, int>) {
            return checkedInt(op == 1 ? static_cast<long long>(x) + replacement
                                      : static_cast<long long>(x) * replacement);
        } else if constexpr (std::is_same_v<T, double>) {
            return checkedReal(op == 1 ? x + replacement : x * replacement);
        } else if constexpr (std::is_same_v<T, std::complex<double>>) {
            return checkedComplex(op == 1 ? x + replacement : x * replacement);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return op == 1 ? text + x : x + text;
        } else if constexpr (std::is_same_v<T, Student> || std::is_same_v<T, Teacher>) {
            T result = x;
            if (op == 1) {
                result.id = checkedInt(static_cast<long long>(x.id) + offset);
                if (result.id < 0) throw std::invalid_argument("ID не может быть отрицательным.");
            } else result.name += text;
            return result;
        } else return x;
    };
}

template<class T> auto makeFilter(int op, const std::string& text) {
    if (op < 0 || op > 2) throw std::invalid_argument("Неизвестное условие.");
    double threshold = 0;
    T value{};
    if constexpr (std::is_same_v<T, int> || std::is_same_v<T, double>) value = Codec<T>::parse(text);
    else if constexpr (std::is_same_v<T, std::complex<double>>) threshold = readNumber<double>(text);
    else if constexpr (std::is_same_v<T, Function>) {
        if (op == 2) value = Codec<T>::parse(text);
        else threshold = readNumber<int>(text);
    } else if constexpr (std::is_same_v<T, Student> || std::is_same_v<T, Teacher>) {
        if (op != 2) threshold = readNumber<int>(text);
    } else if (op == 1) {
        threshold = readNumber<int>(text);
        if (threshold < 0) throw std::invalid_argument("Длина не может быть отрицательной.");
    }
    return [=](const T& x) {
        if constexpr (std::is_same_v<T, int> || std::is_same_v<T, double>)
            return op == 0 ? x >= value : op == 1 ? x <= value : x != value;
        else if constexpr (std::is_same_v<T, std::complex<double>>)
            return op == 0 ? std::abs(x) >= threshold : op == 1 ? std::abs(x) <= threshold : std::abs(x) != threshold;
        else if constexpr (std::is_same_v<T, std::string>) {
            if (op == 0) return x.find(text) != std::string::npos;
            if (op == 2) return x != text;
            std::size_t length = 0;
            for (unsigned char c : x) if ((c & 0xC0) != 0x80) ++length;
            return static_cast<double>(length) >= threshold;
        } else if constexpr (std::is_same_v<T, Function>) {
            return op == 2 ? x == value : op == 0 ? x(0) >= threshold : x(0) <= threshold;
        } else return op == 2 ? x.name.find(text) != std::string::npos
                             : op == 0 ? x.id >= threshold : x.id <= threshold;
    };
}

std::string pathFor(std::size_t index) {
    std::string path;
    while (index) { path.push_back(static_cast<char>('1' + (index - 1) % 3)); index = (index - 1) / 3; }
    std::reverse(path.begin(), path.end());
    return path;
}

template<class T> T generatedValue(int n, std::mt19937& random) {
    if constexpr (std::is_same_v<T, int>) return n;
    else if constexpr (std::is_same_v<T, double>) return n / 10.0;
    else if constexpr (std::is_same_v<T, std::complex<double>>) return {n / 10.0, n / 20.0};
    else if constexpr (std::is_same_v<T, std::string>) return "строка " + std::to_string(n);
    else if constexpr (std::is_same_v<T, Function>) {
        const Function functions[] = {inc1, twice, square, negate}; return functions[random() % 4];
    } else {
        T p; p.id = checkedInt(n < 0 ? -static_cast<long long>(n) : n);
        p.name = "Участник " + std::to_string(p.id); p.birthDate = "2000-01-01";
        if constexpr (std::is_same_v<T, Teacher>) p.subject = "Информатика";
        return p;
    }
}
} // namespace

Workspace::Workspace() = default;
void Workspace::setType(DataType type) {
    // Новый вариант создаётся до замены старых данных.
    Data next;
    switch (type) {
    case DataType::Integer: next.emplace<TreePair<int>>(); break;
    case DataType::Real: next.emplace<TreePair<double>>(); break;
    case DataType::Complex: next.emplace<TreePair<std::complex<double>>>(); break;
    case DataType::String: next.emplace<TreePair<std::string>>(); break;
    case DataType::Function: next.emplace<TreePair<Function>>(); break;
    case DataType::Student: next.emplace<TreePair<Student>>(); break;
    case DataType::Teacher: next.emplace<TreePair<Teacher>>(); break;
    default: throw std::invalid_argument("Неизвестный тип.");
    }
    data_ = std::move(next); type_ = type; active_ = 0;
}
void Workspace::setActive(int i) {
    if (i < 0 || i > 1) throw std::out_of_range("Индекс дерева: 0 или 1.");
    active_ = i;
}
std::size_t Workspace::size() const { return std::visit([&](const auto& p) { return p.trees[active_].Size(); }, data_); }
std::size_t Workspace::otherSize() const { return std::visit([&](const auto& p) { return p.trees[1-active_].Size(); }, data_); }
void Workspace::clear() { std::visit([&](auto& p) { p.trees[active_].Clear(); }, data_); }
void Workspace::setRoot(const std::string& s) {
    std::visit([&](auto& p) { p.trees[active_].SetRoot(Codec<ValueOf<decltype(p)>>::parse(s)); }, data_);
}
void Workspace::insert(const std::string& path, int slot, const std::string& s) {
    std::visit([&](auto& p) { p.trees[active_].Insert(path, static_cast<std::size_t>(slot), Codec<ValueOf<decltype(p)>>::parse(s)); }, data_);
}
void Workspace::setValue(const std::string& path, const std::string& s) {
    std::visit([&](auto& p) { p.trees[active_].At(path) = Codec<ValueOf<decltype(p)>>::parse(s); }, data_);
}
std::string Workspace::at(const std::string& path, const std::string& relative, bool useRelative) const {
    return std::visit([&](const auto& p) {
        return Codec<ValueOf<decltype(p)>>::text(useRelative ? p.trees[active_].AtRelative(path, relative) : p.trees[active_].At(path));
    }, data_);
}
bool Workspace::contains(const std::string& s) const {
    return std::visit([&](const auto& p) { return p.trees[active_].Contains(Codec<ValueOf<decltype(p)>>::parse(s)); }, data_);
}
bool Workspace::removeValue(const std::string& s) {
    return std::visit([&](auto& p) { return p.trees[active_].Remove(Codec<ValueOf<decltype(p)>>::parse(s)); }, data_);
}
std::size_t Workspace::removeSubtree(const std::string& path) {
    return std::visit([&](auto& p) { return p.trees[active_].RemoveSubtree(path); }, data_);
}
void Workspace::extract(const std::string& query, bool byValue) {
    std::visit([&](auto& p) {
        p.trees[1-active_] = byValue ? p.trees[active_].ExtractSubtree(Codec<ValueOf<decltype(p)>>::parse(query))
                                   : p.trees[active_].ExtractSubtreeAt(query);
    }, data_);
}
void Workspace::merge(const std::string& parent, int slot) {
    std::visit([&](auto& p) { p.trees[active_].Merge(parent, static_cast<std::size_t>(slot), p.trees[1-active_]); }, data_);
}
void Workspace::copyToOther() { std::visit([&](auto& p) { p.trees[1-active_] = p.trees[active_]; }, data_); }
void Workspace::swapTrees() { std::visit([](auto& p) { p.trees[0].Swap(p.trees[1]); }, data_); }
bool Workspace::containsOther() const { return std::visit([&](const auto& p) { return p.trees[active_].ContainsSubtree(p.trees[1-active_]); }, data_); }
bool Workspace::equal() const { return std::visit([](const auto& p) { return p.trees[0] == p.trees[1]; }, data_); }
std::string Workspace::traverse(const std::string& order) const {
    return std::visit([&](const auto& p) {
        std::string result;
        p.trees[active_].Traverse([&](const auto& x) {
            if (!result.empty()) result += " → ";
            result += Codec<ValueOf<decltype(p)>>::text(x);
        }, order);
        return result.empty() ? std::string("Пустое дерево") : result;
    }, data_);
}
std::string Workspace::serialize(Storage mode, const std::string& order, const std::string& format) const {
    return std::visit([&](const auto& p) {
        auto encoder = [](const auto& x) { return Codec<ValueOf<decltype(p)>>::text(x); };
        switch (mode) {
        case Storage::Traversal: return p.trees[active_].ToStringWith(encoder, order);
        case Storage::Format: return p.trees[active_].ToStringFormatWith(format, encoder);
        case Storage::ParentList: return p.trees[active_].ToParentListWith(encoder);
        }
        throw std::invalid_argument("Неизвестный формат.");
    }, data_);
}
void Workspace::deserialize(const std::string& text, Storage mode, const std::string& order, const std::string& format) {
    std::visit([&](auto& p) {
        using T = ValueOf<decltype(p)>;
        auto decoder = [](std::string_view s) { return Codec<T>::parse(s); };
        TernaryTree<T> result;
        switch (mode) {
        case Storage::Traversal: result = TernaryTree<T>::FromStringWith(text, decoder, order); break;
        case Storage::Format: result = TernaryTree<T>::FromStringFormatWith(text, format, decoder); break;
        case Storage::ParentList: result = TernaryTree<T>::FromParentListWith(text, decoder); break;
        default: throw std::invalid_argument("Неизвестный формат.");
        }
        p.trees[active_] = std::move(result);
    }, data_);
}
std::deque<ViewNode> Workspace::snapshot(std::size_t limit) const {
    // Используем открытый API дерева: ни указатели Node, ни внутренности дерева UI не меняет.
    const std::string list = serialize(Storage::ParentList, "K123", "");
    std::istringstream in(list);
    std::size_t count = 0; in >> count;
    std::deque<ViewNode> result;
    for (std::size_t i = 0; i < std::min(count, limit); ++i) {
        ViewNode node;
        std::size_t bytes = 0; char colon = 0;
        in >> node.id >> node.parent >> node.slot >> bytes;
        in.get(colon);
        if (!in || colon != ':') throw std::logic_error("Некорректный снимок дерева.");
        node.value.resize(bytes);
        in.read(node.value.data(), static_cast<std::streamsize>(bytes));
        if (!in) throw std::logic_error("Неполный снимок дерева.");
        node.path = node.parent ? result.at(node.parent - 1).path + std::to_string(node.slot) : "";
        result.push_back(std::move(node));
    }
    return result;
}
std::string Workspace::hint() const {
    switch (type_) {
    case DataType::Integer: return "Целое число: 42";
    case DataType::Real: return "Вещественное число с точкой: 2.5";
    case DataType::Complex: return "Комплексное число: (2.5,-3)";
    case DataType::String: return "Любая строка UTF-8, в том числе пустая";
    case DataType::Function: return "inc1, twice, square, negate";
    case DataType::Student: return "12 | Иван Иванов | 2005-02-28";
    case DataType::Teacher: return "7 | Анна Петрова | 1980-03-12 | Информатика";
    }
    return {};
}
std::deque<std::string> Workspace::mapNames() const {
    if (type_ == DataType::Function) return {"Заменить функцией"};
    if (type_ == DataType::String) return {"Заменить строкой", "Добавить префикс", "Добавить суффикс"};
    if (type_ == DataType::Student || type_ == DataType::Teacher)
        return {"Заменить записью", "Прибавить к ID", "Добавить к ФИО"};
    return {"Заменить значением", "Прибавить", "Умножить"};
}
std::deque<std::string> Workspace::filterNames() const {
    if (type_ == DataType::String) return {"Содержит текст", "Длина не меньше", "Не равна строке"};
    if (type_ == DataType::Complex) return {"Модуль ≥", "Модуль ≤", "Модуль ≠"};
    if (type_ == DataType::Function) return {"f(0) ≥", "f(0) ≤", "Функция равна"};
    if (type_ == DataType::Student || type_ == DataType::Teacher) return {"ID ≥", "ID ≤", "ФИО содержит"};
    return {"Значение ≥", "Значение ≤", "Значение ≠"};
}
void Workspace::mapToOther(int op, const std::string& parameter) {
    std::visit([&](auto& p) { p.trees[1-active_] = p.trees[active_].Map(makeMap<ValueOf<decltype(p)>>(op, parameter)); }, data_);
}
void Workspace::filterToOther(int op, const std::string& parameter) {
    std::visit([&](auto& p) { p.trees[1-active_] = p.trees[active_].Where(makeFilter<ValueOf<decltype(p)>>(op, parameter)); }, data_);
}
std::string Workspace::reduce(const std::string& parameter, const std::string& order) const {
    return std::visit([&](const auto& p) -> std::string {
        using T = ValueOf<decltype(p)>;
        const auto& tree = p.trees[active_];
        if constexpr (std::is_same_v<T, double>) {
            return Codec<double>::text(tree.Reduce(0.0, [](double sum, double x) { return checkedReal(sum + x); }, order));
        } else if constexpr (std::is_same_v<T, std::complex<double>>) {
            return Codec<T>::text(tree.Reduce(T{}, [](T sum, T x) { return checkedComplex(sum + x); }, order));
        } else if constexpr (std::is_same_v<T, std::string>) {
            bool first = true;
            return tree.Reduce(std::string{}, [&](std::string sum, const std::string& x) {
                if (!first) sum += parameter;
                first = false; return sum + x;
            }, order);
        } else {
            int argument = 0;
            if constexpr (std::is_same_v<T, Function>) argument = readNumber<int>(parameter);
            const long long sum = tree.Reduce(0LL, [&](long long total, const T& x) {
                long long value;
                if constexpr (std::is_same_v<T, int>) value = x;
                else if constexpr (std::is_same_v<T, Function>) value = x(argument);
                else value = x.id;
                if ((value > 0 && total > std::numeric_limits<long long>::max() - value) ||
                    (value < 0 && total < std::numeric_limits<long long>::min() - value))
                    throw std::overflow_error("Переполнение суммы.");
                return total + value;
            }, order);
            return std::to_string(sum);
        }
    }, data_);
}
void Workspace::generate(std::size_t count, int minimum, int maximum, unsigned seed) {
    if (count > 100000 || minimum > maximum)
        throw std::invalid_argument("Количество: 0–100000; минимум не больше максимума.");
    std::mt19937 random(seed);
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    std::visit([&](auto& p) {
        using T = ValueOf<decltype(p)>;
        TernaryTree<T> result;
        for (std::size_t i = 0; i < count; ++i) {
            const T value = generatedValue<T>(distribution(random), random);
            if (i == 0) result.SetRoot(value);
            else result.Insert(pathFor((i - 1) / 3), (i - 1) % 3 + 1, value);
        }
        p.trees[active_] = std::move(result);
    }, data_);
}
} // namespace lab
