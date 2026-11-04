#include <algorithm>
#include <map>
#include "solver.hpp"
#include "validator.hpp"
#include "../../shared/landscape.hpp"
#include <cmath>
#include <iomanip>
#include <limits>
#include <numeric>
#include <random>

using namespace std;

namespace {
// Candidates already have the correct group sizes and one appearance per player per week.
// Each occurrence of a pair after its first occurrence contributes one violation.
size_t repeatedPairs(const Schedule &schedule, int groups, int perGroup, int weeks) {
    map<pair<int, int>, size_t> meetings;
    size_t violations = 0;
    const size_t players = static_cast<size_t>(groups) * perGroup;
    for (int week = 0; week < weeks; ++week) {
        for (int group = 0; group < groups; ++group) {
            const size_t start =
                static_cast<size_t>(week) * players + static_cast<size_t>(group) * perGroup;
            for (int i = 0; i < perGroup; ++i) {
                for (int j = i + 1; j < perGroup; ++j) {
                    auto pair = minmax(schedule[start + i], schedule[start + j]);
                    if (meetings[pair]++ > 0)
                        ++violations;
                }
            }
        }
    }
    return violations;
}

Schedule makeCandidate(int players, int weeks, mt19937 &engine) {
    Schedule candidate(static_cast<size_t>(players) * weeks);
    for (int week = 0; week < weeks; ++week) {
        auto begin = candidate.begin() + static_cast<size_t>(week) * players;
        iota(begin, begin + players, 0);
        if (week > 0)
            shuffle(begin, begin + players, engine);
    }
    return candidate;
}

// Preserve the fixed first week and weekly participation while changing two groups.
bool exchange(Schedule &candidate, int groups, int perGroup, int weeks, mt19937 &engine) {
    if (weeks < 2 || groups < 2)
        return false;
    const int week = uniform_int_distribution<int>(1, weeks - 1)(engine);
    const int firstGroup = uniform_int_distribution<int>(0, groups - 1)(engine);
    int secondGroup = uniform_int_distribution<int>(0, groups - 2)(engine);
    if (secondGroup >= firstGroup)
        ++secondGroup;
    const int firstSeat = uniform_int_distribution<int>(0, perGroup - 1)(engine);
    const int secondSeat = uniform_int_distribution<int>(0, perGroup - 1)(engine);
    const size_t start = static_cast<size_t>(week) * groups * perGroup;
    swap(candidate[start + static_cast<size_t>(firstGroup) * perGroup + firstSeat],
         candidate[start + static_cast<size_t>(secondGroup) * perGroup + secondSeat]);
    return true;
}

long double logFactorial(int n) {
    return lgammal(static_cast<long double>(n) + 1) / logl(10.0L);
}

void tests() {
    require(repeatedPairs({0, 1, 2, 3, 0, 2, 1, 3}, 2, 2, 2) == 0,
            "Valid schedule should have zero penalty");
    require(repeatedPairs({0, 1, 2, 3, 0, 1, 2, 3}, 2, 2, 2) == 2,
            "Two repeated pairs should give penalty two");
    require(repeatedPairs({0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3}, 2, 2, 3) == 4,
            "Third occurrences must also be counted");
    mt19937 engine(42);
    for (int i = 0; i < 100; ++i) {
        Schedule candidate = makeCandidate(4, 2, engine);
        require((repeatedPairs(candidate, 2, 2, 2) == 0) == golfValid(candidate, 2, 2, 2),
                "Penalty and independent validator disagree");
        require(exchange(candidate, 2, 2, 2, engine), "Expected a swap neighbor");
        require(equal(candidate.begin(), candidate.begin() + 4, Schedule{0, 1, 2, 3}.begin()),
                "Neighbor changed the fixed first week");
        auto lastWeek = Schedule(candidate.begin() + 4, candidate.end());
        sort(lastWeek.begin(), lastWeek.end());
        require(lastWeek == Schedule({0, 1, 2, 3}), "Neighbor changed weekly participation");
        require((repeatedPairs(candidate, 2, 2, 2) == 0) == golfValid(candidate, 2, 2, 2),
                "Neighbor penalty and validator disagree");
    }
    Schedule singleWeek{0, 1, 2, 3};
    require(!exchange(singleWeek, 2, 2, 1, engine), "A fixed single week has no swap neighbor");
    require(fabsl(logFactorial(4) - 2 * logFactorial(2) - logFactorial(2) - log10l(3)) < 1e-12L,
            "Four players must have three weekly partitions");
    cout << "PASS: landscape penalty, neighbors, validator agreement and space formula\n";
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 2 && string(argv[1]) == "--test") {
            tests();
            return 0;
        }
        if (argc != 1 && (argc < 4 || argc > 6)) {
            cerr
                << "Usage: social_golfers_landscape [groups players/group weeks [samples [seed]]]\n";
            return 2;
        }
        const int maxInt = numeric_limits<int>::max();
        const int groups = argc >= 4 ? integer(argv[1], 1, maxInt) : 8;
        const int perGroup = argc >= 4 ? integer(argv[2], 1, maxInt) : 4;
        const int weeks = argc >= 4 ? integer(argv[3], 1, maxInt) : 8;
        const int samples = argc >= 5 ? integer(argv[4], 1, maxInt) : 1000;
        const unsigned seed = argc >= 6 ? static_cast<unsigned>(integer(argv[5], 0, maxInt)) : 42;
        if (groups > maxInt / perGroup)
            throw invalid_argument("Too many players for integer indexing");
        const int players = groups * perGroup;
        if (static_cast<size_t>(weeks) > Schedule{}.max_size() / players)
            throw invalid_argument("Schedule dimensions exceed addressable storage");

        cout << "Landscape analysis only; DFS is not called or modified.\n"
             << "Players: " << players << ", groups: " << groups << ", group size: " << perGroup
             << ", weeks: " << weeks << ", samples: " << samples << ", seed: " << seed << '\n';
        const long double weekly =
            logFactorial(players) - groups * logFactorial(perGroup) - logFactorial(groups);
        cout << fixed << setprecision(6) << "Weekly partitions A: approximately 10^(" << weekly
             << ")\n"
             << "Candidate schedules A^w: approximately 10^(" << weeks * weekly << ")\n"
             << "With first week fixed A^(w-1): approximately 10^(" << (weeks - 1) * weekly
             << ")\nThese counts still include repeated-pair violations.\n";

        auto report = shared::sampleLandscape(
            samples, seed, [&](auto &engine) { return makeCandidate(players, weeks, engine); },
            [&](const auto &candidate) {
                return repeatedPairs(candidate, groups, perGroup, weeks);
            },
            [&](auto &candidate, auto &engine) {
                return exchange(candidate, groups, perGroup, weeks, engine);
            });
        cout << "Penalty = pair occurrences beyond the first occurrence.\n";
        shared::printLandscape(report);
        return 0;
    } catch (const exception &error) {
        cerr << "Analysis error: " << error.what() << '\n';
        return 2;
    }
}
