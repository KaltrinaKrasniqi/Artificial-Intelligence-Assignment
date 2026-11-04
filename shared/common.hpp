#pragma once
#include <iostream>
#include <stdexcept>
#include <string>
#include <chrono>
#include <cstddef>

// Result of a search
enum class Status {
    solved,
    unsat,
    limit
};

// Statistics
struct Metrics {
    std::size_t visited = 0;
    std::size_t generated = 0;
    std::size_t peak = 0;
    std::size_t backtracks = 0;
    int iterations = 0;
    int maxDepth = 0;
};

// Stop the search when its node budget is reached.
class SearchContext {
    std::size_t nodeLimit;

  public:
    Metrics &metrics;
    bool stopped = false;

    SearchContext(Metrics &statistics, std::size_t limit) : nodeLimit(limit), metrics(statistics) {
        if (!limit)
            throw std::invalid_argument("Node limit must be positive");
    }
    bool enter() {
        if (metrics.visited >= nodeLimit) {
            stopped = true;
            return false;
        }
        ++metrics.visited;
        return true;
    }
};

// Measure elapsed execution time.
class Timer {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

  public:
    double milliseconds() const {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
            .count();
    }
};

// Small helpers used by the command-line programs and tests.
inline void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}

inline int integer(const std::string &input, int minimum, int maximum) {
    std::size_t parsed = 0;
    const int value = std::stoi(input, &parsed);
    if (parsed != input.size() || value < minimum || value > maximum)
        throw std::invalid_argument("Integer outside supported range");
    return value;
}

inline void metrics(const Metrics &statistics, double milliseconds) {
    std::cout << "Visited: " << statistics.visited << "\nGenerated: " << statistics.generated
              << "\nBacktracks: " << statistics.backtracks
              << "\nPeak frontier (BFS): " << statistics.peak << "\nTime (ms): " << milliseconds
              << '\n';
}

inline int report(Status status) {
    if (status == Status::solved) {
        std::cout << "SOLVED\n";
        return 0;
    }
    if (status == Status::unsat) {
        std::cout << "UNSAT\n";
        return 1;
    }
    std::cout << "UNKNOWN: resource limit reached\n";
    return 3;
}
