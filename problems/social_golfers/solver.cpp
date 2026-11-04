#include <algorithm>
#include <cstdint>
#include <random>
#include <numeric>
#include <limits>
#include "solver.hpp"
#include "../../shared/common.hpp"

using namespace std;

namespace golf_detail {
template <class Mask> class GroupSearch {
    const int groupCount, groupSize, weekCount, playerCount;
    SearchContext context;
    Schedule &schedule;
    Metrics &stats;
    const Mask everyone;
    vector<Mask> availablePartners;
    vector<vector<int>> candidateOrder;
    int choosePlayer(Mask bits, int week) const {
        for (int player : candidateOrder[week])
            if (containsPlayer(bits, playerCount, player))
                return player;
        return 0;
    }

    // Stop if a remaining player has too few possible partners.
    bool hasEnoughPartners(Mask remainingPlayers) const {
        for (Mask todo = remainingPlayers; todo; removeSmallestPlayer(todo)) {
            int player = smallestPlayer(todo);
            if (numberOfPlayers(sharedPlayers(availablePartners[player], remainingPlayers)) <
                groupSize - 1)
                return false;
        }
        return true;
    }

    // Check whether these players can still form a group without repeated pairs.
    bool canFormGroup(Mask candidatePlayers, int needed) {
        if (!context.enter())
            return false;
        if (!needed)
            return true;
        if (numberOfPlayers(candidatePlayers) < needed)
            return false;
        while (candidatePlayers) {
            int player = smallestPlayer(candidatePlayers);
            removeSmallestPlayer(candidatePlayers);
            if (canFormGroup(sharedPlayers(candidatePlayers, availablePartners[player]),
                             needed - 1))
                return true;
            if (context.stopped || numberOfPlayers(candidatePlayers) < needed)
                break;
        }
        return false;
    }

    // Fill one group; after a failed continuation, undo its meetings.
    Status fillGroup(int week, int group, Mask remainingPlayers, Mask groupMembers,
                     Mask candidatePlayers) {
        if (!context.enter())
            return Status::limit;
        int needed = groupSize - numberOfPlayers(groupMembers);
        if (!needed) {
            const size_t offset =
                static_cast<size_t>(week) * playerCount + static_cast<size_t>(group) * groupSize;
            int seat = 0;
            for (Mask bits = groupMembers; bits; removeSmallestPlayer(bits))
                schedule[offset + seat++] = smallestPlayer(bits);
            for (int i = 0; i < groupSize; ++i)
                availablePartners[schedule[offset + i]] =
                    excludingPlayers(availablePartners[schedule[offset + i]], groupMembers);
            ++stats.generated;
            // Each member must still have a possible group for a later week.
            bool future = true;
            if (week + 1 < weekCount)
                for (int i = 0; i < groupSize; ++i) {
                    if (!canFormGroup(availablePartners[schedule[offset + i]], groupSize - 1)) {
                        future = false;
                        break;
                    }
                }
            // DFS: continue with the next group.
            Status result;
            if (context.stopped)
                result = Status::limit;
            else if (!future)
                result = Status::unsat;
            else
                result =
                    fillWeek(week, group + 1, excludingPlayers(remainingPlayers, groupMembers));
            if (result == Status::solved)
                return result;

            // Backtracking: restore the previous meetings and clear this group.
            for (int i = 0; i < groupSize; ++i) {
                const int player = schedule[offset + i];
                availablePartners[player] = includingPlayers(
                    availablePartners[player],
                    excludingPlayers(groupMembers, onePlayer<Mask>(playerCount, player)));
                schedule[offset + i] = -1;
            }
            ++stats.backtracks;
            return result;
        }
        if (numberOfPlayers(candidatePlayers) < needed)
            return Status::unsat;
        while (candidatePlayers) {
            int player = choosePlayer(candidatePlayers, week);
            Mask bit = onePlayer<Mask>(playerCount, player);
            candidatePlayers = excludingPlayers(candidatePlayers, bit);
            if (numberOfPlayers(candidatePlayers) + 1 < needed)
                break;
            // Keep only candidates who can meet the player just added.
            Status result =
                fillGroup(week, group, remainingPlayers, includingPlayers(groupMembers, bit),
                          sharedPlayers(candidatePlayers, availablePartners[player]));
            if (result != Status::unsat)
                return result;
        }
        return Status::unsat;
    }

    // Move through groups, then start the next week.
    Status fillWeek(int week, int group, Mask remainingPlayers) {
        if (!context.enter())
            return Status::limit;
        if (!remainingPlayers) {
            if (week + 1 == weekCount)
                return Status::solved;
            return fillWeek(week + 1, 0, everyone);
        }
        if (!hasEnoughPartners(remainingPlayers))
            return Status::unsat;
        // Group labels are interchangeable: start with the smallest remaining player.
        int anchor = smallestPlayer(remainingPlayers);
        Mask bit = onePlayer<Mask>(playerCount, anchor);
        return fillGroup(week, group, remainingPlayers, bit,
                         sharedPlayers(remainingPlayers, availablePartners[anchor]));
    }

  public:
    GroupSearch(int g, int p, int w, size_t cap, Schedule &s, Metrics &m, unsigned seed = 0)
        : groupCount(g), groupSize(p), weekCount(w), playerCount(g * p), context(m, cap),
          schedule(s), stats(m), everyone(allPlayers<Mask>(playerCount)),
          availablePartners(playerCount, everyone) {
        candidateOrder.assign(weekCount, vector<int>(playerCount));
        // A fixed seed keeps the candidate order repeatable.
        std::mt19937 engine(seed);
        for (auto &row : candidateOrder) {
            std::iota(row.begin(), row.end(), 0);
            if (seed)
                std::shuffle(row.begin(), row.end(), engine);
        }
        for (int a = 0; a < playerCount; ++a)
            availablePartners[a] =
                excludingPlayers(availablePartners[a], onePlayer<Mask>(playerCount, a));
    }
    Status run() {
        schedule.assign(static_cast<size_t>(weekCount) * playerCount, -1);
        // Any first week can be renamed to these consecutive groups.
        for (int a = 0; a < playerCount; ++a)
            schedule[a] = a;
        for (int group = 0; group < groupCount; ++group) {
            const size_t offset = static_cast<size_t>(group) * groupSize;
            Mask members = noPlayers<Mask>(playerCount);
            for (int i = 0; i < groupSize; ++i)
                members =
                    includingPlayers(members, onePlayer<Mask>(playerCount, schedule[offset + i]));
            for (int i = 0; i < groupSize; ++i)
                availablePartners[schedule[offset + i]] =
                    excludingPlayers(availablePartners[schedule[offset + i]], members);
        }
        if (!context.enter())
            return Status::limit;
        if (weekCount == 1)
            return Status::solved;
        return fillWeek(1, 0, everyone);
    }
};
} // namespace golf_detail

// Entry point: validate dimensions, then run the same search with the appropriate set storage.
Status golfers(int g, int p, int w, size_t cap, Schedule &s, Metrics &m, unsigned seed) {
    m = {};
    s.clear();
    if (g < 1 || p < 1 || w < 1 || g > std::numeric_limits<int>::max() / p || cap == 0)
        throw invalid_argument("Invalid golfers parameters or zero node limit");
    if (static_cast<size_t>(w) > s.max_size() / static_cast<size_t>(g * p))
        throw invalid_argument("Schedule dimensions exceed addressable storage");
    if (p > 1 && w > (g * p - 1) / (p - 1))
        return Status::unsat;
    // In a second week a group can contain at most one member of each old group.
    if (w > 1 && p > g)
        return Status::unsat;
    Status result;
    if (g * p <= 64) {
        golf_detail::GroupSearch<std::uint64_t> search(g, p, w, cap, s, m, seed);
        result = search.run();
    } else {
        golf_detail::GroupSearch<golf_detail::DynamicMask> search(g, p, w, cap, s, m, seed);
        result = search.run();
    }
    if (result != Status::solved)
        s.clear();
    return result;
}
