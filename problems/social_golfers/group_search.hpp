#pragma once
#include <cstdint>
#include <random>
#include <numeric>
#include "solver.hpp"

namespace golf_detail {
using Mask = std::uint64_t;
inline int count(Mask bits) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(bits);
#else
    int total=0;for(;bits;bits&=bits-1)++total;return total;
#endif
}
inline int first(Mask bits) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(bits);
#else
    int index=0;while(!(bits&1)){bits>>=1;++index;}return index;
#endif
}

class GroupSearch {
    const int groups, size, weeks, players;
    const size_t budget;
    Schedule& schedule;
    Metrics& stats;
    const Mask all;
    vector<Mask> available;
    bool limited=false;
    vector<vector<int>> order;
    int choose(Mask bits,int week) const {
        for(int player:order[week]) if(bits & (Mask{1}<<player)) return player;
        return 0;
    }

    bool enter() {
        if (stats.visited >= budget) { limited=true; return false; }
        ++stats.visited;
        return true;
    }

    // Necessary condition: each remaining player needs size-1 compatible
    // partners in this week. It does not rank or estimate solution quality.
    bool feasible(Mask remaining) const {
        for (Mask todo=remaining; todo; todo &= todo-1) {
            int player=first(todo);
            if (count(available[player] & remaining) < size-1) return false;
        }
        return true;
    }

    bool clique(Mask candidates,int needed) {
        if (!enter()) return false;
        if (!needed) return true;
        if (count(candidates)<needed) return false;
        while (candidates) {
            int player=first(candidates); candidates &= candidates-1;
            if (clique(candidates & available[player],needed-1)) return true;
            if (limited || count(candidates)<needed) break;
        }
        return false;
    }

    Status extend(int week, int group, Mask remaining, Mask chosen, Mask candidates) {
        if (!enter()) return Status::limit;
        int needed=size-count(chosen);
        if (!needed) {
            auto& output=schedule[week][group];
            for (Mask bits=chosen; bits; bits &= bits-1) output.push_back(first(bits));
            for (int player:output) available[player] &= ~chosen;
            ++stats.generated;
            bool future=true;
            if (week+1<weeks) for (int player:output) {
                if (!clique(available[player],size-1)) { future=false; break; }
            }
            Status result=limited?Status::limit:!future?Status::unsat:
                partition(week,group+1,remaining & ~chosen);
            if (result==Status::solved) return result;
            for (int player:output) available[player] |= chosen & ~(Mask{1}<<player);
            output.clear(); ++stats.backtracks;
            return result;
        }
        if (count(candidates)<needed) return Status::unsat;
        while (candidates) {
            int player=choose(candidates,week); Mask bit=Mask{1}<<player;
            candidates &= ~bit;
            if (count(candidates)+1<needed) break;
            Status result=extend(week,group,remaining,chosen|bit,candidates & available[player]);
            if (result!=Status::unsat) return result;
        }
        return Status::unsat;
    }

    Status partition(int week,int group,Mask remaining) {
        if (!enter()) return Status::limit;
        if (!remaining) {
            if (week+1==weeks) return Status::solved;
            return partition(week+1,0,all);
        }
        if (!feasible(remaining)) return Status::unsat;
        // Interchangeable group labels: anchor the smallest remaining player.
        int anchor=first(remaining); Mask bit=Mask{1}<<anchor;
        return extend(week,group,remaining,bit,remaining & available[anchor]);
    }

public:
    GroupSearch(int g,int p,int w,size_t cap,Schedule& s,Metrics& m,unsigned seed=0)
      : groups(g),size(p),weeks(w),players(g*p),budget(cap),schedule(s),stats(m),
        all(players==64?~Mask{0}:(Mask{1}<<players)-1),available(players,all) {
        order.assign(weeks,vector<int>(players));
        std::mt19937 engine(seed);
        for(auto& row:order) {std::iota(row.begin(),row.end(),0);if(seed)std::shuffle(row.begin(),row.end(),engine);}
        for (int a=0;a<players;++a) available[a] &= ~(Mask{1}<<a);
    }
    Status run() {
        schedule.assign(weeks,vector<vector<int>>(groups));
        // Canonical first week follows by relabeling players.
        for (int a=0;a<players;++a) schedule[0][a/size].push_back(a);
        for (const auto& group:schedule[0]) {
            Mask members=0;
            for (int a:group) members |= Mask{1}<<a;
            for (int a:group) available[a] &= ~members;
        }
        if (!enter()) return Status::limit;
        if (weeks==1) return Status::solved;
        return partition(1,0,all);
    }
};
}
