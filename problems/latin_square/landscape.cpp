#include "solver.hpp"
#include "validator.hpp"
#include "../../shared/landscape.hpp"
#include <cmath>
#include <numeric>
#include <iostream>
#include <stdexcept>

namespace {
// Rows are permutations; the first row and column are fixed just as in the solver.
Grid candidate(int n, std::mt19937 &engine) {
    Grid grid(n, std::vector<int>(n));
    std::iota(grid[0].begin(), grid[0].end(), 1);
    for (int row = 1; row < n; ++row) {
        grid[row][0] = row + 1;
        std::vector<int> rest;
        for (int value = 1; value <= n; ++value)
            if (value != row + 1)
                rest.push_back(value);
        std::shuffle(rest.begin(), rest.end(), engine);
        std::copy(rest.begin(), rest.end(), grid[row].begin() + 1);
    }
    return grid;
}

std::size_t penalty(const Grid &grid) {
    const int n = static_cast<int>(grid.size());
    std::size_t total = 0;
    for (int col = 0; col < n; ++col) {
        std::vector<bool> seen(n + 1);
        for (int row = 0; row < n; ++row) {
            total += seen[grid[row][col]];
            seen[grid[row][col]] = true;
        }
    }
    return total;
}

bool neighbor(Grid &grid, std::mt19937 &engine) {
    const int n = static_cast<int>(grid.size());
    if (n < 3)
        return false;
    const int row = std::uniform_int_distribution<int>(1, n - 1)(engine);
    const int a = std::uniform_int_distribution<int>(1, n - 1)(engine);
    int b = std::uniform_int_distribution<int>(1, n - 2)(engine);
    if (b >= a)
        ++b;
    // Swap inside a row without touching the fixed first column.
    std::swap(grid[row][a], grid[row][b]);
    return true;
}

void tests() {
    Grid valid{{1, 2, 3}, {2, 3, 1}, {3, 1, 2}};
    check(penalty(valid) == 0 && validate(valid), "Zero penalty fixture failed");
    std::swap(valid[1][1], valid[1][2]);
    check(penalty(valid) == 2 && !validate(valid), "Column penalty fixture failed");
    std::mt19937 engine(42);
    for (int n = 1; n <= 8; ++n)
        for (int i = 0; i < 20; ++i) {
            Grid grid = candidate(n, engine);
            for (int pass = 0; pass < 2; ++pass) {
                for (int r = 0; r < n; ++r) {
                    check(grid[0][r] == r + 1 && grid[r][0] == r + 1, "Fixed cells changed");
                    auto row = grid[r];
                    std::sort(row.begin(), row.end());
                    for (int c = 0; c < n; ++c)
                        check(row[c] == c + 1, "Row permutation broken");
                }
                check((penalty(grid) == 0) == validate(grid), "Penalty and validator disagree");
                neighbor(grid, engine);
            }
        }
    std::cout << "PASS: Latin landscape candidates, penalty and swap neighbors\n";
}

int argument(const char *text, int maximum, int minimum = 1) {
    std::size_t end = 0;
    std::string input = text;
    int value = std::stoi(input, &end);
    if (end != input.size() || value < minimum || value > maximum)
        throw std::invalid_argument("Invalid argument");
    return value;
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--test") {
            tests();
            return 0;
        }
        if (argc > 4)
            throw std::invalid_argument("Usage: latin_square_landscape [n [samples [seed]]]");
        const int n = argc > 1 ? argument(argv[1], 8) : 5;
        const int samples = argc > 2 ? argument(argv[2], 1000000) : 1000;
        const unsigned seed =
            argc > 3 ? static_cast<unsigned>(argument(argv[3], 2147483647, 0)) : 42;
        const long double logSpace =
            (n - 1) * std::lgammal(static_cast<long double>(n)) / std::log(10.0L);
        std::cout << "Latin Square landscape; search is not run.\nSize: " << n
                  << ", samples: " << samples << ", seed: " << seed
                  << "\nCandidate space: ((n-1)!)^(n-1), approximately 10^(" << logSpace << ")\n"
                  << "Penalty: extra occurrences in columns; rows and fixed cells are preserved.\n";
        auto report = shared::sampleLandscape(
            samples, seed, [&](auto &engine) { return candidate(n, engine); }, penalty, neighbor);
        shared::printLandscape(report);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
