#include "Test.h"
#include "Workspace.h"

using namespace lab;
namespace {
constexpr const char* format="{K}(1)[2]{3}";
std::string encoded(const Workspace& w) {return w.serialize(Storage::Traversal,"K123",format);}
}

TEST("Workspace validates indexes, input values and child slots") {
    Workspace w;
    THROWS_AS(w.setActive(2),std::out_of_range); THROWS_AS(w.setType(static_cast<DataType>(9)),std::invalid_argument);
    THROWS_AS(w.setRoot("3.5"),std::invalid_argument); THROWS_AS(w.setRoot("2147483648"),std::invalid_argument);
    w.setRoot("10"); w.insert("",2,"20");
    THROWS_AS(w.insert("",-1,"1"),std::out_of_range); CHECK(w.size()==2);
    CHECK(w.at("2")=="20"); CHECK(w.at("2","^",true)=="10");
    CHECK(w.contains("10")); CHECK(!w.contains("99"));
}
TEST("Workspace operations keep both trees independent") {
    Workspace w; w.setRoot("1"); w.insert("",1,"2"); w.copyToOther();
    CHECK(w.equal()); w.setValue("1","3"); CHECK(!w.equal());
    w.setActive(1); CHECK(w.at("1")=="2"); w.setActive(0);
    w.extract("1",false); CHECK(w.otherSize()==1); CHECK(w.containsOther());
    w.merge("",3); CHECK(w.at("3")=="3"); CHECK(w.size()==3);
    w.swapTrees(); CHECK(w.size()==1); CHECK(w.otherSize()==3);
}
TEST("Workspace serialization failures preserve the destination") {
    Workspace w; w.setRoot("9"); const auto before=encoded(w);
    THROWS_AS(w.deserialize("broken",Storage::Traversal,"K123",format),std::invalid_argument);
    THROWS_AS(w.deserialize("1\n1 0 0 1:x\n",Storage::ParentList,"K123",format),std::invalid_argument);
    CHECK(encoded(w)==before);
}
TEST("Every UI data type roundtrips all three storage modes") {
    const char* values[]={"-15","0.125","(2.5,-3)","Привет #():","square",
                          "42 | Иван Иванов | 2004-02-29","7 | Анна Петрова | 1980-01-12 | Математика"};
    for(int type=0;type<7;++type) {
        Workspace w; w.setType(static_cast<DataType>(type)); w.setRoot(values[type]);
        w.insert("",3,values[type]);
        for(auto mode:{Storage::Traversal,Storage::Format,Storage::ParentList}) {
            auto text=w.serialize(mode,"23K1",format);
            Workspace copy; copy.setType(static_cast<DataType>(type)); copy.deserialize(text,mode,"23K1",format);
            CHECK(copy.at("")==w.at("")); CHECK(copy.at("3")==w.at("3")); CHECK(copy.size()==2);
        }
    }
}
TEST("Numeric transformations and reduction produce expected results") {
    Workspace w; w.setRoot("2"); w.insert("",2,"3"); w.mapToOther(2,"4");
    CHECK(w.reduce("","K123")=="5"); w.setActive(1); CHECK(w.at("")=="8"); CHECK(w.at("2")=="12");
    CHECK(w.reduce("","K123")=="20"); w.filterToOther(0,"10"); w.setActive(0);
    CHECK(w.size()==1); CHECK(w.at("")=="12");
}
TEST("Integer overflow in map preserves both trees") {
    Workspace w; w.setRoot("2147483647"); w.copyToOther();
    THROWS_AS(w.mapToOther(1,"1"),std::overflow_error); CHECK(w.equal()); CHECK(w.size()==1);
    THROWS_AS(w.mapToOther(2,"2"),std::overflow_error); CHECK(w.equal());
}
TEST("Finite real and complex values and arithmetic are checked") {
    Workspace w; w.setType(DataType::Real);
    for(const auto* text:{"nan","inf","1e500","1.5junk"}) THROWS_AS(w.setRoot(text),std::invalid_argument);
    w.setRoot("1e308"); THROWS_AS(w.mapToOther(2,"10"),std::overflow_error); CHECK(w.otherSize()==0);
    w.setType(DataType::Complex); w.setRoot("(3,4)"); w.filterToOther(0,"5"); CHECK(w.equal());
    w.mapToOther(2,"(0,1)"); w.setActive(1); CHECK(w.at("")=="(-4,3)");
}
TEST("Unicode string transforms and character length filtering") {
    Workspace w; w.setType(DataType::String); w.setRoot("ёж"); w.insert("",1,"кот");
    w.filterToOther(1,"3"); w.setActive(1); CHECK(w.size()==1); CHECK(w.at("")=="кот");
    w.setActive(0); w.mapToOther(1,"мой "); w.setActive(1); CHECK(w.at("")=="мой ёж");
    CHECK(w.reduce(" / ","K123")=="мой ёж / мой кот");
    w.setType(DataType::String); w.setRoot(""); w.insert("",1,""); CHECK(w.reduce("-","K123")=="-");
}
TEST("Function catalog evaluates, serializes and checks overflow") {
    Workspace w; w.setType(DataType::Function); w.setRoot("inc1"); w.insert("",1,"square");
    CHECK(w.reduce("3","K123")=="13"); THROWS_AS(w.reduce("50000","K123"),std::overflow_error);
    w.filterToOther(2,"square"); w.setActive(1); CHECK(w.size()==1); CHECK(w.at("")=="square");
    THROWS_AS(w.setValue("","unknown"),std::invalid_argument); CHECK(w.at("")=="square");
}
TEST("Person codecs validate calendar dates and required fields") {
    CHECK(validDate("2000-02-29")); CHECK(!validDate("1900-02-29")); CHECK(!validDate("2024-04-31"));
    for(const auto* text:{"1 | Name | 2001-02-29", "-1 | Name | 2000-01-01", "1 | | 2000-01-01",
                         "1 | Name | 2000-01-01 | extra", "1 | Name | 2000-01-01 |"})
        THROWS_AS(Codec<Student>::parse(text),std::invalid_argument);
    THROWS_AS(Codec<Teacher>::parse("1 | Name | 2000-01-01"),std::invalid_argument);
}
TEST("Person map and filter preserve all other fields") {
    Workspace w; w.setType(DataType::Student); w.setRoot("12 | Иван | 2000-02-29");
    w.mapToOther(1,"5"); w.setActive(1); CHECK(w.at("")=="17 | Иван | 2000-02-29");
    CHECK(w.reduce("","K123")=="17"); w.filterToOther(2,"Иван"); CHECK(w.equal());
    w.mapToOther(2," Петров"); w.setActive(0); CHECK(w.at("")=="17 | Иван Петров | 2000-02-29");
}
TEST("Generation is reproducible and supports every UI type") {
    for(int type=0;type<7;++type) {
        Workspace a,b; a.setType(static_cast<DataType>(type)); b.setType(static_cast<DataType>(type));
        a.generate(100,-10,20,42); b.generate(100,-10,20,42); CHECK(encoded(a)==encoded(b)); CHECK(a.size()==100);
        a.generate(0,0,1,42); CHECK(a.size()==0);
    }
}
TEST("Invalid generation parameters preserve existing values") {
    Workspace w; w.setRoot("8"); const auto before=encoded(w);
    THROWS_AS(w.generate(1,5,1,0),std::invalid_argument); THROWS_AS(w.generate(100001,0,1,0),std::invalid_argument);
    CHECK(encoded(w)==before);
}
TEST("Snapshot preserves paths, empty and multiline values and preview limit") {
    Workspace w; w.setType(DataType::String); w.setRoot("root\n#"); w.insert("",3,""); w.insert("3",2,"Привет");
    const auto nodes=w.snapshot(); CHECK(nodes.size()==3); CHECK(nodes[0].value=="root\n#");
    CHECK(nodes[1].path=="3"); CHECK(nodes[1].value.empty()); CHECK(nodes[2].path=="32");
    CHECK(w.snapshot(2).size()==2); CHECK(w.snapshot(0).empty());
}
TEST("Type switch resets both trees and active index") {
    Workspace w; w.setRoot("1"); w.copyToOther(); w.setActive(1); w.setType(DataType::String);
    CHECK(w.size()==0); CHECK(w.otherSize()==0); CHECK(w.active()==0); CHECK(w.type()==DataType::String);
}
