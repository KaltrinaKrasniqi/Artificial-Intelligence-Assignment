#pragma once
#include <vector>
#include "common.hpp"
// Consecutive groups form a week; consecutive weeks form the schedule.
// Player index: week * (g*p) + group * p + seat. -1 means unassigned.
using Schedule = std::vector<int>;
Status golfers(int, int, int, std::size_t, Schedule &, Metrics &, unsigned seed = 1);

// Technical player-set support. The scheduling algorithm is in solver.cpp.
// A set bit means that a player belongs to the set; no scheduling rules are defined here.
#include <cstdint>
#include <type_traits>
namespace golf_detail {
using Mask = std::uint64_t;
inline int numberOfPlayers(Mask bits) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(bits);
#else
    int total = 0;
    for (; bits; bits &= bits - 1)
        ++total;
    return total;
#endif
}
inline int smallestPlayer(Mask bits) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(bits);
#else
    int index = 0;
    while (!(bits & 1)) {
        bits >>= 1;
        ++index;
    }
    return index;
#endif
}

// More than 64 players use as many 64-bit words as needed.
// The same search retains the single-word representation for <=64 players.
class DynamicMask {
    std::vector<std::uint64_t> words;
    int length;

  public:
    explicit DynamicMask(int n, bool full = false)
        : words((static_cast<std::size_t>(n) + 63) / 64, full ? ~std::uint64_t{0} : 0), length(n) {
        if (full && n % 64)
            words.back() &= (std::uint64_t{1} << (n % 64)) - 1;
    }
    void set(int player) {
        words[player / 64] |= std::uint64_t{1} << (player % 64);
    }
    explicit operator bool() const {
        for (auto word : words)
            if (word)
                return true;
        return false;
    }
    int population() const {
        int total = 0;
        for (auto word : words)
            total += numberOfPlayers(word);
        return total;
    }
    int lowest() const {
        for (std::size_t i = 0; i < words.size(); ++i)
            if (words[i])
                return static_cast<int>(i * 64 + smallestPlayer(words[i]));
        throw std::logic_error("No player in an empty mask");
    }
    void dropLowest() {
        for (auto &word : words)
            if (word) {
                word &= word - 1;
                return;
            }
    }
    DynamicMask &operator&=(const DynamicMask &other) {
        for (std::size_t i = 0; i < words.size(); ++i)
            words[i] &= other.words[i];
        return *this;
    }
    DynamicMask &operator|=(const DynamicMask &other) {
        for (std::size_t i = 0; i < words.size(); ++i)
            words[i] |= other.words[i];
        return *this;
    }
    friend DynamicMask operator&(DynamicMask a, const DynamicMask &b) {
        return a &= b;
    }
    friend DynamicMask operator|(DynamicMask a, const DynamicMask &b) {
        return a |= b;
    }
    DynamicMask operator~() const {
        DynamicMask result = *this;
        for (auto &word : result.words)
            word = ~word;
        if (length % 64)
            result.words.back() &= (std::uint64_t{1} << (length % 64)) - 1;
        return result;
    }
};
inline int numberOfPlayers(const DynamicMask &bits) {
    return bits.population();
}
inline int smallestPlayer(const DynamicMask &bits) {
    return bits.lowest();
}
inline void removeSmallestPlayer(std::uint64_t &bits) {
    bits &= bits - 1;
}
inline void removeSmallestPlayer(DynamicMask &bits) {
    bits.dropLowest();
}
template <class Bits> Bits allPlayers(int players) {
    if constexpr (std::is_same_v<Bits, std::uint64_t>)
        return players == 64 ? ~Bits{0} : (Bits{1} << players) - 1;
    else
        return Bits(players, true);
}
template <class Bits> Bits onePlayer(int players, int player) {
    if constexpr (std::is_same_v<Bits, std::uint64_t>)
        return Bits{1} << player;
    else {
        Bits result(players);
        result.set(player);
        return result;
    }
}
template <class Bits> Bits noPlayers(int players) {
    if constexpr (std::is_same_v<Bits, std::uint64_t>)
        return Bits{0};
    else
        return Bits(players);
}

// These names describe set operations; the bit-level implementation stays here.
template <class Players> Players sharedPlayers(const Players &a, const Players &b) {
    return a & b;
}
template <class Players> Players excludingPlayers(const Players &a, const Players &b) {
    return a & ~b;
}
template <class Players> Players includingPlayers(const Players &a, const Players &b) {
    return a | b;
}
template <class Players> bool containsPlayer(const Players &set, int playerCount, int player) {
    return static_cast<bool>(set & onePlayer<Players>(playerCount, player));
}

} // namespace golf_detail
