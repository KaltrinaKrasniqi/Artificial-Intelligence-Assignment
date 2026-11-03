#include "solver.hpp"
#include "validator.hpp"
void tests();
int main(int argc,char** argv) {
    try {
        if(argc==2 && string(argv[1])=="--test") { tests(); return 0; }
        Metrics m; auto start=chrono::steady_clock::now();
        auto elapsed=[&](){return chrono::duration<double,milli>(chrono::steady_clock::now()-start).count();};
        if(argc==2 && string(argv[1])=="--linear-nine") {
            Schedule schedule;
            Status status=golfersLinearNine(2000000,schedule,m);
            double ms=elapsed();
            cout<<"Mode: restricted linear construction with DFS (9 weeks, not maximum)\n";
            if(status==Status::solved) {
                require(golfValid(schedule,8,4,9),"Linear schedule validation failed");
                for(size_t week=0;week<schedule.size();++week) {
                    cout<<"Week "<<week+1<<": ";
                    for(const auto& group:schedule[week]) {cout<<"[ ";for(int a:group)cout<<a<<' ';cout<<"] ";}
                    cout<<'\n';
                }
                cout<<"Validation: PASS\n";
            }
            metrics(m,ms); return report(status);
        }
        if(argc<4 || argc>6) { cerr<<"Usage: social_golfers <groups> <players/group> <weeks> [limit] [seed=1]\n"; return 2; }
            int g=integer(argv[1],1,16),p=integer(argv[2],1,8),w=integer(argv[3],1,16);
            if(g*p>64) throw invalid_argument("At most 64 players supported");
            Schedule s; Status status=golfers(g,p,w,argc>=5?integer(argv[4],1,2000000000):2000000,s,m,argc==6?static_cast<unsigned>(integer(argv[5],0,2147483647)):1);
            double ms=elapsed();
            if(g==8 && p==4 && w>=8) cout<<"Mode: structured DFS/backtracking, fixed constraint order, no MRV\n";
            if(status==Status::solved) {
                require(golfValid(s,g,p,w),"Golf validation failed");
                for(int i=0;i<w;++i) {cout<<"Week "<<i+1<<": ";for(auto group:s[i]) {cout<<"[ ";for(int a:group) cout<<a<<' ';cout<<"] ";}cout<<'\n';}
                cout<<"Validation: PASS\n";
                if(p>1 && w==(g*p-1)/(p-1))
                    cout<<"Optimality: PROVED (valid schedule reaches the mathematical upper bound)\n";
            }
            metrics(m,ms);return report(status);
    } catch(const invalid_argument& e) {cerr<<"Input error: "<<e.what()<<'\n';return 2;}
      catch(const out_of_range& e) {cerr<<"Input outside range: "<<e.what()<<'\n';return 2;}
      catch(const exception& e) {cerr<<e.what()<<'\n';return 3;}
}
