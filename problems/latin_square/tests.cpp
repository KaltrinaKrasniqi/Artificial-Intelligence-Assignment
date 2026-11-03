#include "solver.hpp"
#include "validator.hpp"
#include <iostream>
#include <stdexcept>
void tests() {
    check(validate({{1}}), "Valid 1x1 rejected");
    check(validate({{1,2,3},{2,3,1},{3,1,2}}), "Valid square rejected");
    check(!validate({}), "Empty grid accepted");
    check(!validate({{1,2},{2}}), "Ragged grid accepted");
    check(!validate({{1,1},{2,2}}), "Row duplicates accepted");
    check(!validate({{1,2},{1,2}}), "Column duplicates accepted");
    check(!validate({{0,2},{2,1}}), "Zero accepted");
    check(!validate({{1,3},{2,1}}), "Out-of-range value accepted");
    for (const std::string algorithm : {"backtracking", "iddfs"}) {
        for (int n = 1; n <= 3; ++n) {
            Solver solver(n);
            check(solver.solve(algorithm), "No solution found");
            check(validate(solver.result()), "Invalid solver output");
            check(solver.metrics().maxDepth == (n-1)*(n-1), "Incorrect solution depth");
            if (algorithm == "iddfs")
                check(solver.metrics().iterations == (n-1)*(n-1)+1, "Incorrect IDDFS iterations");
            check(solver.solve(algorithm), "Repeated solve failed");
        }
    }
    for (int n : {4,5,6,7,8}) {
        Solver solver(n);
        check(solver.solve("backtracking") && validate(solver.result()), "Larger case failed");
    }

    for (int n=4;n<=5;++n) {
        Solver solver(n);
        check(solver.solve("iddfs") && validate(solver.result()),"Reduced IDDFS failed");
    }
    bool rejected=false;
    try { Solver invalid(0); } catch (const std::invalid_argument&) { rejected=true; }
    check(rejected,"Invalid size accepted by API");
    std::cout << "PASS: solver cases, independent validator negative cases, repeated calls.\n";
}

