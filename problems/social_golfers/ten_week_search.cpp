#include "exact_cover.hpp"
#include "validator.hpp"

Status golfersTen(size_t cap,Schedule& result,Metrics& stats) {
    result.clear();
    Schedule linear;
    Status initial=golfersLinearNine(cap,linear,stats);
    if(initial!=Status::solved) return Status::limit;

    // A successful restricted model discovered by testing five-week subsets
    // of the generated linear design. This is NOT a stored solution and NOT
    // a symmetry valid for every schedule. Failure here must mean UNKNOWN.
    const array<int,5> retained{1,2,3,4,8};
    Schedule prefix;
    bool used[32][32]{};
    for(int index:retained) {
        prefix.push_back(linear[index]);
        for(const auto& group:prefix.back())
            for(int a:group) for(int b:group) used[a][b]=true;
    }

    int pairIndex[32][32];
    for(auto& row:pairIndex) for(int& value:row) value=-1;
    int pairCount=0;
    for(int a=0;a<32;++a) for(int b=a+1;b<32;++b)
        if(!used[a][b]) pairIndex[a][b]=pairIndex[b][a]=pairCount++;

    // 160 primary constraints: one appearance per player per new week.
    // Order interleaves weeks for each player: 0/week0,0/week1,...,1/week0,...
    // Secondary constraints prohibit reuse of any available pair.
    golf_detail::ExactCover search(160,pairCount,cap,stats);
    for(int a=0;a<32;++a) for(int b=a+1;b<32;++b)
        for(int c=b+1;c<32;++c) for(int d=c+1;d<32;++d) {
            array<int,4> group{a,b,c,d};
            vector<int> pairs;
            bool valid=true;
            for(int i=0;i<4;++i) for(int j=i+1;j<4;++j) {
                int pair=pairIndex[group[i]][group[j]];
                if(pair<0) valid=false;
                pairs.push_back(pair);
            }
            if(!valid) continue;
            for(int week=0;week<5;++week) {
                vector<int> constraints;
                for(int player:group) constraints.push_back(player*5+week);
                for(int pair:pairs) constraints.push_back(160+pair);
                search.add(constraints,{week,group});
            }
        }
    Schedule completion;
    Status status=search.solve(completion);
    if(status!=Status::solved) return Status::limit;
    prefix.insert(prefix.end(),completion.begin(),completion.end());
    if(!golfValid(prefix,8,4,10)) throw runtime_error("Ten-week independent validation failed");
    result=std::move(prefix);
    return Status::solved;
}
