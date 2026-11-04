#include "solver.hpp"
#include "validator.hpp"

using namespace std;
void tests();
int main(int argc, char **argv) {
    try {
        if (argc == 2 && string(argv[1]) == "--test") {
            tests();
            return 0;
        }
        Metrics m;
        Timer timer;
        auto elapsed = [&]() { return timer.milliseconds(); };
        if (argc != 3 && argc != 4) {
            cerr
                << "Usage: sudoku <bfs|backtracking> <easy|medium|hard|hard_plus|81 digits> [limit]\n";
            return 2;
        }
        string algorithm = argv[1], input = argv[2];
        if (algorithm != "bfs" && algorithm != "backtracking")
            throw invalid_argument("Choose bfs or backtracking");
        if (examples.count(input))
            input = examples.at(input);
        Board b = parseBoard(input), clues = b;
        Status status =
            sudoku(b, algorithm == "bfs", argc == 4 ? integer(argv[3], 1, 5000000) : 200000, m);
        double ms = elapsed();
        if (status == Status::solved) {
            require(sudokuSolutionValid(clues, b), "Validation failed");
            for (int i = 0; i < 81; ++i) {
                require(!clues[i] || clues[i] == b[i], "Clue changed");
                cout << b[i] << (i % 9 == 8 ? '\n' : ' ');
            }
            cout << "Validation: PASS\n";
        }
        metrics(m, ms);
        return report(status);
    } catch (const invalid_argument &e) {
        cerr << "Input error: " << e.what() << '\n';
        return 2;
    } catch (const out_of_range &e) {
        cerr << "Input outside range: " << e.what() << '\n';
        return 2;
    } catch (const exception &e) {
        cerr << e.what() << '\n';
        return 3;
    }
}
