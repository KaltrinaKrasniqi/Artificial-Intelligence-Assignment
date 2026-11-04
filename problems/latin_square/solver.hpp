#pragma once
#include "../../shared/common.hpp"
#include <cstddef>
#include <utility>
#include <vector>

using Grid = std::vector<std::vector<int>>;

using Stats = Metrics;

class Solver {
    int n;
    Grid grid;
    Stats stats;
    std::vector<unsigned> rowUsed, colUsed;
    std::vector<std::pair<int, int>> cells;
    std::size_t nodeBudget; // 0 means no visit limit.

    bool allowed(int r, int c, int value) const;
    void place(int row, int col, int value);
    void undo(int row, int col, int value);
    bool search(int depth, int limit);

  public:
    explicit Solver(int size, std::size_t visitLimit = 0);
    bool solve();
    const Grid &result() const;
    const Stats &metrics() const;
};

void check(bool condition, const char *message);
