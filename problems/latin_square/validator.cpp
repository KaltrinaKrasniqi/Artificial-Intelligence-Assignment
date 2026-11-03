#include "validator.hpp"
// Independent validator: does not use the solver's placement checks.
bool validate(const Grid& grid) {
    const int n = static_cast<int>(grid.size());
    if (n == 0) return false;
    for (const auto& row : grid)
        if (static_cast<int>(row.size()) != n) return false;
    for (int i = 0; i < n; ++i) {
        std::vector<int> rowCount(n + 1), colCount(n + 1);
        for (int j = 0; j < n; ++j) {
            const int a = grid[i][j], b = grid[j][i];
            if (a < 1 || a > n || b < 1 || b > n) return false;
            if (++rowCount[a] != 1 || ++colCount[b] != 1) return false;
        }
    }
    return true;
}

