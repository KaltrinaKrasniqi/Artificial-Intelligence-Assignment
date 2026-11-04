#include "solver.hpp"
#include "validator.hpp"
#include <iostream>
#include <stdexcept>
void tests() {
    check(validate({{1}}), "Valid 1x1 rejected");
    check(validate({{1, 2, 3}, {2, 3, 1}, {3, 1, 2}}), "Valid square rejected");
    check(!validate({}), "Empty grid accepted");
    check(!validate({{1, 2}, {2}}), "Ragged grid accepted");
    check(!validate({{1, 1}, {2, 2}}), "Row duplicates accepted");
    check(!validate({{1, 2}, {1, 2}}), "Column duplicates accepted");
    check(!validate({{0, 2}, {2, 1}}), "Zero accepted");
    check(!validate({{1, 3}, {2, 1}}), "Out-of-range value accepted");
    for (int n = 1; n <= 6; ++n) {
        Solver solver(n);
        check(solver.solve(), "IDDFS found no solution");
        check(validate(solver.result()), "Invalid solver output");
        check(solver.metrics().maxDepth == (n - 1) * (n - 1), "Incorrect solution depth");
        check(solver.metrics().iterations == (n - 1) * (n - 1) + 1, "Incorrect IDDFS iterations");
        const auto visits = solver.metrics().visited;
        check(solver.solve() && solver.metrics().visited == visits,
              "Repeated IDDFS solve changed behavior");
    }
    // Budget exhaustion must stay UNKNOWN; there is no fallback to ordinary DFS.
    for (int n : {7, 8}) {
        Solver solver(n, 2000000);
        bool limited = false;
        try {
            solver.solve();
        } catch (const std::runtime_error &error) {
            limited = std::string(error.what()).find("UNKNOWN") != std::string::npos;
        }
        check(limited && solver.metrics().visited == 2000000, "IDDFS budget behavior changed");
    }
    Solver bounded(5, 2000000), unlimited(5, 0);
    check(bounded.solve() && unlimited.solve(), "Budget modes failed");
    check(bounded.result() == unlimited.result() &&
              bounded.metrics().visited == unlimited.metrics().visited,
          "Optional budget changed search order or work");
    Solver tiny(5, 1);
    bool stopped = false;
    try { tiny.solve(); }
    catch (const std::runtime_error &) { stopped = true; }
    check(stopped && tiny.metrics().visited == 1, "Selected visit limit ignored");
    bool rejected = false;
    try {
        Solver invalid(0);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    check(rejected, "Invalid size accepted by API");
    std::cout << "PASS: solver cases, independent validator negative cases, repeated calls.\n";
}
