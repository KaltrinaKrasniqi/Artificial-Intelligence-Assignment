#pragma once
#include "solver.hpp"

namespace golf_detail {
struct GroupCandidate {
    int week;
    array<int,4> players;
};

// Algorithm X implemented as depth-first search with reversible deletions
// (Dancing Links). Primary columns must be covered exactly once; secondary
// pair columns may be covered at most once. Columns and rows use fixed order:
// there is no MRV, objective score, A*, or best-first queue.
class ExactCover {
    vector<int> left,right,up,down,column,row;
    vector<GroupCandidate> candidates;
    vector<int> selected;
    Metrics& stats;
    size_t budget;
    bool limited=false;

    void cover(int c) {
        right[left[c]]=right[c]; left[right[c]]=left[c];
        for(int i=down[c];i!=c;i=down[i])
            for(int j=right[i];j!=i;j=right[j]) {
                up[down[j]]=up[j]; down[up[j]]=down[j];
            }
    }
    void uncover(int c) {
        for(int i=up[c];i!=c;i=up[i])
            for(int j=left[i];j!=i;j=left[j]) {
                up[down[j]]=j; down[up[j]]=j;
            }
        right[left[c]]=c; left[right[c]]=c;
    }
    bool weekOrderAllowed(int id) const {
        const auto& candidate=candidates[id];
        if(candidate.players[0]!=0) return true;
        // The five new weeks are interchangeable. Sort their groups containing
        // player 0, removing week permutations without losing a resolution.
        for(int other:selected) {
            const auto& previous=candidates[other];
            if(previous.players[0]!=0) continue;
            if((candidate.week<previous.week)!=(candidate.players<previous.players)) return false;
        }
        return true;
    }
    bool dfs() {
        if(stats.visited>=budget) {limited=true;return false;}
        ++stats.visited;
        if(right[0]==0) return true;
        const int c=right[0]; // first unmet player/week, in fixed order
        cover(c);
        for(int r=down[c];r!=c;r=down[r]) {
            if(!weekOrderAllowed(row[r])) continue;
            selected.push_back(row[r]); ++stats.generated;
            for(int j=right[r];j!=r;j=right[j]) cover(column[j]);
            if(dfs()) return true;
            for(int j=left[r];j!=r;j=left[j]) uncover(column[j]);
            selected.pop_back(); ++stats.backtracks;
            if(limited) break;
        }
        uncover(c);
        return false;
    }
public:
    ExactCover(int primary,int secondary,size_t cap,Metrics& metrics)
      : left(primary+secondary+1),right(left.size()),up(left.size()),down(left.size()),
        column(left.size()),row(left.size()),stats(metrics),budget(cap) {
        for(int i=0;i<=primary+secondary;++i) {
            up[i]=down[i]=column[i]=i;
            if(i<=primary) {left[i]=i-1;right[i]=i+1;}
            else left[i]=right[i]=i;
        }
        left[0]=primary; right[primary]=0;
    }
    void add(const vector<int>& constraints,GroupCandidate candidate) {
        const int id=static_cast<int>(candidates.size()); candidates.push_back(candidate);
        int first=-1;
        for(int index:constraints) {
            int c=index+1, node=static_cast<int>(left.size());
            left.push_back(node);right.push_back(node);up.push_back(up[c]);down.push_back(c);
            column.push_back(c);row.push_back(id);
            down[up[c]]=node; up[c]=node;
            if(first<0) first=node;
            else {
                left[node]=left[first];right[node]=first;
                right[left[first]]=node;left[first]=node;
            }
        }
    }
    Status solve(Schedule& newWeeks) {
        if(!dfs()) return limited?Status::limit:Status::unsat;
        newWeeks.assign(5,vector<vector<int>>{});
        for(int id:selected) {
            const auto& candidate=candidates[id];
            newWeeks[candidate.week].emplace_back(candidate.players.begin(),candidate.players.end());
        }
        for(auto& week:newWeeks) sort(week.begin(),week.end());
        return Status::solved;
    }
};
}
