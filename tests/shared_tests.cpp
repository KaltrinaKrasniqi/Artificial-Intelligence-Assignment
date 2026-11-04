#include "shared/common.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    Metrics statistics;
    SearchContext context(statistics, 2);
    if (!context.enter() || !context.enter() || context.enter() || statistics.visited != 2 ||
        !context.stopped)
        throw std::runtime_error("Visit budget was not enforced");
    bool rejected = false;
    try {
        SearchContext invalid(statistics, 0);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    if (!rejected)
        throw std::runtime_error("Zero budget accepted");
    std::cout << "PASS: shared budget and invalid-limit handling\n";
}
