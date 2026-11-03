#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using Grid = std::vector<std::vector<int>>;

struct Stats {
    std::uint64_t visited = 0, backtracks = 0;
    int iterations = 0, maxDepth = 0;
};

class Solver {
    int n;
    Grid grid;
    Stats stats;
    std::vector<unsigned> rowUsed, colUsed;
    std::vector<std::pair<int,int>> cells;
    static constexpr std::uint64_t nodeBudget = 2000000;

    bool allowed(int r, int c, int value) const;
    bool search(int depth, int limit);

public:
    explicit Solver(int size);
    bool solve(const std::string& algorithm);
    const Grid& result() const;
    const Stats& metrics() const;
};

void check(bool condition, const char* message);
