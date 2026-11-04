#include <array>
#include "solver.hpp"
#include "validator.hpp"

using namespace std;
void tests() {
    for (auto params : vector<array<int, 3>>{{2, 2, 3}, {3, 2, 3}, {8, 4, 2}}) {
        Schedule s;
        Metrics m;
        int g = params[0], p = params[1], w = params[2];
        require(golfers(g, p, w, 100000, s, m) == Status::solved && golfValid(s, g, p, w),
                "Golf solver failed");
        s[0] = s[1];
        require(!golfValid(s, g, p, w), "Duplicate golfer accepted");
    }
    Schedule s;
    Metrics gm;
    require(golfers(2, 2, 4, 1000, s, gm) == Status::unsat, "Golf bound failed");
    Schedule repeated = {0, 1, 2, 3, 0, 1, 2, 3};
    require(!golfValid(repeated, 2, 2, 2), "Repeated pair accepted");
    // Reject malformed flat schedules, including unused slots and extra values.
    require(!golfValid({0, 1, 2}, 2, 2, 1), "Missing player accepted");
    require(!golfValid({0, 1, 2, 3, 4}, 2, 2, 1), "Extra player accepted");
    require(!golfValid({0, 1, 2, -1}, 2, 2, 1), "Unassigned slot accepted");
    require(!golfValid({0, 1, 2, 4}, 2, 2, 1), "Out-of-range player accepted");
    Metrics tiny;
    require(golfers(8, 4, 3, 1, s, tiny) == Status::limit, "Golf limit ignored");

    // Pair-count bound is necessary but not sufficient: this case needs exhaustive search.
    Metrics exhaustive;
    Schedule none;
    require(golfers(2, 3, 2, 100000, none, exhaustive) == Status::unsat && none.empty(),
            "Exhaustive UNSAT failed");
    Metrics reused;
    Schedule result;
    require(golfers(2, 2, 3, 100000, result, reused) == Status::solved, "Initial solve failed");
    size_t visits = reused.visited;
    require(golfers(2, 2, 3, 100000, result, reused) == Status::solved && reused.visited == visits,
            "Metrics not reset");
    require(golfers(16, 4, 1, 100, result, reused) == Status::solved && golfValid(result, 16, 4, 1),
            "Player 63 mask failed");
    bool rejected = false;
    try {
        golfers(0, 4, 3, 100, result, reused);
    } catch (const invalid_argument &) {
        rejected = true;
    }
    require(rejected, "Invalid API parameters accepted");
    require(!golfValid({}, 0, 4, 3), "Invalid validator parameters accepted");

    // Large cases must use the general search and report UNKNOWN on budget exhaustion.
    for (int weeks : {8, 9, 10}) {
        Schedule interrupted;
        Metrics im;
        require(golfers(8, 4, weeks, 1000, interrupted, im) == Status::limit && interrupted.empty(),
                "General search cutoff mishandled");
        require(im.visited == 1000, "General search budget not enforced");
    }
    Schedule impossible;
    Metrics bound;
    require(golfers(8, 4, 11, 2000000, impossible, bound) == Status::unsat && impossible.empty(),
            "Eleven-week bound failed");
    for (unsigned seed : {0u, 1u, 42u}) {
        Schedule alternative;
        Metrics am;
        require(golfers(3, 2, 3, 2000000, alternative, am, seed) == Status::solved &&
                    golfValid(alternative, 3, 2, 3),
                "Seed changed feasibility");
    }
    // Cross word boundaries, including a partial last word and a third word.
    for (auto params : vector<array<int, 3>>{
             {65, 1, 3}, {17, 4, 1}, {33, 2, 3}, {32, 4, 2}, {65, 2, 2}, {9, 9, 1}}) {
        Schedule large;
        Metrics lm;
        const int g = params[0], p = params[1], w = params[2];
        require(golfers(g, p, w, 1000000, large, lm, 0) == Status::solved &&
                    golfValid(large, g, p, w),
                "Dynamic mask solve failed");
        large.back() = large[static_cast<size_t>(w - 1) * g * p];
        require(!golfValid(large, g, p, w), "Dynamic schedule duplicate accepted");
    }
    Schedule largeLimited;
    Metrics largeMetrics;
    require(golfers(33, 2, 3, 100, largeLimited, largeMetrics) == Status::limit &&
                largeLimited.empty() && largeMetrics.visited == 100,
            "Dynamic mask limit failed");
    require(!golfValid({}, 2147483647, 2, 1), "Overflow dimensions accepted");
    bool overflowRejected = false;
    try {
        golfers(2147483647, 2, 1, 100, result, reused);
    } catch (const invalid_argument &) {
        overflowRejected = true;
    }
    require(overflowRejected, "Solver dimensions overflow accepted");
    cout << "PASS: social_golfers tests\n";
}
