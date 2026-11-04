#include <algorithm>
#include <set>
#include "validator.hpp"
#include <limits>

using namespace std;
bool golfValid(const Schedule &s, int g, int p, int w) {
    if (g < 1 || p < 1 || w < 1 || g > std::numeric_limits<int>::max() / p)
        return false;
    const int players = g * p;
    if (static_cast<size_t>(w) > s.max_size() / static_cast<size_t>(players))
        return false;
    if (s.size() != static_cast<size_t>(w) * players)
        return false;
    // Keep pairs across all weeks, not just the current one.
    set<pair<int, int>> pairs;
    for (int week = 0; week < w; ++week) {
        vector<int> seen(players);
        for (int group = 0; group < g; ++group) {
            const size_t offset =
                static_cast<size_t>(week) * players + static_cast<size_t>(group) * p;
            for (int seat = 0; seat < p; ++seat) {
                const int player = s[offset + seat];
                if (player < 0 || player >= players || ++seen[player] != 1)
                    return false;
            }
            for (int i = 0; i < p; ++i)
                for (int j = i + 1; j < p; ++j) {
                    // Store both player orders as the same pair.
                    auto key = minmax(s[offset + i], s[offset + j]);
                    if (!pairs.insert(key).second)
                        return false;
                }
        }
        if (any_of(seen.begin(), seen.end(), [](int x) { return x != 1; }))
            return false;
    }
    return true;
}
