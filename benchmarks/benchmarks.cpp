#include "TernaryTree.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <fstream>
#include <iostream>
#include <locale>

namespace {
using Clock=std::chrono::steady_clock;
constexpr std::size_t operations=8;
const std::array<const char*,operations> names={"build","find_missing","map","where","reduce","serialize","deserialize","clear"};
const std::array<const char*,operations> colors={"#205cad","#d45b31","#27885f","#9161b1","#b29121","#168995","#d44571","#667287"};
struct Row {std::size_t count; std::array<double,operations> ms;};
volatile long long checksum=0;
template<class F> double measure(F&& f){const auto begin=Clock::now();f();return std::chrono::duration<double,std::milli>(Clock::now()-begin).count();}
std::string pathFor(std::size_t i){std::string p;while(i){p.push_back(static_cast<char>('1'+(i-1)%3));i=(i-1)/3;}std::reverse(p.begin(),p.end());return p;}
std::size_t number(const std::string& s){
    if(s.empty() || s.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("Expected unsigned size: "+s);
    std::size_t used=0;auto n=std::stoull(s,&used);if(used!=s.size() || n>std::numeric_limits<std::size_t>::max())throw std::invalid_argument("Size overflow");
    return static_cast<std::size_t>(n);
}
Row benchmark(std::size_t count,int repeats){
    std::array<std::deque<double>,operations> trials;
    for(int r=0;r<repeats;++r){
        TernaryTree<int> tree;
        trials[0].push_back(measure([&]{
            for(std::size_t i=0;i<count;++i){
                int x=static_cast<int>(i%1000);
                if(i==0)tree.SetRoot(x);else tree.Insert(pathFor((i-1)/3),(i-1)%3+1,x);
            }
        }));
        trials[1].push_back(measure([&]{checksum+=tree.Contains(-1);}));
        TernaryTree<int> result;
        trials[2].push_back(measure([&]{result=tree.Map([](int x){return x+1;});}));checksum+=static_cast<long long>(result.Size());result.Clear();
        trials[3].push_back(measure([&]{result=tree.Where([](int x){return x%2==0;});}));checksum+=static_cast<long long>(result.Size());result.Clear();
        trials[4].push_back(measure([&]{checksum+=tree.Reduce(0LL,[](long long a,int x){return a+x;});}));
        std::string text;
        trials[5].push_back(measure([&]{text=tree.ToString();}));checksum+=static_cast<long long>(text.size());
        trials[6].push_back(measure([&]{result=TernaryTree<int>::FromString(text);}));
        if(result.Size()!=count)throw std::runtime_error("Roundtrip size mismatch");
        result.Clear();
        trials[7].push_back(measure([&]{tree.Clear();}));
    }
    Row row{count,{}};
    for(std::size_t op=0;op<operations;++op){
        auto& times=trials[op];std::sort(times.begin(),times.end());
        const auto middle=times.size()/2;
        row.ms[op]=times.size()%2 ? times[middle] : (times[middle-1]+times[middle])/2;
    }
    return row;
}
void writeSvg(const std::string& path,const std::deque<Row>& rows){
    std::ofstream out(path); if(!out)throw std::runtime_error("Cannot write "+path);out.imbue(std::locale::classic());
    // Логарифмические оси позволяют сравнить размеры, различающиеся на порядки.
    double xmin=std::log10(static_cast<double>(rows.front().count)),xmax=std::log10(static_cast<double>(rows.back().count));
    if(xmin==xmax){xmin-=0.5;xmax+=0.5;}
    double low=1000000,high=0.000001;
    for(const auto& r:rows)for(double v:r.ms){low=std::min(low,std::max(v,0.000001));high=std::max(high,v);}
    double ymin=std::floor(std::log10(low)),ymax=std::ceil(std::log10(high));if(ymin==ymax)ymax+=1;
    auto x=[&](std::size_t n){return 90+760*(std::log10(static_cast<double>(n))-xmin)/(xmax-xmin);};
    auto y=[&](double t){return 500-400*(std::log10(std::max(t,0.000001))-ymin)/(ymax-ymin);};
    out<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1120\" height=\"610\" viewBox=\"0 0 1120 610\"><rect width=\"1120\" height=\"610\" fill=\"white\"/><g font-family=\"sans-serif\" fill=\"#26344a\">\n";
    out<<"<text x=\"60\" y=\"35\" font-size=\"24\">Ternary tree: median execution time</text><text x=\"60\" y=\"62\">Logarithmic axes; Release build recommended</text>\n";
    for(double e=ymin;e<=ymax;++e){double py=y(std::pow(10,e));out<<"<line x1=\"90\" y1=\""<<py<<"\" x2=\"850\" y2=\""<<py<<"\" stroke=\"#e1e6ed\"/><text x=\"15\" y=\""<<py+5<<"\">"<<std::pow(10,e)<<" ms</text>\n";}
    for(const auto& r:rows)out<<"<text x=\""<<x(r.count)<<"\" y=\"530\" text-anchor=\"middle\">"<<r.count<<"</text>\n";
    out<<"<line x1=\"90\" y1=\"100\" x2=\"90\" y2=\"500\" stroke=\"#526178\"/><line x1=\"90\" y1=\"500\" x2=\"850\" y2=\"500\" stroke=\"#526178\"/><text x=\"470\" y=\"570\" text-anchor=\"middle\">Number of nodes</text>\n";
    for(std::size_t op=0;op<operations;++op){
        out<<"<polyline fill=\"none\" stroke=\""<<colors[op]<<"\" stroke-width=\"2\" points=\"";
        for(const auto& r:rows) out<<x(r.count)<<','<<y(r.ms[op])<<' ';
        out<<"\"/>\n";
        for(const auto& r:rows)out<<"<circle cx=\""<<x(r.count)<<"\" cy=\""<<y(r.ms[op])<<"\" r=\"3\" fill=\""<<colors[op]<<"\"/>\n";
        out<<"<rect x=\"885\" y=\""<<110+op*38<<"\" width=\"18\" height=\"4\" fill=\""<<colors[op]<<"\"/><text x=\"915\" y=\""<<117+op*38<<"\">"<<names[op]<<"</text>\n";
    }
    out<<"</g></svg>\n";if(!out)throw std::runtime_error("SVG write failed");
}
}
int main(int argc,char** argv){
    try{
        std::deque<std::size_t> sizes;std::string output="benchmark";int repeats=3;
        for(int i=1;i<argc;++i){const std::string arg=argv[i];
            if(arg=="--output" && i+1<argc)output=argv[++i];
            else if(arg=="--repeats" && i+1<argc){auto n=number(argv[++i]);if(n<1||n>99)throw std::invalid_argument("Repeats: 1..99");repeats=static_cast<int>(n);}
            else if(arg=="--sizes"){while(i+1<argc && std::string(argv[i+1]).rfind("--",0)!=0)sizes.push_back(number(argv[++i]));}
            else if(arg=="--help"){std::cout<<"tree_benchmarks [--sizes 1000 10000 100000] [--repeats 3] [--output benchmark]\n";return 0;}
            else throw std::invalid_argument("Unknown or incomplete option: "+arg);
        }
        if(sizes.empty())sizes={1000,10000,100000};
        for(auto n:sizes)if(n==0 || n>100000000)throw std::invalid_argument("Node counts: 1..100000000");
        std::sort(sizes.begin(),sizes.end());sizes.erase(std::unique(sizes.begin(),sizes.end()),sizes.end());
        std::ofstream csv(output+".csv");if(!csv)throw std::runtime_error("Cannot open output CSV");csv.imbue(std::locale::classic());
        csv<<"nodes";for(const auto* name:names)csv<<','<<name<<"_ms";csv<<'\n';
        std::deque<Row> rows;
        for(auto n:sizes){std::cout<<"Benchmark "<<n<<" nodes, "<<repeats<<" trial(s)..."<<std::endl;auto row=benchmark(n,repeats);rows.push_back(row);
            csv<<n;for(double t:row.ms)csv<<','<<t;csv<<'\n';csv.flush();if(!csv)throw std::runtime_error("CSV write failed");
        }
        writeSvg(output+".svg",rows);std::cout<<"Saved "<<output<<".csv and "<<output<<".svg\n";return 0;
    }catch(const std::exception& error){std::cerr<<"Error: "<<error.what()<<'\n';return 1;}
}
