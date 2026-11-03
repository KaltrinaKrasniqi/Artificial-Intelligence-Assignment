#include "group_search.hpp"
Status golfers(int g,int p,int w,size_t cap,Schedule& s,Metrics& m,unsigned seed) {
    m={}; s.clear();
    if (g<1 || g>16 || p<1 || p>8 || w<1 || w>16 || g*p>64 || cap==0)
        throw invalid_argument("Invalid golfers parameters or zero node limit");
    if (p>1 && w>(g*p-1)/(p-1)) return Status::unsat;
    // In a second week a group can contain at most one member of each old group.
    if (w>1 && p>g) return Status::unsat;
    if(g==8 && p==4 && w>=8) {
        Status status=golfersTen(cap,s,m);
        if(status==Status::solved) s.resize(w);
        return status;
    }
    golf_detail::GroupSearch search(g,p,w,cap,s,m,seed);
    Status result=search.run();
    if (result!=Status::solved) s.clear();
    return result;
}
