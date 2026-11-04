#include "solver.hpp"
#include "validator.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
void tests();
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--test") {
            tests();
            return 0;
        }
        if (argc != 2 && argc != 3) {
            std::cerr << "Usage: latin_square <n: 1..8> [visit limit: 0 = unlimited]\n"
                      << "       latin_square --test\n";
            return 2;
        }
        const std::string input = argv[1];
        if (input.size() != 1 || input[0] < '1' || input[0] > '8')
            throw std::invalid_argument("n must be an integer from 1 to 8.");
        std::size_t visitLimit = 0;
        if (argc == 3) {
            const std::string text = argv[2];
            if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("Visit limit must be a nonnegative integer.");
            const auto parsed = std::stoull(text);
            if (parsed > std::numeric_limits<std::size_t>::max())
                throw std::invalid_argument("Visit limit exceeds this platform's range.");
            visitLimit = static_cast<std::size_t>(parsed);
        }
        Solver solver(input[0] - '0', visitLimit);
        Timer timer;
        const bool solved = solver.solve();
        const double ms = timer.milliseconds();
        if (!solved) {
            std::cout << "No solution.\n";
            return 1;
        }
        check(validate(solver.result()), "Independent validation failed");
        for (const auto &row : solver.result()) {
            for (int value : row)
                std::cout << value << ' ';
            std::cout << '\n';
        }
        const auto &s = solver.metrics();
        std::cout << "Validation: PASS\nVisited: " << s.visited << "\nBacktracks: " << s.backtracks
                  << "\nIterations: " << s.iterations
                  << "\nMaximum depth (unfixed cells): " << s.maxDepth << "\nTime (ms): " << ms
                  << '\n';
        return 0;
    } catch (const std::invalid_argument &e) {
        std::cerr << "Input error: " << e.what() << '\n';
        return 2;
    } catch (const std::out_of_range &e) {
        std::cerr << "Input outside range: " << e.what() << '\n';
        return 2;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 3;
    }
}
