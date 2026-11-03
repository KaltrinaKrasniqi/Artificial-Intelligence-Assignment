#include <chrono>
#include "solver.hpp"
#include "validator.hpp"
#include <iostream>
#include <stdexcept>
void tests();
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--test") { tests(); return 0; }
        if (argc != 3) {
            std::cerr << "Usage: latin_square <n: 1..8> <iddfs|backtracking>\n"
                      << "       latin_square --test\n";
            return 2;
        }
        const std::string input = argv[1], algorithm = argv[2];
        if (input.size() != 1 || input[0] < '1' || input[0] > '8')
            throw std::invalid_argument("n must be an integer from 1 to 8.");
        if (algorithm != "iddfs" && algorithm != "backtracking")
            throw std::invalid_argument("Choose iddfs or backtracking.");
        Solver solver(input[0] - '0');
        const auto start = std::chrono::steady_clock::now();
        const bool solved = solver.solve(algorithm);
        const double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        if (!solved) { std::cout << "No solution.\n"; return 1; }
        check(validate(solver.result()), "Independent validation failed");
        for (const auto& row : solver.result()) {
            for (int value : row) std::cout << value << ' ';
            std::cout << '\n';
        }
        const auto& s = solver.metrics();
        std::cout << "Validation: PASS\nVisited: " << s.visited
                  << "\nBacktracks: " << s.backtracks << "\nIterations: " << s.iterations
                  << "\nMaximum depth (unfixed cells): " << s.maxDepth << "\nTime (ms): " << ms << '\n';
        return 0;
    } catch (const std::invalid_argument& e) {
        std::cerr << "Input error: " << e.what() << '\n'; return 2;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 3;
    }
}
