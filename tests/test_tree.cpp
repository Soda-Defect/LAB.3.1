#include "Test.h"
#include "TernaryTree.h"
#include <algorithm>
#include <complex>
#include <limits>

namespace {
TernaryTree<int> sample() {
    TernaryTree<int> t(10);
    t.Insert("",1,20); t.Insert("",2,30); t.Insert("",3,40);
    t.Insert("1",1,50); t.Insert("1",3,60); t.Insert("2",2,70);
    return t;
}
std::string values(const TernaryTree<int>& t, const std::string& order = "K123") {
    return t.Reduce(std::string{}, [](std::string out, int x) { return out + std::to_string(x) + ','; }, order);
}
struct ThrowCopy {
    int value{};
    inline static int live = 0;
    inline static int copiesLeft = -1;
    explicit ThrowCopy(int x = 0) : value(x) { ++live; }
    ThrowCopy(const ThrowCopy& x) : value(x.value) {
        if (copiesLeft == 0) throw std::runtime_error("copy failed");
        if (copiesLeft > 0) --copiesLeft;
        ++live;
    }
    ThrowCopy(ThrowCopy&& x) noexcept : value(x.value) { ++live; }
    ~ThrowCopy() { --live; }
    bool operator==(const ThrowCopy& x) const { return value == x.value; }
};
}

TEST("Empty tree invariants and safe clearing") {
    TernaryTree<int> t;
    CHECK(t.Empty()); CHECK(t.Size() == 0); CHECK(!t.Find(1)); CHECK(!t.Remove(1));
    CHECK(t.RemoveSubtree("") == 0); CHECK(t.ToString() == "#");
    CHECK(t.ToParentList() == "0\n"); CHECK(t.Reduce(7, [](int a,int b){return a+b;}) == 7);
    CHECK(t.Map([](int x){return x+1;}).Empty()); CHECK(t.Where([](int){return true;}).Empty());
    CHECK(t.ContainsSubtree(TernaryTree<int>{}));
    THROWS_AS(t.GetRoot(), std::out_of_range); THROWS_AS(t.At("1"), std::out_of_range);
    THROWS_AS(t.ExtractSubtree(1), std::out_of_range);
    t.Clear(); t.Clear(); CHECK(t.Size() == 0);
}
TEST("Root creation, duplicate root and child positions") {
    TernaryTree<int> t;
    CHECK(t.SetRoot(9) == 9); CHECK(t.Size() == 1); CHECK(t.GetRoot() == 9);
    THROWS_AS(t.SetRoot(4), std::logic_error);
    for (std::size_t i = 1; i <= 3; ++i) t.Insert("", i, static_cast<int>(i));
    CHECK(t.Size() == 4); CHECK(t.At("1") == 1); CHECK(t.At("3") == 3);
    THROWS_AS(t.Insert("",1,5), std::logic_error);
    THROWS_AS(t.Insert("",0,5), std::out_of_range); THROWS_AS(t.Insert("",4,5), std::out_of_range);
    CHECK(t.Size() == 4); CHECK(t.GetRoot() == 9);
}
TEST("Absolute and relative paths including const overloads") {
    auto t = sample(); t.At("13") = 61;
    const auto& c = t;
    CHECK(c.At("13") == 61); CHECK(c.AtRelative("11", "^3") == 61);
    CHECK(t.AtRelative("13", "^^2") == 30); CHECK(c.AtRelative("22", "^^") == 10);
    CHECK(c.AtRelative("", "") == 10);
    THROWS_AS(t.At("12"), std::out_of_range); THROWS_AS(c.AtRelative("", "^"), std::out_of_range);
    THROWS_AS(t.At("0"), std::invalid_argument); THROWS_AS(t.At("^"), std::invalid_argument);
    THROWS_AS(c.AtRelative("1", "4"), std::invalid_argument);
}
TEST("Search returns mutable first preorder occurrence") {
    auto t = sample(); t.Insert("3",1,20);
    int* result = t.Find(20); CHECK(result); *result = 21;
    CHECK(t.At("1") == 21); CHECK(t.At("31") == 20);
    const auto& c = t; CHECK(c.Find(20) && *c.Find(20) == 20); CHECK(!c.Contains(-1));
}
TEST("RemoveSubtree tracks size, detaches parent and permits reinsertion") {
    auto t = sample(); CHECK(t.RemoveSubtree("1") == 3); CHECK(t.Size() == 4);
    CHECK(!t.Contains(20)); CHECK(t.At("22") == 70);
    t.Insert("",1,99); CHECK(t.Size() == 5); CHECK(t.At("1") == 99);
    THROWS_AS(t.RemoveSubtree("11"), std::out_of_range);
    CHECK(t.RemoveSubtree("") == 5); CHECK(t.Empty());
}
TEST("Remove by value removes first subtree and preserves duplicate") {
    auto t = sample(); t.Insert("3",1,20);
    CHECK(t.Remove(20)); CHECK(t.Size() == 5); CHECK(t.At("31") == 20);
    CHECK(!t.Remove(999)); CHECK(t.Remove(10)); CHECK(t.Empty());
}
TEST("Copy construction and assignment make independent structures") {
    auto t = sample(); TernaryTree<int> copy(t); CHECK(copy == t);
    copy.At("11") = -5; CHECK(t.At("11") == 50); CHECK(copy != t);
    TernaryTree<int> assigned; assigned = t; assigned = assigned; CHECK(assigned == t);
    assigned.RemoveSubtree("2"); CHECK(t.Size() == 7);
}
TEST("Move construction, move assignment and swap") {
    auto t = sample(); TernaryTree<int> moved(std::move(t)); CHECK(t.Empty()); CHECK(t.Size() == 0);
    CHECK(moved.Size() == 7); t.SetRoot(8); t = std::move(moved); CHECK(t.Size() == 7); CHECK(moved.Empty());
    TernaryTree<int> one(1); t.Swap(one); CHECK(t.Size() == 1); CHECK(one.Size() == 7);
    TernaryTree<int>::Swap(t,one); CHECK(t.Size() == 7);
}
TEST("Different traversal orders have independently known results") {
    const auto t = sample();
    CHECK(values(t) == "10,20,50,60,30,70,40,");
    CHECK(values(t,"123K") == "50,60,20,70,30,40,10,");
    CHECK(values(t,"321K") == "40,70,30,60,50,20,10,");
    CHECK(values(t,"1K23") == "50,20,60,10,30,70,40,");
}
TEST("All 24 traversal permutations visit each node exactly once") {
    auto t = sample(); std::string order = "123K"; int count = 0;
    do {
        CHECK(t.Reduce(0, [](int a,int b){return a+b;},order) == 280);
        CHECK(t.Reduce(0, [](int a,int){return a+1;},order) == 7);
        CHECK(TernaryTree<int>::FromString(t.ToString(order),order) == t); ++count;
    } while (std::next_permutation(order.begin(),order.end()));
    CHECK(count == 24);
}
TEST("Invalid traversal orders are rejected even for empty trees") {
    TernaryTree<int> t;
    for (const auto* order : {"", "K12", "K1233", "KK23", "K124", "К123"}) {
        THROWS_AS(t.ToString(order), std::invalid_argument);
        THROWS_AS(t.Traverse([](int){},order), std::invalid_argument);
    }
}
TEST("Mutable traversal visits exactly once") {
    auto t = sample(); t.Traverse([](int& x){x*=2;},"2K31");
    CHECK(t.GetRoot() == 20); CHECK(t.At("22") == 140); CHECK(t.Size() == 7);
}
TEST("Map preserves gaps and source while transforming values") {
    const auto t = sample(); auto mapped = t.Map([](int x){return x+1;});
    CHECK(mapped.Size() == 7); CHECK(mapped.At("13") == 61); CHECK(t.At("13") == 60);
    THROWS_AS(mapped.At("12"), std::out_of_range);
}
TEST("Reduce respects order and noncommutative accumulator") {
    auto t = sample(); CHECK(values(t,"K321") == "10,40,30,70,20,60,50,");
    CHECK(t.Reduce(5LL,[](long long a,int x){return a+x;}) == 285);
}
TEST("Where compacts selected values in preorder and handles none/all") {
    auto t = sample(); auto filtered = t.Where([](int x){return x>=40;});
    CHECK(filtered.Size() == 4); CHECK(filtered.GetRoot() == 50);
    CHECK(filtered.At("1") == 60); CHECK(filtered.At("2") == 70); CHECK(filtered.At("3") == 40);
    CHECK(t.Where([](int){return false;}).Empty()); CHECK(t.Where([](int){return true;}).Size() == 7);
    CHECK(t.Size() == 7);
}
TEST("Extract by path and by value makes independent subtree") {
    const auto t = sample(); auto a = t.ExtractSubtree(20), b = t.ExtractSubtreeAt("1");
    CHECK(a == b); CHECK(a.Size() == 3); CHECK(a.At("3") == 60);
    a.GetRoot() = -1; CHECK(t.At("1") == 20);
    THROWS_AS(t.ExtractSubtree(-1), std::out_of_range);
}
TEST("ContainsSubtree compares values and exact child positions") {
    auto t = sample(); CHECK(t.ContainsSubtree(t)); CHECK(t.ContainsSubtree(t.ExtractSubtreeAt("1")));
    TernaryTree<int> wrong(20); wrong.Insert("",1,50); wrong.Insert("",2,60);
    CHECK(!t.ContainsSubtree(wrong)); CHECK(!t.ContainsSubtree(TernaryTree<int>(20)));
    CHECK(t.ContainsSubtree(TernaryTree<int>(60))); CHECK(!TernaryTree<int>{}.ContainsSubtree(t));
}
TEST("Merge copies source, rejects occupied slots and supports self merge") {
    TernaryTree<int> t(1); auto source = sample(); t.Merge("",2,source);
    CHECK(t.Size() == 8); CHECK(t.At("211") == 50); source.Clear(); CHECK(t.Size() == 8);
    THROWS_AS(t.Merge("",2,source), std::logic_error); CHECK(t.Size() == 8);
    t.Merge("",1,source); CHECK(t.Size() == 8);
    TernaryTree<int> self(9); self.Insert("",1,8); self.Merge("",3,self);
    CHECK(self.Size() == 4); CHECK(self.At("31") == 8);
}
TEST("Canonical serialization has an explicit expected form") {
    TernaryTree<int> t(7); t.Insert("",2,-8);
    CHECK(t.ToString() == "(1:7#(2:-8###)#)");
    CHECK(TernaryTree<int>::FromString("(1:7#(2:-8###)#)") == t);
}
TEST("String values preserve delimiters, UTF8, empty and embedded null bytes") {
    TernaryTree<std::string> t("#(): {К}\nПривет");
    t.Insert("",1,""); t.Insert("",3,std::string("a\0b",3));
    CHECK(TernaryTree<std::string>::FromString(t.ToString()) == t);
    CHECK(TernaryTree<std::string>::FromStringFormat(t.ToStringFormat("{К}(1)[2]{3}"),"{К}(1)[2]{3}") == t);
    CHECK(TernaryTree<std::string>::FromParentList(t.ToParentList()) == t);
}
TEST("Malformed canonical strings are rejected") {
    for (const auto* text : {"", "()", "x", "##", "(1:1##)", "(1:1####)", "(5:12###)",
                            "(1:x###)", "(1:1###)tail", "(999999999999999999999999:0###)", "(2:1x###)"})
        THROWS_AS(TernaryTree<int>::FromString(text), std::invalid_argument);
}
TEST("Formatted serialization supports all orders and escaped literals") {
    auto t = sample(); std::string order="123K";
    do {
        const std::string format = "<" + order.substr(0,1) + ">[" + order.substr(1,1) + "][" + order.substr(2,1) + "][" + order.substr(3,1) + "]";
        CHECK(TernaryTree<int>::FromStringFormat(t.ToStringFormat(format),format) == t);
    } while (std::next_permutation(order.begin(),order.end()));
    const std::string escaped = "\\K{K}(1)[2]{3}\\1";
    CHECK(TernaryTree<int>::FromStringFormat(t.ToStringFormat(escaped),escaped) == t);
    CHECK(TernaryTree<int>::FromStringFormat("#","{K}(1)[2]{3}").Empty());
}
TEST("Ambiguous, duplicate and incomplete formats are rejected") {
    auto t=sample();
    for (const auto* format : {"", "K12", "KK23", "1K23", "#K123", "K123\\"})
        THROWS_AS(t.ToStringFormat(format), std::invalid_argument);
    auto text=t.ToStringFormat("{K}(1)[2]{3}"); text.back()=']';
    THROWS_AS(TernaryTree<int>::FromStringFormat(text,"{K}(1)[2]{3}"), std::invalid_argument);
    THROWS_AS(TernaryTree<int>::FromStringFormat("{1:1}(#)[#]{#}x","{K}(1)[2]{3}"), std::invalid_argument);
}
TEST("Parent list roundtrip and records in arbitrary order") {
    auto t=sample(); CHECK(TernaryTree<int>::FromParentList(t.ToParentList()) == t);
    const auto parsed=TernaryTree<int>::FromParentList("3\n3 2 3 1:9\n1 2 1 1:7\n2 0 0 1:8\n");
    CHECK(parsed.GetRoot() == 8); CHECK(parsed.At("1") == 7); CHECK(parsed.At("3") == 9);
    CHECK(parsed.Size() == 3);
}
TEST("Parent list rejects duplicate IDs, cycles, disconnection and invalid slots") {
    const char* bad[] = {
        "", "0\nx", "1\n1 0 1 1:7\n", "2\n1 0 0 1:1\n1 1 1 1:2\n",
        "2\n1 0 0 1:1\n2 0 0 1:2\n", "2\n1 0 0 1:1\n2 1 4 1:2\n",
        "3\n1 0 0 1:1\n2 1 1 1:2\n3 1 1 1:3\n",
        "3\n1 0 0 1:1\n2 3 1 1:2\n3 2 1 1:3\n",
        "2\n1 0 0 1:1\n2 2 1 1:2\n", "2\n1 0 0 1:1\n2 3 1 1:2\n",
        "1\n1 0 0 3:12", "1\n1 0 0 1:1\nextra", "9999999999999999999999999999"
    };
    for (const auto* text : bad) THROWS_AS(TernaryTree<int>::FromParentList(text),std::invalid_argument);
}
TEST("Double, complex and char native codecs roundtrip") {
    TernaryTree<double> a(1.0/3); a.Insert("",3,-1e200);
    CHECK(TernaryTree<double>::FromString(a.ToString()) == a);
    TernaryTree<std::complex<double>> b({2.5,-3.125});
    CHECK(TernaryTree<std::complex<double>>::FromParentList(b.ToParentList()) == b);
    TernaryTree<char> c('#'); c.Insert("",2,':'); CHECK(TernaryTree<char>::FromString(c.ToString()) == c);
}
TEST("Custom encoder and decoder roundtrip user structures") {
    struct Item { int id; bool operator==(const Item& b) const {return id==b.id;} };
    TernaryTree<Item> t(Item{4}); t.Insert("",3,Item{8});
    auto enc=[](const Item& x){return std::to_string(x.id);};
    auto dec=[](std::string_view x){return Item{std::stoi(std::string(x))};};
    CHECK(TernaryTree<Item>::FromStringWith(t.ToStringWith(enc),dec) == t);
    CHECK(TernaryTree<Item>::FromStringFormatWith(t.ToStringFormatWith("{K}(1)[2]{3}",enc),"{K}(1)[2]{3}",dec) == t);
    CHECK(TernaryTree<Item>::FromParentListWith(t.ToParentListWith(enc),dec) == t);
}
TEST("Failed copies clean allocated nodes and preserve assignment destination") {
    CHECK(ThrowCopy::live == 0);
    {
        TernaryTree<ThrowCopy> t(ThrowCopy{1}); t.Insert("",1,ThrowCopy{2}); t.Insert("1",2,ThrowCopy{3});
        TernaryTree<ThrowCopy> dest(ThrowCopy{9}); const int before=ThrowCopy::live;
        ThrowCopy::copiesLeft=1;
        THROWS_AS(dest=t,std::runtime_error); CHECK(ThrowCopy::live==before); CHECK(dest.GetRoot().value==9);
        ThrowCopy::copiesLeft=-1;
    }
    CHECK(ThrowCopy::live == 0);
}
TEST("Throwing map predicate and decoder preserve source and release nodes") {
    auto t=sample(); const auto before=t.ToString();
    THROWS_AS(t.Map([](int x){if(x==60)throw std::runtime_error("map");return x;}),std::runtime_error);
    THROWS_AS(t.Where([](int x){if(x==60)throw std::runtime_error("filter");return true;}),std::runtime_error);
    auto decoder=[](std::string_view x){int n=std::stoi(std::string(x));if(n==60)throw std::runtime_error("decode");return n;};
    THROWS_AS(TernaryTree<int>::FromStringWith(before,decoder),std::runtime_error);
    THROWS_AS(TernaryTree<int>::FromStringFormatWith(t.ToStringFormat("{1}[2](3)K"),"{1}[2](3)K",decoder),std::runtime_error);
    THROWS_AS(TernaryTree<int>::FromParentListWith(t.ToParentList(),decoder),std::runtime_error);
    CHECK(t.ToString()==before);
}
TEST("Deep chain operations avoid recursive stack overflow") {
    // Создание длинной цепочки через линейный парсер, без квадратичного Insert по пути.
    constexpr std::size_t count=12000;
    std::string text;
    for(std::size_t i=0;i<count;++i) text+="(1:1";
    text+='#';
    for(std::size_t i=0;i<count;++i) text+="##)";
    auto t=TernaryTree<int>::FromString(text); CHECK(t.Size()==count);
    CHECK(t.Reduce(0LL,[](long long a,int x){return a+x;})==static_cast<long long>(count));
    auto copy=t; CHECK(copy==t); CHECK(t.ToString()==text);
    auto f=t.ToStringFormat("{1}K[2](3)");
    CHECK(TernaryTree<int>::FromStringFormat(f,"{1}K[2](3)")==t);
    CHECK(TernaryTree<int>::FromParentList(t.ToParentList())==t);
    copy.Clear(); CHECK(copy.Empty());
}
