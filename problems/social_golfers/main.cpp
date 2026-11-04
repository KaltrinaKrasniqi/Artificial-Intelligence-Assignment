#include "solver.hpp"
#include "validator.hpp"
#include <limits>

using namespace std;
void tests();
int main(int argc, char **argv) {
  try {
    if (argc == 2 && string(argv[1]) == "--test") {
      tests();
      return 0;
    }
    Metrics m;
    Timer timer;
    auto elapsed = [&]() { return timer.milliseconds(); };
    if (argc < 4 || argc > 6) {
      cerr << "Usage: social_golfers <groups> <players/group> <weeks> [limit] "
              "[seed=1]\n";
      return 2;
    }
    int g = integer(argv[1], 1, std::numeric_limits<int>::max()),
        p = integer(argv[2], 1, std::numeric_limits<int>::max()),
        w = integer(argv[3], 1, std::numeric_limits<int>::max());
    std::size_t limit = 2000000;
    if (argc >= 5) {
      const string input = argv[4];
      if (input.empty() ||
          input.find_first_not_of("0123456789") != string::npos)
        throw invalid_argument("Visit limit must be a positive integer");
      const auto parsed = stoull(input);
      if (!parsed || parsed > std::numeric_limits<std::size_t>::max())
        throw out_of_range("Visit limit exceeds this platform's range");
      limit = static_cast<std::size_t>(parsed);
    }
    Schedule s;
    Status status = golfers(
        g, p, w, limit, s, m,
        argc == 6 ? static_cast<unsigned>(integer(argv[5], 0, 2147483647)) : 1);
    double ms = elapsed();
    cout << "Mode: general DFS/backtracking with constraint checks\n";
    if (status == Status::solved) {
      require(golfValid(s, g, p, w), "Golf validation failed");
      for (int week = 0; week < w; ++week) {
        cout << "Week " << week + 1 << ": ";
        for (int group = 0; group < g; ++group) {
          cout << "[ ";
          for (int seat = 0; seat < p; ++seat)
            cout << s[static_cast<size_t>(week) * (g * p) +
                      static_cast<size_t>(group) * p + seat]
                 << ' ';
          cout << "] ";
        }
        cout << '\n';
      }
      cout << "Validation: PASS\n";
      if (p > 1 && w == (g * p - 1) / (p - 1))
        cout << "Optimality: PROVED (valid schedule reaches the mathematical "
                "upper bound)\n";
    }
    metrics(m, ms);
    return report(status);
  } catch (const invalid_argument &e) {
    cerr << "Input error: " << e.what() << '\n';
    return 2;
  } catch (const out_of_range &e) {
    cerr << "Input outside range: " << e.what() << '\n';
    return 2;
  } catch (const exception &e) {
    cerr << e.what() << '\n';
    return 3;
  }
}
