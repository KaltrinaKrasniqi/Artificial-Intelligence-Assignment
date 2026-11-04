# Artificial Intelligence Assignment — Uninformed Search

This project implements three constraint problems in C++17. Each problem is described below using the same five parts: model, representation, objective function, evaluation function and landscape.

| Problem | Main search algorithm |
|---|---|
| Social Golfers | DFS with backtracking |
| Sudoku | FIFO BFS with constraint pruning |
| Latin Square | IDDFS with backtracking |

The search algorithms do not rank branches using a heuristic score. Numeric evaluation functions are used by the separate landscape analysis programs. Validators independently check complete solutions.

## UI Preview

The local **Uninformed Search Techniques** interface runs the C++ solvers and displays their independently validated results. The screenshots below show individual runs; execution times vary by machine and workload.

### Latin Square

A solved 6 × 6 square using IDDFS with backtracking: 26 iterations and 1,194,077 visited nodes. The first row and column are fixed by symmetry.

<img src="docs/images/latin-square-ui.png" alt="Latin Square UI showing a validated 6 by 6 solution and IDDFS statistics" width="520" />

### Sudoku

The built-in hard puzzle solved using FIFO BFS with constraint pruning: 18,885 visited states and a peak queue of 1,716. Starting clues remain highlighted.

<img src="docs/images/sudoku-ui.png" alt="Sudoku UI showing the solved hard puzzle and BFS statistics" width="520" />

### Social Golfers

A validated 8-week schedule for 32 players in 8 groups of 4, found using general DFS with backtracking. The screenshot shows week 1 of 8; the week controls allow the full schedule to be explored. This run visited 2,179,221,950 nodes and does not establish that 8 weeks is the maximum.

<img src="docs/images/social-golfers-ui.png" alt="Social Golfers UI showing week 1 of a validated 8-week schedule and search statistics" width="520" />

## 1. Social Golfers

### 1.1 Problem model

Given `g` groups, `p` players per group and `w` weeks, let `n = g*p`. Find a schedule such that:

- Each week has exactly `g` groups of `p` players.
- Every player appears exactly once in every week.
- Every pair of players shares a group at most once across all weeks.

The assignment's instance has `n=32`, `g=8`, `p=4`. For example, a group `[0,1,2,3]` uses six pairs. In later weeks, none of these four players may share a group with another member of that original group.

### 1.2 Representation

The schedule is a **one-dimensional array** of `w*n` player identifiers:

```cpp
using Schedule = std::vector<int>;
// index = week * n + group * p + seat
```

Every four entries form a group and every 32 entries form a week in the assignment's instance. Indices start at zero; index `34` is the third seat of the first group in week two.

| Concept | Definition |
|---|---|
| Variable `X[i]` | Player assigned to schedule position `i` |
| Initial domain | `{0,1,...,n-1}` |
| Legal candidates | Players not assigned that week who have not met any current group member |
| Unassigned marker | `-1`, which is not part of the player domain |
| Search state | Partial schedule, available partners, remaining players and current group |
| Goal | Complete schedule satisfying all constraints |

`availablePartners[a]` records who may still meet player `a`. Bit masks speed up the set operations and expand to multiple words above 64 players. The schedule itself remains a single array.

The first week is fixed to consecutive groups: `[0,1,2,3] [4,5,6,7] ...`. This removes equivalent player labelings without losing feasibility. In week two, `[0,4]` cannot accept `1`, who has met `0`, or `5`, who has met `4`. `[0,4,8,12]` passes the first-week pair checks, although that alone does not guarantee a complete extension.

### 1.3 Objective function

The original goal is **maximize `w`**, subject to the constraints. For `p>1`, each player meets `p-1` new partners per week and has only `n-1` possible partners:

```text
w <= floor((n-1)/(p-1))
For 32 players in groups of four: w <= floor(31/3) = 10.
```

A valid ten-week schedule would prove the upper bound is attained. The bound alone does not prove that such a schedule exists. When `p=1`, the pair constraint imposes no upper bound on weeks.

Our solver addresses **fixed-`w` feasibility**: find a valid schedule for the requested weeks. It does not automatically increase `w` to find the maximum.

### 1.4 Evaluation function

For complete candidate schedules that already satisfy weekly participation and group sizes, let `c(a,b)` count meetings between players `a` and `b`:

```text
E(S) = sum over a<b of max(0, c(a,b)-1)
```

A pair appearing once contributes zero, twice contributes one, and three times contributes two. Therefore `E(S)=0` means a valid solution on this candidate space.

Example with four players in pairs:

```text
Week 1: [0,1] [2,3]     Week 1: [0,1] [2,3]
Week 2: [0,2] [1,3]     Week 2: [0,1] [2,3]
E = 0                   E = 2
```

`landscape.cpp` calculates this numeric penalty. `golfValid()` instead returns a Boolean and also checks completeness, identifiers and weekly participation. A partial schedule is not a solution just because it has no repeated pairs.

### 1.5 Landscape

The landscape uses complete candidate schedules as states and `E(S)` as their height. A neighbor swaps two players from different groups in a week after the fixed first week. This preserves group sizes and weekly participation, while the penalty can improve, remain equal or worsen.

Before constraints there are `n^(w*n)` position assignments. Requiring a permutation each week gives `(n!)^w` position-based schedules. Ignoring player order inside groups and group order inside weeks, weekly partitions number:

```text
A = n! / ((p!)^g * g!)
Candidate schedules: A^w
With the first week fixed: A^(w-1)
```

Weeks remain distinguishable. These counts still include repeated pairs; they are not counts of valid solutions or visited DFS nodes.

For `8 x 4`, `A = 59,287,247,761,257,140,625`:

| Weeks | Candidate partitions `A^w` | First week fixed `A^(w-1)` |
|---|---|---|
| 8 | approximately `1.526477 x 10^158` | approximately `2.574714 x 10^138` |
| 10 | approximately `5.365533 x 10^197` | approximately `9.050063 x 10^177` |

The analysis samples random complete schedules, retains the canonical first week and tests one random swap per sample. Measured results are summarized after the three problem descriptions.

**Connection to DFS:** `run()` initializes week one; `fillWeek()` starts the next group; `fillGroup()` selects its members. On a failed continuation, the code restores available partners and resets the group entries to `-1`. Necessary feasibility checks and symmetry breaking prune branches without heuristic scoring.

## 2. Sudoku

### 2.1 Problem model

Fill a `9 x 9` board with digits `1..9` so that:

- Every row contains each digit once.
- Every column contains each digit once.
- Every `3 x 3` box contains each digit once.
- All original clues remain unchanged.

For example, an empty cell cannot accept `5` if its row, column or box already contains `5`.

### 2.2 Representation

The public board is a **one-dimensional array**:

```cpp
using Board = std::array<int, 81>;
// index = row * 9 + column
```

| Concept | Definition |
|---|---|
| Variables | Initially empty cells |
| Initial domain | `{1,2,...,9}` |
| Legal candidates | Digits absent from the cell's row, column and box |
| Unassigned marker | `0`; fixed clues are not search variables |
| Search state | Compact board, cached masks for 27 units and assignment depth |
| Goal | Complete valid board preserving all clues |

Empty cells are selected in fixed position order and values in ascending order. The compact state keeps queue storage smaller. `containsDigit`, `addDigit` and `removeDigit` isolate bit operations; Sudoku uses bits `1..9` and leaves bit zero unused.

The CLI and UI also offer **Hard+**, a bonus example named `hard_plus`. The original three examples are retained. Measured BFS work with the fixed cell/value order is:

| Example | Clues | Empty cells | Visited states |
|---|---:|---:|---:|
| easy | 30 | 51 | 4,632 |
| medium | 36 | 45 | 69 |
| hard | 23 | 58 | 18,885 |
| hard_plus | 23 | 58 | 117,167 |

The first two names are illustrative and are not ordered by BFS workload. Hard+ is a legal transformation of our existing hard board (row/column reorderings preserving boxes and a digit relabeling), independently checked to have one solution. The transformation changes workload under fixed position-order BFS; it does not establish higher intrinsic or human difficulty. Peak BFS queue length for Hard+ is 14,929 states. No heuristic was added to the delivered solver; the independent uniqueness check was validation only.

### 2.3 Objective function

Find **any complete board satisfying all constraints**. All valid completions are acceptable; the solver does not optimize uniqueness or execution time. On the candidate space below, this can be expressed as minimizing `E(S)` with target zero.

The main BFS algorithm and the meaning of the backtracking concept are explained in Section 2.6. A separate DFS/backtracking mode is retained only for comparison.

### 2.4 Evaluation function

Landscape candidates preserve clues and make every row a permutation of `1..9`. Let `distinct(U)` count different digits in a unit:

```text
E(S) = sum over columns (9-distinct(column))
     + sum over boxes   (9-distinct(box))
```

Each extra occurrence after a digit's first occurrence in a unit contributes one violation. For example, a column with eight distinct digits contributes one. On this candidate space, zero means a valid solution because rows and clues are already correct.

`sudokuSolutionValid()` independently checks the full solution and preservation of clues. The numeric penalty is only for landscape analysis and does not guide BFS.

### 2.5 Landscape

- **State:** complete board preserving clues and valid row permutations.
- **Height:** column and box penalty `E(S)`.
- **Neighbor:** swap two non-clue cells in the same row, preserving clues and row validity.

If there are `b` blanks, the unrestricted clue-preserving space contains `9^b` assignments. If row `r` has `k_r` blanks, its missing digits can be arranged in `k_r!` ways. The smaller space used by the analysis therefore has:

```text
N = product over rows of k_r!
log10(N) = sum over rows of log10(k_r!)
```

For the built-in `easy` board, `log10(N)` is approximately `23.3815`. Column and box violations are still possible. If no row has two blank cells, no permitted swap neighbor exists.

### 2.6 Breadth-First Search (BFS) with Backtracking Concept

The Sudoku solver uses **Breadth-First Search (BFS)** to explore possible assignments level by level, while incorporating the general backtracking concept of abandoning unsuccessful paths and continuing with alternative choices.

Starting from the initial board, the algorithm selects the next empty cell and identifies the values that satisfy the Sudoku constraints. Each valid candidate generates a copy of the current state with that assignment, which is stored in a FIFO queue for later exploration. Cells are selected in fixed order and candidate digits are tried in ascending order; no heuristic score ranks the states.

When a state cannot be extended because no valid value is available for the next cell, that path is abandoned. The search then continues with other previously generated states, allowing alternative assignments to be explored without restarting the search.

This approach follows the general idea behind backtracking—exploring alternatives when a particular choice cannot lead to a solution—while preserving BFS's level-by-level exploration strategy. **In implementation terms, this is constraint pruning, not recursive DFS-style backtracking.** BFS keeps separate queued states, does not undo placements in the parent board, and does not call the separate DFS mode to finish the solution.

The search ends when a complete valid Sudoku board is found, when all possible legal states have been exhausted, or when the configured search limit is reached. These outcomes correspond to `SOLVED`, `UNSAT` and `UNKNOWN`, respectively. The final solution is independently checked by `sudokuSolutionValid()`.

## 3. Latin Square

### 3.1 Problem model

Construct an `n x n` grid with values `1..n`, each appearing exactly once in every row and column. Unlike Sudoku, there are no box constraints.

Example of a valid `3 x 3` square:

```text
1 2 3
2 3 1
3 1 2
```

### 3.2 Representation

The grid uses `std::vector<std::vector<int>>`. Row and column masks cache the values already present.

| Concept | Definition |
|---|---|
| Variables | Cells outside the fixed first row and column: `(n-1)^2` cells |
| Initial domain | `{1,2,...,n}` |
| Legal candidates | Values absent from the cell's row and column |
| Unassigned marker | `0` |
| Search state | Partial grid, row/column masks and assignment depth |
| Goal | Complete grid with no row or column repetitions |

Fixing the first row and column to `1..n` removes equivalent labelings; it does not exclude the existence of a Latin square. Cells are visited in fixed order and values in ascending order. The implementation supports `n=1..8`.

`containsValue`, `addValue` and `removeValue` isolate bit operations; value one uses bit zero. `place()` and `undo()` update both the grid and its masks.

### 3.3 Objective function

Find **any complete valid Latin square** of the requested size. All valid squares are acceptable. On the analysis space, this is equivalent to minimizing `E(S)` with target zero.

IDDFS tries depth limits `0,1,...,(n-1)^2`. Each iteration runs the same depth-limited DFS, placing values and undoing failed placements. An optional node budget accumulates across iterations; without one, the search has no visit cap. Backtracking is part of every depth-limited DFS iteration; there is no separate algorithm choice for Latin Square.

### 3.4 Evaluation function

Landscape candidates already have valid row permutations and the fixed first row and column. Define:

```text
E(S) = sum over columns (n-distinct(column))
```

For example, a column with `n-1` distinct values contributes one violation. Zero is a valid solution on this space. `validate()` independently checks the final square. The penalty does not rank IDDFS branches.

### 3.5 Landscape

- **State:** complete row-permutation grid preserving the fixed first row and column.
- **Height:** column penalty `E(S)`.
- **Neighbor:** swap two unfixed cells within one row, preserving row validity and the fixed cells.

Without row constraints, the unfixed cells have `n^((n-1)^2)` assignments. Each remaining row can instead permute its `n-1` missing values, giving the sampled space:

```text
N = ((n-1)!)^(n-1)
For n=5: N = 24^4 = 331,776, approximately 10^5.520845.
```

These candidates can still violate columns. For `n<3`, a row has fewer than two unfixed cells, so no such swap exists.

## 4. Measured landscape results

Each separate `landscape.cpp` uses [shared/landscape.hpp](shared/landscape.hpp) for sampling and reporting. The search algorithms do not call this analysis. The recorded runs used **1,000 samples, seed 42**:

| Problem and instance | Minimum | Mean | Maximum | Zero-penalty samples | Improving / equal / worsening swaps |
|---|---:|---:|---:|---:|---|
| Golfers, `8 x 4`, 8 weeks | 88 | 107.785 | 132 | 0 | 387 / 232 / 381 |
| Sudoku, `easy` | 34 | 46.386 | 58 | 0 | 394 / 243 / 363 |
| Latin Square, `n=5` | 2 | 6.374 | 11 | 0 | 359 / 307 / 334 |

Full distributions: [Golfers report](outputs/social_golfers_landscape_8weeks.txt), [Sudoku report](outputs/sudoku_landscape_easy.txt), [Latin Square report](outputs/latin_square_landscape_5.txt).

No zero-penalty sample means only that none was observed. It does not prove nonexistence; the search programs have found valid solutions for these cases. One random neighbor per sample does not identify all local minima or plateaus. Penalties use different definitions across problems and should not be compared as a common scale. Exact seeded samples may differ between C++ standard-library implementations.

## 5. Code organization and statistics

```text
problems/{social_golfers,sudoku,latin_square}/
    main.cpp                    input and output
    solver.hpp / solver.cpp     representation and search
    validator.hpp / validator.cpp  independent correctness checks
    tests.cpp                   solver and validator tests
    landscape.cpp               separate analysis
shared/
    common.hpp                  status, statistics, budget, timing and CLI helpers
    landscape.hpp               common sampling and reporting
tests/shared_tests.cpp          shared budget checks
ui/                             local visualization
outputs/                        measured landscape reports
```

Start with `main -> solver -> validator`. For Golfers, follow `golfers -> run -> fillWeek -> fillGroup`; for Sudoku, the FIFO loop is directly in `sudoku()`; for Latin Square, follow `solve -> search`.

Bit masks optimize data representation and checks. They do not introduce heuristic branch ranking. The code uses fixed candidate order or a fixed seeded shuffle, necessary feasibility checks and symmetry breaking. Backtracking restores the previous state after an unsuccessful branch.

`SOLVED` means a solution was found; `UNSAT` means infeasibility was proved for the requested instance; `UNKNOWN` means the budget was exhausted. Latin Square currently signals budget exhaustion through an exception at its API boundary. A valid Golfers schedule for fewer than ten weeks does not prove the maximum number of weeks.

The counters retain their existing meanings:

- `visited`: processed BFS states or DFS entries; Golfers includes auxiliary group-feasibility recursion.
- `generated`: enqueued Sudoku states or committed Golfers groups; Latin Square does not collect this field.
- `backtracks`: reverted assignments/groups. BFS leaves this zero.
- `peak`: maximum BFS queue length, not total memory in bytes.
- `iterations`, `maxDepth`: collected by Latin Square.

The fields are shared, but counts across different problems are not directly interchangeable. Refactor comparisons preserved solutions, statuses and counters; runtime is a separate measurement.

## 6. Build, test and run

Run these commands from the project directory with CMake, Ninja and a C++17 compiler available:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

CMake writes executables to `build/bin`. Supplied executables are in `bin`, which the UI also uses. To update that directory after a build:

```powershell
Copy-Item .\build\bin\*.exe .\bin\ -Force
```

Examples using the supplied executables:

```powershell
.\bin\social_golfers.exe 8 4 3
.\bin\sudoku.exe bfs hard
.\bin\sudoku.exe bfs hard_plus
.\bin\sudoku.exe backtracking hard
.\bin\latin_square.exe 5                # No visit limit
.\bin\latin_square.exe 7 2000000        # Optional visit limit

.\bin\social_golfers_landscape.exe 8 4 8 1000 42
.\bin\sudoku_landscape.exe easy 1000 42
.\bin\latin_square_landscape.exe 5 1000 42
```

Every solver and landscape executable accepts `--test`. Landscape arguments are:

- Golfers: `groups players/group weeks [samples [seed]]`; defaults `8 4 8 1000 42`.
- Sudoku: `[easy|medium|hard|hard_plus|81 digits [samples [seed]]]`; defaults `easy 1000 42`.
- Latin Square: `[n [samples [seed]]]`; defaults `5 1000 42`.

Golfers CLI arguments are `groups players/group weeks [node limit [seed]]`. Its default node budget is 2,000,000; the recorded general-search eight-week result used a separate unlimited-budget runner. Large cases can therefore return `UNKNOWN` in the CLI or UI even when a valid schedule exists. Sudoku accepts an optional node limit after the puzzle. Latin Square accepts `n [visit limit]`; the limit defaults to `0` (no visit limit). A positive limit is optional and accumulates across IDDFS iterations.

## 7. Local UI

Node.js is required (tested with v20). From the project directory:

```powershell
cd ui
npm start
```

Open [http://127.0.0.1:8787/](http://127.0.0.1:8787/), keep the terminal open, and use `Ctrl+C` to stop the server. Running the supplied UI does not require `npm install`: the server uses Node built-ins and compiled JavaScript is included. If the port is occupied, stop the previous instance or set `$env:PORT = '8788'` before starting.

The server runs `latin_square.exe`, `sudoku.exe` and `social_golfers.exe` from the project's `bin` directory. `AI_PROJECT_ROOT` can point to another project. The adapter parses output and independently verifies results. Programs and arguments are restricted, and executable spawning does not use a shell. The UI is local and has not been published.

- Latin Square offers `n=1..8` and always runs IDDFS with backtracking. Its visit limit is optional, with **No visit limit** selected by default. The UI time limit remains separate; larger cases may take a long time.
- Sudoku offers examples and editable clues; difficulty labels are illustrative, not certified rankings.
- Golfers uses general DFS/backtracking. A read-only background-experiment panel does not control that separate process.
- Playback reveals the **final solution gradually**, not the actual search trace.
- UI experiments have selectable time limits of 5..3600 seconds. Cancel stops only the process created by that UI experiment.
- Raw output can be viewed and results downloaded as JSON. Landscape analysis remains a separate CLI tool.

UI files are `src/app.ts` (interaction), `public/index.html`, `public/styles.css`, `public/assets/app.js` (compiled output), `server.mjs` (server/processes), `solver-adapter.mjs` (parsing/validation), and `test/server.test.mjs` (tests).

For TypeScript changes and UI tests, from `ui`:

```powershell
npm ci
npm run build
npm test
npm start
```

Refresh the browser after frontend changes. Restart the server after changing `server.mjs` or `solver-adapter.mjs`.


### Extended UI runs and interactive playback

Use **Set up an 8-week search** in Social Golfers to select 8 groups of 4,
8 weeks, 3 billion visits and a 600-second timeout. Editing the requested
weeks to 8 or more also raises smaller limits to these values. The earlier
8-week general DFS run needed 2,179,221,950 visits; 2 or 20 million visits
cannot reproduce it. Runtime depends on the machine. A visit or time limit
still reports UNKNOWN, and these settings do not guarantee 9 or 10 weeks.
The UI supports up to 10 billion visits and 3,600 seconds. The CLI parses
the visit limit as a platform-sized unsigned count; the search algorithm
and candidate order are unchanged.

Click a golfer to highlight their current group and inspect partners across
all weeks. Click a week in that list to navigate to it. Sudoku and Latin
Square support Play/Pause, Previous row, Next row and Show all with a row
counter. This reveals a completed solution; it is not a trace of the actual
search. The interface and its server messages are in English.
