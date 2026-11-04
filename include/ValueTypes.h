#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace lab {

// Учебные типы хранят идентификатор, ФИО и дату рождения.
struct Student {
    int id{};
    std::string name, birthDate;
    bool operator==(const Student& b) const {
        return id == b.id && name == b.name && birthDate == b.birthDate;
    }
};
struct Teacher {
    int id{};
    std::string name, birthDate, subject;
    bool operator==(const Teacher& b) const {
        return id == b.id && name == b.name && birthDate == b.birthDate && subject == b.subject;
    }
};

inline int checkedInt(long long value) {
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        throw std::overflow_error("Результат выходит за диапазон int.");
    return static_cast<int>(value);
}
inline double checkedReal(double value) {
    if (!std::isfinite(value)) throw std::overflow_error("Требуется конечное число.");
    return value;
}
inline std::complex<double> checkedComplex(std::complex<double> value) {
    checkedReal(value.real()); checkedReal(value.imag()); return value;
}

using Function = int (*)(int);
inline int inc1(int x) { return checkedInt(static_cast<long long>(x) + 1); }
inline int twice(int x) { return checkedInt(static_cast<long long>(x) * 2); }
inline int square(int x) { return checkedInt(static_cast<long long>(x) * x); }
inline int negate(int x) { return checkedInt(-static_cast<long long>(x)); }

inline std::string trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return std::string(s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1));
}

template<class T> T readNumber(std::string_view text) {
    std::istringstream in{std::string(text)};
    in.imbue(std::locale::classic());
    T value{};
    if (!(in >> value)) throw std::invalid_argument("Некорректное число.");
    in >> std::ws;
    if (!in.eof()) throw std::invalid_argument("Лишние символы после числа.");
    if constexpr (std::is_same_v<T, double>) checkedReal(value);
    return value;
}

inline bool validDate(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (std::size_t i = 0; i < s.size(); ++i)
        if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return false;
    const int y = std::stoi(s.substr(0, 4)), m = std::stoi(s.substr(5, 2)), d = std::stoi(s.substr(8, 2));
    if (y == 0 || m < 1 || m > 12) return false;
    const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return d >= 1 && d <= days[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

template<class T> struct Codec {
    static T parse(std::string_view s) { return readNumber<T>(s); }
    static std::string text(const T& x) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10) << x;
        return out.str();
    }
};
template<> struct Codec<std::string> {
    static std::string parse(std::string_view s) { return std::string(s); }
    static std::string text(const std::string& s) { return s; }
};
template<> struct Codec<std::complex<double>> {
    // Запись: (действительная,мнимая), например (2.5,-3).
    static std::complex<double> parse(std::string_view s) {
        std::istringstream in{std::string(s)};
        in.imbue(std::locale::classic());
        std::complex<double> x;
        if (!(in >> x)) throw std::invalid_argument("Введите комплексное число: (2.5,-3).");
        in >> std::ws;
        if (!in.eof()) throw std::invalid_argument("Лишние символы в комплексном числе.");
        return checkedComplex(x);
    }
    static std::string text(const std::complex<double>& x) {
        return "(" + Codec<double>::text(x.real()) + "," + Codec<double>::text(x.imag()) + ")";
    }
};
template<> struct Codec<Function> {
    static Function parse(std::string_view s) {
        const std::string name = trim(s);
        if (name == "inc1") return inc1;
        if (name == "twice") return twice;
        if (name == "square") return square;
        if (name == "negate") return negate;
        throw std::invalid_argument("Функции: inc1, twice, square, negate.");
    }
    static std::string text(Function f) {
        if (f == inc1) return "inc1";
        if (f == twice) return "twice";
        if (f == square) return "square";
        if (f == negate) return "negate";
        throw std::invalid_argument("Неизвестная функция.");
    }
};

template<class T> T parsePerson(std::string_view s) {
    std::istringstream in{std::string(s)};
    std::string id, name, date, subject, extra;
    if (!std::getline(in, id, '|') || !std::getline(in, name, '|') || !std::getline(in, date, '|'))
        throw std::invalid_argument("Формат: ID | ФИО | ГГГГ-ММ-ДД.");
    T p; p.id = readNumber<int>(id); p.name = trim(name); p.birthDate = trim(date);
    if constexpr (std::is_same_v<T, Teacher>) {
        if (!std::getline(in, subject, '|')) throw std::invalid_argument("Укажите предмет после даты.");
        p.subject = trim(subject);
        if (p.subject.empty()) throw std::invalid_argument("Предмет не должен быть пустым.");
    }
    if (std::getline(in, extra, '|') || (!s.empty() && s.back() == '|'))
        throw std::invalid_argument("Лишние поля в записи.");
    if (p.id < 0 || p.name.empty() || !validDate(p.birthDate))
        throw std::invalid_argument("Проверьте ID, ФИО и дату рождения.");
    return p;
}
template<> struct Codec<Student> {
    static Student parse(std::string_view s) { return parsePerson<Student>(s); }
    static std::string text(const Student& p) {
        return std::to_string(p.id) + " | " + p.name + " | " + p.birthDate;
    }
};
template<> struct Codec<Teacher> {
    static Teacher parse(std::string_view s) { return parsePerson<Teacher>(s); }
    static std::string text(const Teacher& p) {
        return std::to_string(p.id) + " | " + p.name + " | " + p.birthDate + " | " + p.subject;
    }
};

} // namespace lab
