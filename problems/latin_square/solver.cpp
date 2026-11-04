#include "solver.hpp"
#include "../../shared/common.hpp"
#include <stdexcept>

namespace {
// Value 1 uses bit 0, value 2 uses bit 1, and so on.
bool containsValue(unsigned values, int value) {
    return (values & (1u << (value - 1))) != 0;
}
unsigned addValue(unsigned values, int value) {
    return values | (1u << (value - 1));
}
unsigned removeValue(unsigned values, int value) {
    return values & ~(1u << (value - 1));
}
} // namespace

bool Solver::allowed(int row, int col, int value) const {
    return !containsValue(rowUsed[row], value) && !containsValue(colUsed[col], value);
}

void Solver::place(int row, int col, int value) {
    grid[row][col] = value;
    rowUsed[row] = addValue(rowUsed[row], value);
    colUsed[col] = addValue(colUsed[col], value);
}

void Solver::undo(int row, int col, int value) {
    grid[row][col] = 0;
    rowUsed[row] = removeValue(rowUsed[row], value);
    colUsed[col] = removeValue(colUsed[col], value);
}

bool Solver::search(int depth, int limit) {
    // A zero budget lets IDDFS continue without a visit cap.
    if (nodeBudget && stats.visited >= nodeBudget)
        throw std::runtime_error("Node budget reached; result is UNKNOWN, not UNSAT.");
    ++stats.visited;
    if (depth > stats.maxDepth)
        stats.maxDepth = depth;
    if (depth == static_cast<int>(cells.size()))
        return true;
    // This branch can go deeper in the next IDDFS iteration.
    if (depth == limit)
        return false;
    const auto [row, col] = cells[depth];
    for (int value = 1; value <= n; ++value) {
        if (!allowed(row, col, value))
            continue;
        place(row, col, value);
        if (search(depth + 1, limit))
            return true;
        // Restore the cell and its masks before trying another value.
        undo(row, col, value);
        ++stats.backtracks;
    }
    return false;
}

Solver::Solver(int size, std::size_t visitLimit) : n(size), nodeBudget(visitLimit) {
    if (size < 1 || size > 8)
        throw std::invalid_argument("n must be from 1 to 8");
    grid.assign(n, std::vector<int>(n));
}

bool Solver::solve() {
    grid.assign(n, std::vector<int>(n));
    stats = {};
    rowUsed.assign(n, 0);
    colUsed.assign(n, 0);
    cells.clear();
    // Fix the first row and column to remove equivalent labelings.
    for (int i = 0; i < n; ++i) {
        grid[0][i] = i + 1;
        grid[i][0] = i + 1;
    }
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) {
            if (grid[r][c]) {
                place(r, c, grid[r][c]);
            } else
                cells.emplace_back(r, c);
        }
    // IDDFS: run the same depth-limited DFS with limits 0, 1, 2, ...
    for (int limit = 0; limit <= static_cast<int>(cells.size()); ++limit) {
        ++stats.iterations;
        if (search(0, limit))
            return true;
    }
    return false;
}

const Grid &Solver::result() const {
    return grid;
}

const Stats &Solver::metrics() const {
    return stats;
}

void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
