#include "solver.hpp"
#include "validator.hpp"

using namespace std;
void tests() {
    Board solved = parseBoard(
        "534678912672195348198342567859761423426853791713924856961537284287419635345286179");
    require(sudokuValid(solved, true), "Sudoku validator rejects solution");
    for (bool bfs : {false, true}) {
        Board b = solved;
        b[0] = b[10] = b[80] = 0;
        Board clues = b;
        Metrics m;
        require(sudoku(b, bfs, 10000, m) == Status::solved && sudokuValid(b, true),
                "Sudoku solver failed");
        for (int i = 0; i < 81; ++i)
            require(!clues[i] || clues[i] == b[i], "Clue changed");
    }
    Board bad = solved;
    bad[0] = bad[1];
    require(!sudokuValid(bad, false), "Duplicate accepted");
    bad = solved;
    bad[0] = 0;
    require(!sudokuValid(bad, true), "Incomplete accepted");
    Board limited{};
    Metrics lm;
    require(sudoku(limited, true, 1, lm) == Status::limit, "Limit ignored");
    Board impossible = solved;
    impossible[0] = 1;
    impossible[7] = 0;
    impossible[18] = 0;
    require(sudokuValid(impossible, false), "Bad UNSAT fixture");
    for (bool bfs : {false, true}) {
        Board b = impossible;
        Metrics m;
        require(sudoku(b, bfs, 10000, m) == Status::unsat, "UNSAT Sudoku missed");
    }

    Board changed = solved;
    for (int &value : changed)
        if (value == 1)
            value = 2;
        else if (value == 2)
            value = 1;
    require(sudokuValid(changed, true) && !sudokuSolutionValid(solved, changed),
            "Changed clues accepted");
    Metrics repeated;
    Board b = solved;
    b[0] = 0;
    require(sudoku(b, true, 100, repeated) == Status::solved, "Initial solve failed");
    b = solved;
    b[0] = 0;
    require(sudoku(b, true, 100, repeated) == Status::solved && repeated.visited == 2,
            "Metrics not reset");
    bool rejected = false;
    try {
        sudoku(b, true, 0, repeated);
    } catch (const invalid_argument &) {
        rejected = true;
    }
    require(rejected, "Zero limit accepted");
    for (const auto &example : examples)
        for (bool bfs : {false, true}) {
            Board clues = parseBoard(example.second), solution = clues;
            Metrics measurement;
            require(sudoku(solution, bfs, 200000, measurement) == Status::solved &&
                        sudokuSolutionValid(clues, solution),
                    "Example failed");
        }

    for (bool bfs : {false, true}) {
        Board clues = parseBoard(examples.at("hard")), solution = clues;
        Metrics m;
        require(sudoku(solution, bfs, 1, m) == Status::limit && solution == clues,
                "Cutoff changed original clues");
    }
    Board outOfRange = solved;
    outOfRange[0] = 10;
    Metrics invalidMetrics;
    bool invalidRejected = false;
    try {
        sudoku(outOfRange, true, 100, invalidMetrics);
    } catch (const invalid_argument &) {
        invalidRejected = true;
    }
    require(invalidRejected, "Invalid value reached mask construction");
    // Bonus example: independently checked to have exactly one solution.
    Board bonus = parseBoard(examples.at("hard_plus"));
    Metrics bonusMetrics;
    require(
        sudoku(bonus, true, 200000, bonusMetrics) == Status::solved &&
            bonus ==
                parseBoard(
                    "495168237762359184831724695627891453184536972359247861978415326513682749246973518"),
        "Hard+ did not return the verified solution");
    require(bonusMetrics.visited == 117167, "Hard+ BFS workload changed");
    cout << "PASS: sudoku tests\n";
}
