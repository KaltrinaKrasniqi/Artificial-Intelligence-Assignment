# Search trees — Assignment 1, Part 4

These diagrams are constructed from state transitions, not drawn by guessing.
They are separate educational illustrations; the existing C++ solvers and main
UI are unchanged. All labels are in English.

## Open and regenerate

Open `generated/index.html` directly in a browser. The page embeds its data and
works offline. It supports zoom, node inspection, and step-by-step solution
playback. You can also open the SVGs at full resolution or use the PNGs in a
report.

From the project root:

```powershell
python search_trees/generate.py
```

Python 3.10+ is required. The generator needs only the standard library for
SVG, JSON, verification and HTML. If Pillow is installed, it also writes PNGs.
The supplied generated folder already includes the PNGs. It uses Arial on
Windows and an available default font otherwise.

`verification.json` records the checked solutions. `trees.json` records every
drawn node, its parent, action, state, status, depth and repeated-state reference.
`workforce_dfs_trace.json` contains the complete fixed-order DFS attempt trace
through its first solution (1,948 attempts, plus the root).

## What the colors mean

- **Green:** a node on the returned solution path; in the workforce depth-3
  diagram it is only a prefix of the verified complete solution.
- **Red / INVALID:** the attempted action violates a constraint. It is shown
  for explanation and is not expanded.
- **Yellow / repeat #k:** the same state was already discovered at node #k.
  Do not expand it again. IDs are local to each tree.
- **Dashed / depth cutoff:** the picture stops here. This is not a dead end
  and not proof that no solution exists below it.
- **GOAL:** the complete goal predicate is satisfied.

The underlying state graph may contain cycles. The drawings are search trees
with repeated-state pruning, rather than drawings of the entire cyclic graph.
Node numbers follow breadth-first generation, including displayed invalid
attempts. BFS continues constructing the requested view after finding a goal;
the figure is not a trace that claims to stop immediately at first discovery.
Workforce's full attempt trace uses DFS numbering instead.

## 1. 8-Puzzle

**State:** a tuple of nine values in row order; `0` is the blank. The start
matches the example supplied by the user:

```text
Start         Goal
1 2 3         1 2 3
0 4 6         4 5 6
7 5 8         7 8 0
```

**Actions:** move the blank Up, Down, Left or Right, in that fixed order.
Only on-board moves are generated. Every action swaps the blank with one
orthogonally adjacent tile and costs one.

**Construction:** breadth-first through depth 3, with a global discovered-state
set. Root depth is zero. The result has 27 drawn nodes and a goal at node #23:

```text
#0 --Right--> #3 --Down--> #9 --Right--> #23 (GOAL)
```

After `Up` to #1, moving `Down` returns to the start. That child is #4,
marked `repeat #0`. This is a loop, not a new board to explore. Other gray
leaves at depth 3 may have valid successors; those successors are not shown.

Unit-cost BFS explores every smaller depth first, so the returned three-move
solution is shortest for this start and goal. The independent path check
verifies that every change is a single adjacent blank swap and that tiles are
preserved. This is a bounded illustration, not the entire 8-Puzzle state space.

Files: `puzzle_tree.svg`, `puzzle_tree.png`.

## 2. Missionaries and Cannibals

**Chosen standard instance:** three missionaries, three cannibals, a boat
carrying one or two people. Everyone can operate the boat. The assignment
names the problem without numerical parameters, so these assumptions are
explicit rather than attributed to a more detailed professor specification.

**State:** `(M_left, C_left, boat_side)`. Right-bank counts are inferred as
`3-M_left` and `3-C_left`; `L`/`R` mean left/right.

- Start: `(3,3,L)`.
- Goal: `(0,0,R)`.
- Boat loads, in fixed order: `1M`, `2M`, `1C`, `2C`, `1M1C`.
- A boat must leave from its current bank with available passengers.
- Counts must remain in `0..3`.
- On a bank containing missionaries, cannibals cannot outnumber missionaries.
  A bank with zero missionaries may contain cannibals.

The drawing includes all five attempted loads from each expanded state. An
impossible load or unsafe bank is a red leaf. Valid repeated states are yellow
leaves and are not expanded again. The depth bound is 12; this generated tree
has no unresolved depth cutoffs and has 71 nodes including the invalid/repeat
leaves. Its shortest solution uses 11 crossings and reaches goal node #65.

The first successful branch of that shortest path takes two cannibals over:

```text
(3,3,L) --2C--> (3,1,R) --1C--> (3,2,L)
```

Returning one cannibal puts the boat back on the left with no unsafe bank.
Taking a single missionary from the initial state instead produces `(2,3,R)`:
the left bank is unsafe, so that branch is pruned.

The validator checks passenger availability, boat alternation, boat capacity,
both banks and the final state for every crossing. Unit-cost BFS establishes
that no solution exists at fewer than 11 crossings for this instance.

Files: `river_tree.svg`, `river_tree.png`, and the compact solution path
`river_solution.png`.

## 3. Rotating Workforce Scheduling

**Confirmed model:** the educational instance already in the workspace, not
a universal definition of workforce scheduling. It has four employees, seven
weekdays and one 28-day cyclic sequence of `D` (day), `A` (afternoon), `N`
(night), `O` (off). Employees use weekly offsets of the same sequence:

```text
index(employee, week, day) = ((employee + week) mod 4) * 7 + day
```

The four rows below are consecutive weeks of one shared cycle, not four
independently designed employee schedules. Employee 0 in week 0 uses row 0,
employee 1 uses row 1, etc.; the next week rotates those rows.

**Variables:** `X[0]..X[27]`; initial domain `{D,A,N,O}`; `?` means unassigned.
Assignments are chronological; values are tried in fixed `D,A,N,O` order.

**Constraints:**

- Monday–Saturday: exactly one employee on each of `D,A,N,O`.
- Sunday: one `D`, zero `A`, one `N`, two `O`.
- Each consecutive work block lasts 2–6 days; a rest block lasts 1–3 days.
- `N` immediately followed by `D` is forbidden.
- Blocks and transitions include the last-to-first boundary of the cycle.

Partial checks reject excessive shift counts, impossible remaining coverage,
overlong blocks, short closed blocks, and `N→D`. A short block next to an
unassigned cell is not rejected merely for being short: it may still grow.

**Tree:** the prefix drawing is breadth-first through three assignments.
For example, assigning `N` then `D` is an immediate red leaf. A green `DDD`
prefix is not a complete schedule. The whole raw assignment space has `4^28`
candidates, so the drawing explicitly does not attempt to show the full tree.

**Complete witness:** chronological DFS with backtracking finds:

```text
D D D D D D O
A A A A A O D
N N N O O A N
O O O N N N O
```

The Python result matches the existing C++ Basic solver's solution and 1,948
attempts. Independent complete-schedule validation checks daily coverage in
all four employee rotations, cyclic run lengths and the forbidden transition.
No MRV, forward-checking domain propagation or heuristic ranking is used in
this generator's search. Necessary constraint checks still prune branches.

Files: `workforce_tree.svg`, `workforce_tree.png`, `workforce_solution.png`.
The viewer reveals all 28 decisions on the solution branch separately from
the bounded tree. This is solution playback, not playback of every DFS attempt.


## Compact expansion pages

The default viewer now shows one expanded parent and its immediate children
per page. Previous/Next page and the page selector navigate those expansions
in breadth-first order. Original node IDs, states and colors are preserved.
An extra terminal page shows each goal. One page is a local expansion, not
one complete BFS level; these controls are separate from solution playback.

`generated/puzzle_pages`, `river_pages`, and `workforce_pages` contain separate
SVG/PNG images for each page. Open a folder's `index.html` for all pages, with
one expansion per printed page (browser Print / Save as PDF). Select **Full
tree** in the main viewer to return to the large overview. Cutoff and repeated
leaves retain their original meaning; changing layout does not change search.

## English PDF reports

The three ready-to-read reports are in the `search_trees/pdfs` folder:

- `8-puzzle-report.pdf`: 22 pages, actual FIFO BFS until the first dequeued goal, then the three-move shortest solution.
- `missionaries-and-cannibals-report.pdf`: 28 pages, all 15 BFS node checks, then the 11-crossing shortest solution.
- `rotating-workforce-report.pdf`: 31 pages, the 28 decisions on the returned DFS solution branch, attempted alternatives and the final cyclic schedule. Exhausted side subtrees are collapsed and labeled with their descendant counts; this is not an unabridged rendering of all 498 recursive calls.

Each report starts with the state, initial state, actions, constraints, goal,
strategy and counting conventions. Each diagram page shows one local expansion
or one solution move. Colors identify the returned route, repeated states and
invalid attempts. Machine-readable traces and `report-verification.json` are
saved in `search_trees/generated`.

These BFS reports stop when the goal is dequeued, unlike the earlier bounded
tree previews. In 8-Puzzle, this explores some depth-4 frontier states before
the depth-3 goal is dequeued; consequently its counts differ from that preview.
The reports explicitly separate attempted transitions, discovered states and
processed states. Statistics apply only to the documented instance and order.

To regenerate from the project root (Python with ReportLab installed):

```powershell
python -m pip install reportlab
python search_trees/build_reports.py
```

The generator checks transitions and final solutions before writing the reports.
The delivered PDFs were also rendered and checked page by page for layout.

### Cumulative tree layout

The PDF reports now use landscape A3 pages. Each search step retains all nodes
already generated, starting at the root, in stable positions across pages.
A blue outline marks the current node; blue edges mark its new attempts.
An enlarged local expansion appears below the overview. The final route pages
also retain the full explored tree, progressively highlighting the route.
Use PDF zoom to inspect small overview nodes; the diagrams are vector graphics.
For Workforce, the accumulated tree still collapses exhausted side subtrees as
documented above. Search order, traces, results and statistics are unchanged.
