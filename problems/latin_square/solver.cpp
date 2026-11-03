#include "solver.hpp"
#include <stdexcept>

bool Solver::allowed(int r, int c, int value) const {
        const unsigned bit = 1u << (value - 1);
        return ((rowUsed[r] | colUsed[c]) & bit) == 0;
    }

bool Solver::search(int depth, int limit) {
        if (stats.visited >= nodeBudget)
            throw std::runtime_error("Node budget reached; result is UNKNOWN, not UNSAT.");
        ++stats.visited;
        if (depth > stats.maxDepth) stats.maxDepth = depth;
        if (depth == static_cast<int>(cells.size())) return true;
        if (depth == limit) return false;
        const auto [r,c] = cells[depth];
        for (int value = 1; value <= n; ++value) {
            if (!allowed(r, c, value)) continue;
            const unsigned bit = 1u << (value - 1);
            grid[r][c] = value;
            rowUsed[r] |= bit; colUsed[c] |= bit;
            if (search(depth + 1, limit)) return true;
            grid[r][c] = 0;
            rowUsed[r] &= ~bit; colUsed[c] &= ~bit;
            ++stats.backtracks;
        }
        return false;
    }

Solver::Solver(int size) : n(size) {
        if (size < 1 || size > 8) throw std::invalid_argument("n must be from 1 to 8");
        grid.assign(n, std::vector<int>(n));
    }

bool Solver::solve(const std::string& algorithm) {
        grid.assign(n, std::vector<int>(n));
        stats = {};
        rowUsed.assign(n, 0); colUsed.assign(n, 0);
        cells.clear();
        // The same reduced-square symmetry as before, now represented as
        // fixed initial cells rather than single-choice search levels.
        for(int i=0;i<n;++i) { grid[0][i]=i+1; grid[i][0]=i+1; }
        for(int r=0;r<n;++r) for(int c=0;c<n;++c) {
            if(grid[r][c]) {
                unsigned bit=1u<<(grid[r][c]-1);
                rowUsed[r]|=bit; colUsed[c]|=bit;
            } else cells.emplace_back(r,c);
        }
        if (algorithm == "backtracking") {
            ++stats.iterations;
            return search(0, static_cast<int>(cells.size()));
        }
        if (algorithm != "iddfs") throw std::invalid_argument("Unknown algorithm.");
        for (int limit = 0; limit <= static_cast<int>(cells.size()); ++limit) {
            ++stats.iterations;
            if (search(0, limit)) return true;
        }
        return false;
    }

const Grid& Solver::result() const { return grid; }

const Stats& Solver::metrics() const { return stats; }

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

