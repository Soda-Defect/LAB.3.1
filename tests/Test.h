#pragma once
#include <deque>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace test {
struct Case { const char* name; void (*run)(); };
inline std::deque<Case>& cases() { static std::deque<Case> all; return all; }
struct Register { Register(const char* name, void (*run)()) { cases().push_back({name,run}); } };
inline void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::ostringstream message; message << file << ':' << line << ": " << expression;
        throw std::runtime_error(message.str());
    }
}
template<class Exception, class Action> void throws(Action&& action, const char* expr, const char* file, int line) {
    try { action(); }
    catch (const Exception&) { return; }
    catch (...) { check(false, "Unexpected exception type", file, line); }
    check(false, expr, file, line);
}
} // namespace test
#define JOIN_IMPL(a,b) a##b
#define JOIN(a,b) JOIN_IMPL(a,b)
#define TEST(name) static void JOIN(test_,__LINE__)(); \
    static test::Register JOIN(reg_,__LINE__)(name, JOIN(test_,__LINE__)); \
    static void JOIN(test_,__LINE__)()
#define CHECK(expr) test::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define THROWS_AS(expr, exception) test::throws<exception>([&] { (void)(expr); }, #expr, __FILE__, __LINE__)
