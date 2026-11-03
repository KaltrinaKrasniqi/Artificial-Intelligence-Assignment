#include "solver.hpp"
#include "validator.hpp"
void tests() {
    for(auto params:vector<array<int,3>>{{2,2,3},{3,2,3},{8,4,2}}) {
        Schedule s; Metrics m; int g=params[0],p=params[1],w=params[2];
        require(golfers(g,p,w,100000,s,m)==Status::solved && golfValid(s,g,p,w),"Golf solver failed");
        s[0][0][0]=s[0][0][1]; require(!golfValid(s,g,p,w),"Duplicate golfer accepted");
    }
    Schedule s; Metrics gm; require(golfers(2,2,4,1000,s,gm)==Status::unsat,"Golf bound failed");
    Schedule repeated={{{0,1},{2,3}},{{0,1},{2,3}}};
    require(!golfValid(repeated,2,2,2),"Repeated pair accepted");
    Metrics tiny; require(golfers(8,4,3,1,s,tiny)==Status::limit,"Golf limit ignored");

    // Pair-count bound is necessary but not sufficient: this case needs exhaustive search.
    Metrics exhaustive; Schedule none;
    require(golfers(2,3,2,100000,none,exhaustive)==Status::unsat && none.empty(),"Exhaustive UNSAT failed");
    Metrics reused; Schedule result;
    require(golfers(2,2,3,100000,result,reused)==Status::solved,"Initial solve failed");
    size_t visits=reused.visited;
    require(golfers(2,2,3,100000,result,reused)==Status::solved && reused.visited==visits,"Metrics not reset");
    require(golfers(16,4,1,100,result,reused)==Status::solved && golfValid(result,16,4,1),"Player 63 mask failed");
    bool rejected=false;
    try { golfers(0,4,3,100,result,reused); } catch(const invalid_argument&) { rejected=true; }
    require(rejected,"Invalid API parameters accepted");
    require(!golfValid({},0,4,3),"Invalid validator parameters accepted");

    Schedule ten; Metrics tenMetrics;
    require(golfers(8,4,10,2000000,ten,tenMetrics)==Status::solved,"Ten weeks not found");
    require(golfValid(ten,8,4,10),"Ten-week validation failed");
    set<pair<int,int>> allPairs;
    for(const auto& week:ten) for(const auto& group:week)
        for(int i=0;i<4;++i) for(int j=i+1;j<4;++j) allPairs.insert(minmax(group[i],group[j]));
    require(allPairs.size()==480,"Expected 480 distinct pairs");
    Schedule repeatedTen; Metrics repeatedMetrics;
    require(golfers(8,4,10,2000000,repeatedTen,repeatedMetrics)==Status::solved &&
            repeatedTen==ten && repeatedMetrics.visited==tenMetrics.visited,"Ten-week reproducibility failed");
    for(int weeks:{8,9}) {
        Schedule prefix; Metrics pm;
        require(golfers(8,4,weeks,2000000,prefix,pm)==Status::solved && golfValid(prefix,8,4,weeks),"Prefix failed");
    }
    Schedule interrupted; Metrics im;
    require(golfers(8,4,10,1000,interrupted,im)==Status::limit && interrupted.empty(),"Ten-week cutoff mishandled");
    require(im.visited==1000,"Node budget not enforced across both search phases");
    require(golfers(8,4,11,2000000,interrupted,im)==Status::unsat && interrupted.empty(),"Eleven-week bound failed");
    Schedule linear; Metrics lin;
    require(golfersLinearNine(2000000,linear,lin)==Status::solved && golfValid(linear,8,4,9),"Linear seed construction failed");
    for(unsigned seed:{0u,1u,42u}) {
        Schedule alternative; Metrics am;
        require(golfers(3,2,3,2000000,alternative,am,seed)==Status::solved && golfValid(alternative,3,2,3),"Seed changed feasibility");
    }
    cout<<"PASS: social_golfers tests\n";
}
