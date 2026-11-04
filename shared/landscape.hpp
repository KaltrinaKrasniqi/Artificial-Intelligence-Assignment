#pragma once
#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>

namespace shared {
struct LandscapeReport {
    std::size_t samples = 0, minimum = std::numeric_limits<std::size_t>::max(), maximum = 0,
                zeros = 0;
    std::size_t improved = 0, unchanged = 0, worsened = 0;
    long double total = 0;
    std::map<std::size_t, std::size_t> frequencies;
};

// Analysis only: this does not select branches or alter a search algorithm.
template <class MakeState, class Evaluate, class Neighbor>
LandscapeReport sampleLandscape(int samples, unsigned seed, MakeState makeState, Evaluate evaluate,
                                Neighbor neighbor) {
    if (samples < 1)
        throw std::invalid_argument("At least one sample is required");
    std::mt19937 engine(seed);
    LandscapeReport report;
    report.samples = static_cast<std::size_t>(samples);
    for (int i = 0; i < samples; ++i) {
        auto state = makeState(engine);
        const std::size_t value = evaluate(state);
        report.minimum = std::min(report.minimum, value);
        report.maximum = std::max(report.maximum, value);
        report.total += value;
        report.zeros += value == 0;
        ++report.frequencies[value];
        if (neighbor(state, engine)) {
            const std::size_t next = evaluate(state);
            if (next < value)
                ++report.improved;
            else if (next == value)
                ++report.unchanged;
            else
                ++report.worsened;
        }
    }
    return report;
}

inline void printLandscape(const LandscapeReport &report) {
    std::cout << std::fixed << std::setprecision(6) << "Minimum penalty: " << report.minimum
              << "\nMean penalty: " << report.total / report.samples
              << "\nMaximum penalty: " << report.maximum
              << "\nZero-penalty samples: " << report.zeros
              << "\nPenalty distribution (penalty: number of samples):\n";
    for (const auto &[value, frequency] : report.frequencies)
        std::cout << "  " << value << ": " << frequency << '\n';
    if (report.improved + report.unchanged + report.worsened)
        std::cout << "One random swap per sample: improved=" << report.improved
                  << ", unchanged=" << report.unchanged << ", worsened=" << report.worsened << '\n';
    else
        std::cout << "No permitted swap neighbor exists for these parameters.\n";
    std::cout << "Sampling does not prove existence, nonexistence or optimality.\n";
}
} // namespace shared
