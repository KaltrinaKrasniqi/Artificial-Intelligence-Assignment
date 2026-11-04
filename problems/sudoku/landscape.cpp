#include <vector>
#include <algorithm>
#include <random>
#include "solver.hpp"
#include "validator.hpp"
#include "../../shared/landscape.hpp"
#include <cmath>

using namespace std;

namespace {
using FreeRows = array<vector<int>, 9>;

FreeRows mutableCells(const Board &clues) {
    FreeRows rows;
    for (int i = 0; i < 81; ++i)
        if (!clues[i])
            rows[i / 9].push_back(i);
    return rows;
}

Board candidate(const Board &clues, const FreeRows &rows, mt19937 &engine) {
    Board board = clues;
    for (int row = 0; row < 9; ++row) {
        array<bool, 10> used{};
        for (int col = 0; col < 9; ++col)
            used[board[row * 9 + col]] = true;
        vector<int> missing;
        for (int value = 1; value <= 9; ++value)
            if (!used[value])
                missing.push_back(value);
        // Fill blanks with the missing digits so every row stays valid.
        shuffle(missing.begin(), missing.end(), engine);
        for (size_t i = 0; i < rows[row].size(); ++i)
            board[rows[row][i]] = missing[i];
    }
    return board;
}

size_t penalty(const Board &board) {
    size_t total = 0;
    for (int unit = 0; unit < 9; ++unit) {
        array<bool, 10> column{}, box{};
        for (int i = 0; i < 9; ++i) {
            int a = board[i * 9 + unit];
            int b = board[(unit / 3 * 3 + i / 3) * 9 + unit % 3 * 3 + i % 3];
            total += column[a];
            column[a] = true;
            total += box[b];
            box[b] = true;
        }
    }
    return total;
}

bool neighbor(Board &board, const FreeRows &rows, mt19937 &engine) {
    vector<int> choices;
    for (int row = 0; row < 9; ++row)
        if (rows[row].size() > 1)
            choices.push_back(row);
    if (choices.empty())
        return false;
    const auto &cells = rows[choices[uniform_int_distribution<int>(
        0, static_cast<int>(choices.size()) - 1)(engine)]];
    const int a = uniform_int_distribution<int>(0, static_cast<int>(cells.size()) - 1)(engine);
    int b = uniform_int_distribution<int>(0, static_cast<int>(cells.size()) - 2)(engine);
    if (b >= a)
        ++b;
    swap(board[cells[a]], board[cells[b]]);
    return true;
}

void tests() {
    Board solved = parseBoard(
        "534678912672195348198342567859761423426853791713924856961537284287419635345286179");
    require(penalty(solved) == 0 && sudokuValid(solved, true), "Zero penalty fixture failed");
    swap(solved[0], solved[1]);
    require(penalty(solved) == 2 && !sudokuValid(solved, true), "Duplicate penalty fixture failed");
    mt19937 engine(42);
    for (const auto &[name, text] : examples) {
        Board clues = parseBoard(text);
        auto rows = mutableCells(clues);
        for (int i = 0; i < 30; ++i) {
            Board board = candidate(clues, rows, engine);
            for (int pass = 0; pass < 2; ++pass) {
                for (int cell = 0; cell < 81; ++cell)
                    require(!clues[cell] || clues[cell] == board[cell], "Clue changed");
                for (int row = 0; row < 9; ++row) {
                    vector<int> values(board.begin() + row * 9, board.begin() + row * 9 + 9);
                    sort(values.begin(), values.end());
                    for (int col = 0; col < 9; ++col)
                        require(values[col] == col + 1, "Row permutation broken");
                }
                require((penalty(board) == 0) == sudokuSolutionValid(clues, board),
                        "Penalty and validator disagree");
                neighbor(board, rows, engine);
            }
        }
    }
    cout << "PASS: Sudoku landscape clues, row permutations, penalty and neighbors\n";
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 2 && string(argv[1]) == "--test") {
            tests();
            return 0;
        }
        if (argc > 4)
            throw invalid_argument(
                "Usage: sudoku_landscape [easy|medium|hard|hard_plus|81 digits [samples [seed]]]");
        string input = argc > 1 ? argv[1] : "easy";
        if (examples.count(input))
            input = examples.at(input);
        const Board clues = parseBoard(input);
        if (!sudokuValid(clues, false))
            throw invalid_argument("Invalid Sudoku clues");
        const int samples = argc > 2 ? integer(argv[2], 1, 1000000) : 1000;
        const unsigned seed =
            argc > 3 ? static_cast<unsigned>(integer(argv[3], 0, 2147483647)) : 42;
        const auto rows = mutableCells(clues);
        long double logSpace = 0;
        for (const auto &row : rows)
            logSpace += lgammal(static_cast<long double>(row.size()) + 1) / logl(10.0L);
        cout
            << "Sudoku landscape; search is not run.\nSamples: " << samples << ", seed: " << seed
            << "\nCandidate space: product of k_row!, approximately 10^(" << logSpace << ")\n"
            << "Penalty: extra occurrences in columns and 3x3 boxes; clues and row permutations are preserved.\n";
        auto report = shared::sampleLandscape(
            samples, seed, [&](auto &engine) { return candidate(clues, rows, engine); }, penalty,
            [&](auto &board, auto &engine) { return neighbor(board, rows, engine); });
        shared::printLandscape(report);
        return 0;
    } catch (const exception &error) {
        cerr << error.what() << '\n';
        return 2;
    }
}
