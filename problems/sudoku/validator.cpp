#include "validator.hpp"

using namespace std;
bool sudokuValid(const Board &b, bool complete) {
    for (int unit = 0; unit < 9; ++unit) {
        // Check one row, column and box in the same pass.
        array<bool, 10> row{}, col{}, box{};
        for (int k = 0; k < 9; ++k) {
            int values[3] = {b[unit * 9 + k], b[k * 9 + unit],
                             b[(unit / 3 * 3 + k / 3) * 9 + unit % 3 * 3 + k % 3]};
            array<bool, 10> *seen[3] = {&row, &col, &box};
            for (int j = 0; j < 3; ++j) {
                int v = values[j];
                if (v < 0 || v > 9 || (complete && v == 0))
                    return false;
                if (v && (*seen[j])[v])
                    return false;
                if (v)
                    (*seen[j])[v] = true;
            }
        }
    }
    return true;
}

bool sudokuSolutionValid(const Board &clues, const Board &solution) {
    if (!sudokuValid(clues, false) || !sudokuValid(solution, true))
        return false;
    // A valid completed board must also preserve every starting clue.
    for (int i = 0; i < 81; ++i)
        if (clues[i] && clues[i] != solution[i])
            return false;
    return true;
}
