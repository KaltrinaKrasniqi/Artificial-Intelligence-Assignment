#include <cstdint>
#include "solver.hpp"

// A separate, explicitly restricted construction for 32 players and 9 weeks.
// DFS chooses nine disjoint sets {a,b,a XOR b} of nonzero 5-bit directions.
// It does not contain a stored schedule or solve the 10-week instance.
Status golfersLinearNine(size_t budget,Schedule& output,Metrics& stats) {
    output.clear(); stats={};
    if (!budget) throw invalid_argument("Node limit must be positive");
    vector<array<int,3>> directions;
    function<Status(std::uint32_t,int)> dfs=[&](std::uint32_t used,int start) {
        if (stats.visited>=budget) return Status::limit;
        ++stats.visited;
        if (directions.size()==9) return Status::solved;
        for(int a=start;a<32;++a) {
            if (used & (std::uint32_t{1}<<a)) continue;
            for(int b=a+1;b<32;++b) {
                const int c=a^b;
                if(c<=b || (used & (std::uint32_t{1}<<b)) || (used & (std::uint32_t{1}<<c))) continue;
                directions.push_back({a,b,c}); ++stats.generated;
                auto result=dfs(used | (std::uint32_t{1}<<a) | (std::uint32_t{1}<<b) |
                                (std::uint32_t{1}<<c),a+1);
                if(result==Status::solved) return result;
                directions.pop_back(); ++stats.backtracks;
                if(result==Status::limit) return result;
            }
        }
        return Status::unsat;
    };
    Status result=dfs(0,1);
    // Exhaustion here only excludes this family, not the general problem.
    if(result!=Status::solved) return Status::limit;
    for (const auto& direction:directions) {
        vector<vector<int>> week;
        std::uint32_t assigned=0;
        for(int player=0;player<32;++player) {
            if(assigned & (std::uint32_t{1}<<player)) continue;
            vector<int> group{player,player^direction[0],player^direction[1],player^direction[2]};
            sort(group.begin(),group.end());
            for(int a:group) assigned |= std::uint32_t{1}<<a;
            week.push_back(group);
        }
        output.push_back(week);
    }
    return Status::solved;
}
