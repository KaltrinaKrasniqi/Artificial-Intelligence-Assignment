#include "validator.hpp"
bool golfValid(const Schedule& s,int g,int p,int w) {
    if (g<1 || g>16 || p<1 || p>8 || w<1 || w>16 || g*p>64) return false;
    if(static_cast<int>(s.size())!=w) return false;
    set<pair<int,int>> pairs;
    for(const auto& week:s) {
        if(static_cast<int>(week.size())!=g) return false;
        vector<int> seen(g*p);
        for(const auto& group:week) {
            if(static_cast<int>(group.size())!=p) return false;
            for(int a:group) if(a<0 || a>=g*p || ++seen[a]!=1) return false;
            for(int i=0;i<p;++i) for(int j=i+1;j<p;++j) {
                auto key=minmax(group[i],group[j]);
                if(!pairs.insert(key).second) return false;
            }
        }
        if(any_of(seen.begin(),seen.end(),[](int x){return x!=1;})) return false;
    }
    return true;
}
