#include <algorithm>
#include <queue>
#include <vector>
#include "solver.hpp"
#include "validator.hpp"
#include "../../shared/common.hpp"

using namespace std;

namespace {
// A set bit means that a digit is present. Sudoku uses bits 1..9; bit 0 is unused.
bool containsDigit(unsigned digits, int digit) {
    return (digits & (1u << digit)) != 0;
}
unsigned addDigit(unsigned digits, int digit) {
    return digits | (1u << digit);
}
unsigned removeDigit(unsigned digits, int digit) {
    return digits & ~(1u << digit);
}

struct CompactState {
    array<unsigned char, 81> cells{};
    array<unsigned short, 9> rows{}, cols{}, boxes{};
    // Depth is the number of empty cells filled so far.
    unsigned char depth = 0;
};
int boxIndex(int pos) {
    return (pos / 27) * 3 + (pos % 9) / 3;
}
unsigned allowedDigits(const CompactState &state, int pos) {
    // Digits present in any of the three units are forbidden.
    constexpr unsigned allDigits = 0b1111111110; // Bits 1..9 set.
    const unsigned usedDigits =
        state.rows[pos / 9] | state.cols[pos % 9] | state.boxes[boxIndex(pos)];
    return allDigits & ~usedDigits;
}
void place(CompactState &state, int pos, int value) {
    state.cells[pos] = static_cast<unsigned char>(value);
    state.rows[pos / 9] = addDigit(state.rows[pos / 9], value);
    state.cols[pos % 9] = addDigit(state.cols[pos % 9], value);
    state.boxes[boxIndex(pos)] = addDigit(state.boxes[boxIndex(pos)], value);
}
void undo(CompactState &state, int pos, int value) {
    state.cells[pos] = 0;
    state.rows[pos / 9] = removeDigit(state.rows[pos / 9], value);
    state.cols[pos % 9] = removeDigit(state.cols[pos % 9], value);
    state.boxes[boxIndex(pos)] = removeDigit(state.boxes[boxIndex(pos)], value);
}
// Ordinary DFS: place a value, recurse, and undo the placement on failure.
Status searchDepthFirst(CompactState &state, const vector<int> &empty, size_t depth,
                        SearchContext &context) {
    if (!context.enter())
        return Status::limit;
    if (depth == empty.size())
        return Status::solved;
    const int pos = empty[depth];
    const unsigned legalDigits = allowedDigits(state, pos);
    for (int value = 1; value <= 9; ++value) {
        if (!containsDigit(legalDigits, value))
            continue;
        place(state, pos, value);
        ++context.metrics.generated;
        Status result = searchDepthFirst(state, empty, depth + 1, context);
        if (result == Status::solved)
            return result;
        undo(state, pos, value);
        ++context.metrics.backtracks;
        if (result == Status::limit)
            return result;
    }
    return Status::unsat;
}
} // namespace
Status sudoku(Board &b, bool bfs, size_t cap, Metrics &m) {
    m = {};
    if (!cap)
        throw invalid_argument("Node limit must be positive");
    if (!sudokuValid(b, false))
        throw invalid_argument("Sudoku clues contain conflicts");
    CompactState state;
    vector<int> empty;
    for (int i = 0; i < 81; ++i) {
        if (b[i])
            place(state, i, b[i]);
        else
            empty.push_back(i);
    }
    SearchContext context(m, cap);
    if (bfs) {
        // BFS: take states from the front and add legal successors at the back.
        queue<CompactState> frontier;
        frontier.push(state);
        m.peak = 1;
        while (!frontier.empty()) {
            if (!context.enter())
                return Status::limit;
            CompactState current = frontier.front();
            frontier.pop();
            if (current.depth == empty.size()) {
                copy(current.cells.begin(), current.cells.end(), b.begin());
                return Status::solved;
            }
            // With no legal digit, this state produces no children.
            const int pos = empty[current.depth];
            const unsigned legalDigits = allowedDigits(current, pos);
            for (int value = 1; value <= 9; ++value) {
                if (!containsDigit(legalDigits, value))
                    continue;
                if (frontier.size() >= cap)
                    return Status::limit;
                // Each alternative keeps its own board in the queue.
                CompactState child = current;
                place(child, pos, value);
                ++child.depth;
                frontier.push(child);
                ++m.generated;
                m.peak = max(m.peak, frontier.size());
            }
        }
        return Status::unsat;
    }
    Status result = searchDepthFirst(state, empty, 0, context);
    if (result == Status::solved)
        copy(state.cells.begin(), state.cells.end(), b.begin());
    return result;
}
Board parseBoard(const string &text) {
    if (text.size() != 81)
        throw invalid_argument("Sudoku needs exactly 81 digits (0 or . for blanks)");
    Board b{};
    for (int i = 0; i < 81; ++i) {
        char c = text[i];
        if (c == '.')
            b[i] = 0;
        else if (c >= '0' && c <= '9')
            b[i] = c - '0';
        else
            throw invalid_argument("Invalid Sudoku character");
    }
    return b;
}
extern const map<string, string> examples = {
    {"easy", "530070000600195000098000060800060003400803001700020006060000280000419005000080079"},
    {"medium", "000260701680070090190004500820100040004602900050003028009300074040050036703018000"},
    {"hard", "100007090030020008009600500005300900010080002600004000300000010040000007007000300"},
    {"hard_plus",
     "400100007002050080030004600007090000100500000050007000008010020000600009040003500"}};
