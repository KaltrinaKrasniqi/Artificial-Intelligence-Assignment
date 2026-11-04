"use strict";
const get = (id) => {
    const element = document.getElementById(id);
    if (!element)
        throw new Error(`Missing ${id}`);
    return element;
};
const numberValue = (id) => Number(get(id).value);
const value = (id) => get(id).value;
const format = (n) => new Intl.NumberFormat('en-US').format(n);
let problem = 'latin', examples = {}, clues = Array(81).fill(0);
let result = null, activeJob = null, visibleRows = 0, week = 0, playing = null;
let selectedPlayer = null;
let lastRequest = null, longRun = null;
const content = {
    latin: {
        title: 'Latin Square',
        eyebrow: '01 / GENERATING GRIDS',
        description: 'Each number appears exactly once in every row and column.',
        rules: ['Numbers from 1 to n', 'No repeated numbers in rows', 'No repeated numbers in columns'],
    },
    sudoku: {
        title: 'Sudoku',
        eyebrow: '02 / CONSTRAINT SATISFACTION',
        description: 'Complete the board while preserving clues and all Sudoku constraints.',
        rules: [
            'Numbers 1–9 in every row',
            'Numbers 1–9 in every column and box',
            'Starting clues never change',
        ],
    },
    golfers: {
        title: 'Social Golfers',
        eyebrow: '03 / GROUP SCHEDULING',
        description: 'Everyone plays every week. No pair shares a group twice.',
        rules: [
            'Every player exactly once each week',
            'Every group has the required size',
            'No repeated pairs across weeks',
        ],
    },
};
// Short summaries of README sections 1–4, not a second implementation of the solvers.
const explanations = {
    golfers: [
        {
            title: 'Problem model',
            points: [
                'Given g groups, p players per group and w weeks, schedule n = g × p players.',
                'Every week contains g groups of exactly p players. Every player appears once that week. No pair shares a group more than once across all weeks.',
            ],
            example: 'For 32 players: 8 groups of 4 play each week. Group [0, 1, 2, 3] uses six pairs; none of those six pairs may share a later group.',
        },
        {
            title: 'Representation: variables and domain',
            points: [
                'Variable X[i]: the player assigned to schedule position i. Domain: player IDs {0, …, n−1}. The marker −1 means unassigned and is outside the domain.',
                'Storage: a one-dimensional vector<int> with w × n entries. The search state also tracks remaining players, the current group and available partners using masks.',
                'A legal candidate is unused in the current week and has never met a current group member. The first week is fixed to remove equivalent player labelings.',
            ],
            formula: 'index = week × n + group × p + seat  (zero-based)',
            example: 'With n = 32 and p = 4, index 34 is seat 3 of group 1 in week 2. After week 1, group [0, 4] cannot add 1 (already met 0) or 5 (already met 4).',
        },
        {
            title: 'Objective function',
            points: [
                'The original optimization objective is to maximize w while satisfying all constraints.',
                'The implemented solver answers a fixed-w feasibility question: does a valid schedule exist for the requested number of weeks? It does not automatically maximize w.',
            ],
            formula: 'For p > 1: w ≤ floor((n−1)/(p−1))\nFor n = 32, p = 4: w ≤ floor(31/3) = 10',
            example: 'A valid 8-week schedule proves feasibility for 8 weeks. It does not prove that 8 is optimal. A valid 10-week schedule would attain the upper bound. For p = 1, pairs impose no week bound.',
        },
        {
            title: 'Evaluation function',
            points: [
                'For complete candidates with correct weekly participation and group sizes, c(a,b) counts how many times players a and b meet.',
                'E(S) counts extra meetings of pairs. E = 0 is a solution on this candidate space. A partial schedule with zero repeated pairs is not yet a solution.',
            ],
            formula: 'E(S) = Σ over a < b of max(0, c(a,b) − 1)',
            example: 'Week 1: [0,1] [2,3]. Repeating those groups in week 2 gives E = 2. Using [0,2] [1,3] instead gives E = 0. golfValid() separately checks the full schedule.',
        },
        {
            title: 'Landscape: space, score and neighbors',
            points: [
                'A landscape state is a complete schedule with the first week fixed. Its height is the repeated-pair penalty E(S). A neighbor swaps two players between different groups in a later week.',
                'A counts weekly partitions, ignoring order inside groups and order of groups. Weeks are distinguishable. These candidates still include repeated pairs; they are not valid-solution counts or DFS visit counts.',
            ],
            formula: 'A = n! / ((p!)^g × g!)\nFirst week fixed: N = A^(w−1)\nFor 8 groups of 4 over 8 weeks: N ≈ 2.574714 × 10^138',
            example: 'Recorded analysis: 1,000 random candidates, seed 42, for 8 × 4 over 8 weeks. E min / mean / max = 88 / 107.785 / 132; zero-score samples = 0. Not sampling a solution does not prove impossibility.',
        },
        {
            title: 'Algorithm: DFS with backtracking',
            points: [
                'Fix week 1, start a group, and try a legal player. Recursively extend that group and subsequent groups and weeks.',
                'If an extension fails, restore the partner sets, reset the group positions and try the next candidate. DFS and backtracking are combined in the same search.',
                'Symmetry and necessary constraint checks remove impossible branches. No heuristic evaluation score ranks them. A valid complete schedule is independently checked.',
            ],
            formula: 'golfers() → run() → fillWeek() → fillGroup()\nChoose → recurse → undo on failure → try another candidate',
            example: 'SOLVED means a validated schedule was found. UNSAT means infeasibility was proved. UNKNOWN means a visit or time limit stopped the search; it does not mean no schedule exists.',
        },
    ],
    sudoku: [
        {
            title: 'Problem model',
            points: [
                'Fill a 9 × 9 board with digits 1–9. Each digit appears exactly once in every row, column and 3 × 3 box.',
                'The original clues are fixed and must remain unchanged.',
            ],
            example: 'An empty cell cannot contain 5 if its row, column or box already contains 5. Sudoku adds box constraints to the row/column rules of a Latin square.',
        },
        {
            title: 'Representation: variables and domain',
            points: [
                'Variables: initially empty cells. Domain: {1, …, 9}. Fixed clues are not search variables; 0 marks an empty cell.',
                'The public board is std::array<int, 81>. Each queued search state contains a compact board, cached row/column/box masks and assignment depth.',
                'Cells are selected in fixed position order; legal digits are tried in ascending order. A legal digit is absent from the row, column and box.',
            ],
            formula: 'index = row × 9 + column  (zero-based)',
            example: 'Index 10 is row 2, column 2. A board with 30 clues has 51 search variables, each initially drawn from digits 1–9 before applying constraints.',
        },
        {
            title: 'Objective function',
            points: [
                'Find any complete valid board preserving the clues. Every valid completion is acceptable.',
                'On the landscape candidate space, this is equivalent to minimizing E(S) with target zero. The solver does not optimize uniqueness or runtime.',
            ],
            formula: 'Find S satisfying every row, column, box and clue constraint.',
            example: 'A complete grid is not enough: it must also satisfy all constraints. sudokuSolutionValid() checks the returned answer independently.',
        },
        {
            title: 'Evaluation function',
            points: [
                'Landscape candidates preserve clues and already make each row a permutation of digits 1–9. The remaining penalty counts missing distinct values in columns and boxes.',
                'E = 0 means a valid solution on this candidate space. This score is used by landscape.cpp, not to order BFS states.',
            ],
            formula: 'E(S) = Σ columns (9 − distinct(column))\n     + Σ boxes   (9 − distinct(box))',
            example: 'A column with only 8 distinct digits contributes 1. A box with 7 distinct digits contributes 2. Rows and clues need no extra penalty here because candidate generation already preserves them.',
        },
        {
            title: 'Landscape: space, score and neighbors',
            points: [
                'A landscape state is a complete board preserving clues and row validity. Its height is E(S). A neighbor swaps two non-clue cells in the same row.',
                'With b blanks, the unrestricted space is 9^b. If row r has k_r blanks, its missing digits have k_r! arrangements. The sampled space is the product below and still includes column and box conflicts.',
            ],
            formula: 'N = ∏ rows k_r!\nlog10(N) = Σ rows log10(k_r!)\nBuilt-in easy board: N ≈ 10^23.3815',
            example: 'Recorded easy-board analysis: 1,000 samples, seed 42; E min / mean / max = 34 / 46.386 / 58, with 0 zero-score samples. This does not prove there is no solution. If no row has two blanks, this swap neighbor does not exist.',
        },
        {
            title: 'Algorithm: BFS with the backtracking concept',
            points: [
                'A FIFO queue explores partial boards level by level. For the next empty cell, each legal digit creates a separate child state added to the queue.',
                'A state with no legal continuation is discarded and the search continues with alternatives already queued. This is constraint pruning: the general backtracking idea, not recursive DFS-style undo.',
                'The BFS solution is not completed by calling DFS. The optional recursive DFS mode is a separate comparison. No heuristic score ranks BFS states.',
            ],
            formula: 'Dequeue oldest state → choose next empty cell\nCopy and enqueue each legal assignment → discard dead ends',
            example: 'On the recorded Hard+ case, BFS visited 117,167 states and its queue peaked at 14,929. A limit gives UNKNOWN; only exhaustive failure gives UNSAT.',
        },
    ],
    latin: [
        {
            title: 'Problem model',
            points: [
                'Construct an n × n grid using values 1–n. Every value appears exactly once in each row and column.',
                'There are no box constraints. The implementation supports sizes n = 1…8.',
            ],
            formula: 'A valid 3 × 3 example:\n1 2 3\n2 3 1\n3 1 2',
            example: 'The same number may appear in different rows and columns, but never twice within one row or within one column.',
        },
        {
            title: 'Representation: variables and domain',
            points: [
                'Storage: std::vector<std::vector<int>>. Row and column masks cache the values already present; 0 marks an unassigned cell.',
                'The first row and column are fixed to 1–n to remove equivalent labelings. Variables are the remaining (n−1)^2 cells. Domain: {1, …, n}.',
                'The state contains the partial grid, row/column masks and assignment depth. Legal values are absent from the cell’s row and column. Cells and values follow fixed order.',
            ],
            formula: 'Variable X(r,c), with r > 0 and c > 0\nLegal domain = {1, …, n} minus values used in row r or column c',
            example: 'For n = 5, 9 cells in the first row or column are fixed and 16 cells remain variables. The code updates both grid and masks with place() and undo().',
        },
        {
            title: 'Objective function',
            points: [
                'Find any complete valid Latin square of the requested size. Every valid square is acceptable.',
                'On the landscape candidate space, the target is E(S) = 0. A resource limit can prevent the search from finishing even when a square exists.',
            ],
            formula: 'Find S with every row and every column a permutation of 1…n.',
            example: 'UNKNOWN under a selected visit or time limit is not evidence that a Latin square of that size is impossible.',
        },
        {
            title: 'Evaluation function',
            points: [
                'Landscape candidates already have valid row permutations and preserve the fixed first row and column. Only column violations remain to be scored.',
                'E = 0 is a valid solution on this space. validate() separately checks the final answer. This score does not guide IDDFS.',
            ],
            formula: 'E(S) = Σ columns (n − distinct(column))',
            example: 'For n = 5, a column with 4 distinct values contributes 1 violation. If all five columns contain all five values, E = 0.',
        },
        {
            title: 'Landscape: space, score and neighbors',
            points: [
                'A state is a complete row-permutation grid preserving the first row and column. Its height is the column penalty E(S).',
                'A neighbor swaps two unfixed cells within one row. Each remaining row can arrange its n−1 missing values in (n−1)! ways. These candidates still include column conflicts.',
            ],
            formula: 'Unrestricted unfixed cells: n^((n−1)^2)\nSampled space: N = ((n−1)!)^(n−1)\nFor n = 5: N = 24^4 = 331,776 ≈ 10^5.520845',
            example: 'Recorded n = 5 analysis: 1,000 samples, seed 42; E min / mean / max = 2 / 6.374 / 11, with 0 zero-score samples. This is not a proof of nonexistence. For n < 3, there is no pair of unfixed cells to swap in a row.',
        },
        {
            title: 'Algorithm: IDDFS with backtracking',
            points: [
                'Try depth limits 0, 1, …, (n−1)^2. Every iteration runs a depth-limited DFS.',
                'Place a legal value, recurse, and undo the placement on failure. Backtracking is part of each IDDFS iteration; these are not two separate solution modes.',
                'If selected, the visit budget accumulates across iterations; otherwise there is no visit cap. No heuristic score ranks branches, and there is no fallback to ordinary DFS.',
            ],
            formula: 'solve() → increasing depth limit → search(depth, limit)\nplace() → recurse → undo() on failure',
            example: 'For n = 5, the goal has 16 unfixed cells. The recorded solve tried limits 0…16: 17 iterations and 4,914 visited nodes, followed by independent validation.',
        },
    ],
};
// Native MathML keeps formulas typeset and available offline, without a math library.
// Only these fixed, developer-authored strings become markup; user input never does.
const typesetFormulas = {
    golfers: {
        1: '<math><mi>i</mi><mo>=</mo><mi>week</mi><mo>·</mo><mi>n</mi><mo>+</mo><mi>group</mi><mo>·</mo><mi>p</mi><mo>+</mo><mi>seat</mi></math>',
        2: '<math><mi>w</mi><mo>≤</mo><mo>⌊</mo><mfrac><mrow><mi>n</mi><mo>−</mo><mn>1</mn></mrow><mrow><mi>p</mi><mo>−</mo><mn>1</mn></mrow></mfrac><mo>⌋</mo></math><math><mi>w</mi><mo>≤</mo><mo>⌊</mo><mfrac><mn>31</mn><mn>3</mn></mfrac><mo>⌋</mo><mo>=</mo><mn>10</mn></math>',
        3: '<math><mi>E</mi><mo>(</mo><mi>S</mi><mo>)</mo><mo>=</mo><munder><mo>∑</mo><mrow><mi>a</mi><mo>&lt;</mo><mi>b</mi></mrow></munder><mo>max</mo><mo>(</mo><mn>0</mn><mo>,</mo><mi>c</mi><mo>(</mo><mi>a</mi><mo>,</mo><mi>b</mi><mo>)</mo><mo>−</mo><mn>1</mn><mo>)</mo></math>',
        4: '<math><mi>A</mi><mo>=</mo><mfrac><mrow><mi>n</mi><mo>!</mo></mrow><mrow><msup><mrow><mo>(</mo><mi>p</mi><mo>!</mo><mo>)</mo></mrow><mi>g</mi></msup><mo>·</mo><mi>g</mi><mo>!</mo></mrow></mfrac></math><math><mi>N</mi><mo>=</mo><msup><mi>A</mi><mrow><mi>w</mi><mo>−</mo><mn>1</mn></mrow></msup></math><span class="equation-caption">32 players · 8 groups · 8 weeks · first week fixed</span><math><mi>N</mi><mo>≈</mo><mn>2.574714</mn><mo>×</mo><msup><mn>10</mn><mn>138</mn></msup></math>',
    },
    sudoku: {
        1: '<math><mi>i</mi><mo>=</mo><mn>9</mn><mi>r</mi><mo>+</mo><mi>c</mi></math><span class="equation-caption">Zero-based row r and column c</span>',
        3: '<math><mi>E</mi><mo>(</mo><mi>S</mi><mo>)</mo><mo>=</mo><munder><mo>∑</mo><mtext>columns</mtext></munder><mo>(</mo><mn>9</mn><mo>−</mo><mi>distinct</mi><mo>(</mo><mi>column</mi><mo>)</mo><mo>)</mo></math><math><mo>+</mo><munder><mo>∑</mo><mtext>boxes</mtext></munder><mo>(</mo><mn>9</mn><mo>−</mo><mi>distinct</mi><mo>(</mo><mi>box</mi><mo>)</mo><mo>)</mo></math>',
        4: '<math><mi>N</mi><mo>=</mo><munder><mo>∏</mo><mtext>rows r</mtext></munder><msub><mi>k</mi><mi>r</mi></msub><mo>!</mo></math><math><msub><mo>log</mo><mn>10</mn></msub><mo>(</mo><mi>N</mi><mo>)</mo><mo>=</mo><munder><mo>∑</mo><mtext>rows r</mtext></munder><msub><mo>log</mo><mn>10</mn></msub><mo>(</mo><msub><mi>k</mi><mi>r</mi></msub><mo>!</mo><mo>)</mo></math><span class="equation-caption">Built-in easy board · sampled candidate space</span><math><mi>N</mi><mo>≈</mo><msup><mn>10</mn><mn>23.3815</mn></msup></math>',
    },
    latin: {
        0: '<math><mo>(</mo><mtable columnspacing="1.2em" rowspacing="0.45em"><mtr><mtd><mn>1</mn></mtd><mtd><mn>2</mn></mtd><mtd><mn>3</mn></mtd></mtr><mtr><mtd><mn>2</mn></mtd><mtd><mn>3</mn></mtd><mtd><mn>1</mn></mtd></mtr><mtr><mtd><mn>3</mn></mtd><mtd><mn>1</mn></mtd><mtd><mn>2</mn></mtd></mtr></mtable><mo>)</mo></math>',
        1: '<math><msub><mi>X</mi><mrow><mi>r</mi><mo>,</mo><mi>c</mi></mrow></msub><mo>∈</mo><mo>{</mo><mn>1</mn><mo>,</mo><mo>…</mo><mo>,</mo><mi>n</mi><mo>}</mo><mspace width="1em"/><mi>r</mi><mo>,</mo><mi>c</mi><mo>&gt;</mo><mn>0</mn></math><span class="equation-caption">Exclude values already used in the same row or column.</span>',
        3: '<math><mi>E</mi><mo>(</mo><mi>S</mi><mo>)</mo><mo>=</mo><munder><mo>∑</mo><mtext>columns</mtext></munder><mo>(</mo><mi>n</mi><mo>−</mo><mi>distinct</mi><mo>(</mo><mi>column</mi><mo>)</mo><mo>)</mo></math>',
        4: '<span class="equation-caption">Unrestricted assignments to unfixed cells</span><math><msup><mi>n</mi><msup><mrow><mo>(</mo><mi>n</mi><mo>−</mo><mn>1</mn><mo>)</mo></mrow><mn>2</mn></msup></msup></math><span class="equation-caption">Sampled row-valid candidate space</span><math><mi>N</mi><mo>=</mo><msup><mrow><mo>(</mo><mo>(</mo><mi>n</mi><mo>−</mo><mn>1</mn><mo>)</mo><mo>!</mo><mo>)</mo></mrow><mrow><mi>n</mi><mo>−</mo><mn>1</mn></mrow></msup></math><math><mi>n</mi><mo>=</mo><mn>5</mn><mo>:</mo><mspace width="0.5em"/><mi>N</mi><mo>=</mo><msup><mn>24</mn><mn>4</mn></msup><mo>=</mo><mn>331,776</mn><mo>≈</mo><msup><mn>10</mn><mn>5.520845</mn></msup></math>',
    },
};
function renderFormula(topic, index) {
    const box = document.createElement('div');
    box.className = 'guide-formula';
    const label = document.createElement('div');
    label.className = 'formula-label';
    label.textContent = index === 5 ? 'SEARCH FLOW' : 'THE MATHEMATICS';
    box.append(label);
    const markup = typesetFormulas[problem][index];
    if (markup) {
        const equations = document.createElement('div');
        equations.className = 'equations';
        equations.innerHTML = markup;
        if (index !== 0) {
            equations.querySelectorAll('mo').forEach((operator) => {
                if (operator.textContent === '(' || operator.textContent === ')')
                    operator.setAttribute('stretchy', 'false');
            });
        }
        equations.querySelectorAll('math').forEach((math) => math.setAttribute('display', 'block'));
        box.append(equations);
    }
    else {
        for (const line of (topic.formula || '').split('\n')) {
            const step = document.createElement('div');
            step.className = index === 5 ? 'search-flow' : 'equation-caption';
            step.textContent = line;
            box.append(step);
        }
    }
    return box;
}
function renderExplanations() {
    get('guide-title').textContent = `${content[problem].title}: from model to search`;
    const topics = get('guide-topics');
    topics.replaceChildren();
    for (const [index, topic] of explanations[problem].entries()) {
        const details = document.createElement('details');
        details.className = 'guide-topic';
        details.open = index === 0;
        const summary = document.createElement('summary');
        const number = document.createElement('span');
        number.className = 'topic-number';
        number.textContent = String(index + 1).padStart(2, '0');
        const title = document.createElement('span');
        title.textContent = topic.title;
        summary.append(number, title);
        details.append(summary);
        for (const point of topic.points) {
            const paragraph = document.createElement('p');
            paragraph.textContent = point
                .replaceAll('9^b', '9ᵇ')
                .replaceAll('k_r', 'kᵣ')
                .replaceAll('(n−1)^2', '(n−1)²');
            details.append(paragraph);
        }
        if (topic.formula) {
            details.append(renderFormula(topic, index));
        }
        const example = document.createElement('p');
        example.className = 'guide-example';
        example.textContent = topic.example;
        details.append(example);
        details.addEventListener('toggle', updateGuideToggle);
        topics.append(details);
    }
    updateGuideToggle();
}
function updateGuideToggle() {
    const topics = Array.from(get('guide-topics').querySelectorAll('details'));
    get('guide-toggle').textContent = topics.every((topic) => topic.open)
        ? 'Collapse all'
        : 'Expand all';
}
get('guide-toggle').addEventListener('click', () => {
    const topics = Array.from(get('guide-topics').querySelectorAll('details'));
    const open = topics.some((topic) => !topic.open);
    topics.forEach((topic) => {
        topic.open = open;
    });
    updateGuideToggle();
});
async function api(url, options = {}) {
    const response = await fetch(url, {
        ...options,
        headers: { 'Content-Type': 'application/json', ...options.headers },
    });
    const data = await response.json();
    if (!response.ok)
        throw new Error(data.error || 'Could not connect to the server.');
    return data;
}
function stopPlaying() {
    if (playing !== null)
        window.clearInterval(playing);
    playing = null;
    get('play').textContent = 'Play';
}
function setBanner(text, error = false) {
    const banner = get('run-banner');
    banner.hidden = !text;
    banner.textContent = text;
    banner.classList.toggle('error', error);
}
// Brief visual transitions only; no animation represents actual search progress.
function revealPanel(selector) {
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches)
        return;
    document.querySelector(selector)?.animate([
        { opacity: 0.5, transform: 'translateY(5px)' },
        { opacity: 1, transform: 'translateY(0)' },
    ], { duration: 240, easing: 'ease-out' });
}
function setStatus(text, kind = '') {
    const badge = get('status-badge');
    badge.textContent = text;
    badge.className = 'status-badge ' + kind;
    document.body.classList.toggle('searching', kind === 'running');
}
function busy(yes) {
    get('solve-button').disabled = yes;
    get('reset-button').disabled = yes;
    get('cancel-button').hidden = !yes;
    document
        .querySelectorAll('[data-tab]')
        .forEach((button) => (button.disabled = yes));
    get('solve-form')
        .querySelectorAll('input,select')
        .forEach((input) => (input.disabled = yes));
}
function metrics() {
    const m = result?.metrics || {};
    const ms = m['Time (ms)'];
    get('metric-time').textContent =
        ms === undefined
            ? '—'
            : ms < 1000
                ? `${ms.toLocaleString('en-US', { maximumFractionDigits: 3 })} ms`
                : `${(ms / 1000).toLocaleString('en-US', { maximumFractionDigits: 2 })} s`;
    get('metric-nodes').textContent = m.Visited === undefined ? '—' : format(m.Visited);
    let title = 'Backtracks', hint = 'Assignments undone', metric = m.Backtracks;
    if (problem === 'latin') {
        title = 'IDDFS iterations';
        hint = 'Depth limits attempted';
        metric = m.Iterations;
    }
    if (problem === 'sudoku' && value('sudoku-algorithm') === 'bfs') {
        title = 'Peak BFS queue';
        hint = 'States waiting at the same time';
        metric = m['Peak frontier (BFS)'];
    }
    get('metric-third-title').textContent = title;
    get('metric-third-hint').textContent = hint;
    get('metric-third').textContent = metric === undefined ? '—' : format(metric);
    const verified = result?.verified && result.status === 'SOLVED';
    get('metric-validation').textContent = verified
        ? '✓ Valid'
        : result
            ? 'No verified solution'
            : 'Waiting';
    get('metric-validation').parentElement.classList.toggle('valid', Boolean(verified));
    get('metric-validation-hint').textContent = verified
        ? 'Both C++ and UI validation passed'
        : 'Independent constraint checks';
}
function clearResult() {
    stopPlaying();
    result = null;
    lastRequest = null;
    visibleRows = 0;
    week = 0;
    selectedPlayer = null;
    get('player-details').hidden = true;
    setBanner('');
    setStatus('Ready');
    get('presentation').hidden = true;
    get('raw-panel').hidden = true;
    metrics();
}
function modeNote() {
    const g = numberValue('golf-g'), p = numberValue('golf-p');
    get('golf-total').textContent = format(g * p || 0);
    get('golf-mode').textContent =
        `General DFS with backtracking. This run allows ${format(numberValue('golf-budget'))} visits and ${numberValue('time-limit')} seconds. Reaching either limit means UNKNOWN, not UNSAT.`;
}
function renderGrid() {
    const container = get('grid-board');
    container.replaceChildren();
    const n = problem === 'sudoku' ? 9 : Math.min(8, Math.max(1, numberValue('latin-size') || 1));
    container.style.setProperty('--n', String(n));
    container.style.setProperty('--board-width', `${Math.min(n * 56, 450)}px`);
    container.setAttribute('aria-label', `${content[problem].title}, ${n} rows and ${n} columns`);
    for (let r = 0; r < n; r++)
        for (let c = 0; c < n; c++) {
            const initial = problem === 'sudoku' ? clues[r * 9 + c] : r === 0 ? c + 1 : c === 0 ? r + 1 : 0;
            const solution = result?.status === 'SOLVED' ? result.grid?.[r]?.[c] : undefined;
            const shown = solution !== undefined && r < visibleRows ? solution : initial;
            const cell = document.createElement('div');
            cell.className = 'cell';
            cell.style.setProperty('--reveal-delay', `${r * 18}ms`);
            if (initial)
                cell.classList.add('fixed');
            if (c === n - 1)
                cell.classList.add('last-col');
            if (r === n - 1)
                cell.classList.add('last-row');
            if (problem === 'sudoku') {
                if (c === 2 || c === 5)
                    cell.classList.add('block-right');
                if (r === 2 || r === 5)
                    cell.classList.add('block-bottom');
                const input = document.createElement('input');
                input.type = 'text';
                input.inputMode = 'numeric';
                input.maxLength = 1;
                input.setAttribute('aria-label', `Row ${r + 1}, column ${c + 1}`);
                input.value = shown ? String(shown) : '';
                input.disabled = Boolean(activeJob) || result?.status === 'SOLVED';
                input.addEventListener('input', () => {
                    const cleaned = input.value.replace(/[^1-9]/g, '').slice(-1);
                    input.value = cleaned;
                    clues[r * 9 + c] = Number(cleaned) || 0;
                    get('sudoku-level').value = 'custom';
                    clearResult();
                    cell.classList.toggle('fixed', Boolean(cleaned));
                });
                cell.append(input);
            }
            else
                cell.textContent = shown ? String(shown) : '';
            container.append(cell);
        }
    get('step-label').textContent = `Revealed rows: ${visibleRows} / ${n}`;
    get('previous-step').disabled = visibleRows === 0;
    get('board-count').textContent =
        problem === 'sudoku'
            ? `${clues.filter(Boolean).length} clues · 81 cells`
            : `${n * n} cells · ${(n - 1) ** 2} decisions`;
}
function renderGolf() {
    const container = get('golf-board');
    container.replaceChildren();
    const groups = result?.schedule?.[week];
    const g = groups?.length || Math.min(16, Math.max(1, numberValue('golf-g') || 1));
    const p = groups?.[0]?.length || Math.min(8, Math.max(1, numberValue('golf-p') || 1));
    for (let k = 0; k < g; k++) {
        const card = document.createElement('div');
        card.className = 'group-card';
        card.style.setProperty('--reveal-delay', `${Math.min(k, 10) * 20}ms`);
        const title = document.createElement('small');
        title.textContent = `GROUP ${String(k + 1).padStart(2, '0')}`;
        const players = document.createElement('div');
        players.className = 'group-players';
        for (let i = 0; i < p; i++) {
            const player = document.createElement(groups ? 'button' : 'span');
            player.className = 'player';
            player.textContent = groups ? String(groups[k][i]) : '·';
            if (groups) {
                const id = groups[k][i];
                player.setAttribute('type', 'button');
                player.setAttribute('aria-label', `Track player ${id}`);
                player.setAttribute('aria-pressed', String(selectedPlayer === id));
                player.classList.toggle('selected', selectedPlayer === id);
                player.classList.toggle('partner', selectedPlayer !== null && groups[k].includes(selectedPlayer) && id !== selectedPlayer);
                player.addEventListener('click', () => {
                    selectedPlayer = selectedPlayer === id ? null : id;
                    renderGolf();
                });
            }
            players.append(player);
        }
        card.append(title, players);
        container.append(card);
    }
    const details = get('player-details');
    details.replaceChildren();
    details.hidden = selectedPlayer === null || !result?.schedule;
    if (selectedPlayer !== null && result?.schedule) {
        const heading = document.createElement('strong');
        heading.textContent = `Player ${selectedPlayer} · partners by week`;
        const list = document.createElement('ol');
        const seen = new Set();
        for (const [index, groupsInWeek] of result.schedule.entries()) {
            const group = groupsInWeek.find((group) => group.includes(selectedPlayer)) || [];
            const partners = group.filter((id) => id !== selectedPlayer);
            const repeated = partners.some((id) => seen.has(id));
            partners.forEach((id) => seen.add(id));
            const item = document.createElement('li');
            const button = document.createElement('button');
            button.type = 'button';
            button.className = 'small-button';
            button.textContent = `Week ${index + 1}: ${partners.join(', ') || 'no partners'} ${repeated ? '— repeated pair' : '— no repeated pair'}`;
            button.setAttribute('aria-current', String(index === week));
            button.addEventListener('click', () => {
                week = index;
                renderGolf();
            });
            item.append(button);
            list.append(item);
        }
        details.append(heading, list);
    }
    const totalWeeks = result?.schedule?.length || 0;
    get('golf-week-controls').hidden = !totalWeeks;
    get('week-range').max = String(totalWeeks || 1);
    get('week-range').value = String(week + 1);
    get('week-label').textContent = `Week ${week + 1} / ${totalWeeks}`;
    get('previous-week').disabled = week === 0;
    get('next-week').disabled = week >= totalWeeks - 1;
    get('board-count').textContent =
        `${g * p} players · ${totalWeeks || numberValue('golf-w')} weeks`;
}
function renderBoard() {
    get('grid-board').hidden = problem === 'golfers';
    get('golf-board').hidden = problem !== 'golfers';
    get('golf-week-controls').hidden = true;
    get('board-title').textContent = problem === 'golfers' ? 'Player schedule' : 'Problem board';
    get('board-subtitle').textContent =
        problem === 'golfers'
            ? 'Player IDs start at 0. Everyone plays once each week.'
            : problem === 'sudoku'
                ? 'Clues are preserved. You can edit the board before running.'
                : 'The first row and column are fixed by symmetry.';
    get('board-legend').textContent =
        problem === 'golfers'
            ? 'Click a player to track their partners across weeks'
            : '■  Fixed cells use a different color';
    if (problem === 'golfers')
        renderGolf();
    else
        renderGrid();
}
function setProblem(next) {
    if (activeJob)
        return;
    problem = next;
    clearResult();
    document.body.dataset.problem = problem;
    const info = content[problem];
    renderExplanations();
    get('problem-title').textContent = info.title;
    get('problem-description').textContent = info.description;
    get('problem-eyebrow').textContent = info.eyebrow;
    for (const name of ['latin', 'sudoku', 'golfers'])
        get(`${name}-controls`).hidden = problem !== name;
    document.querySelectorAll('[data-tab]').forEach((button) => {
        const selected = button.dataset.tab === problem;
        button.classList.toggle('active', selected);
        button.setAttribute('aria-pressed', String(selected));
    });
    const list = get('constraint-list');
    list.replaceChildren();
    for (const rule of info.rules) {
        const li = document.createElement('li');
        li.textContent = rule;
        list.append(li);
    }
    get('long-run-panel').hidden = problem !== 'golfers';
    get('form-error').hidden = true;
    history.replaceState(null, '', `#${problem}`);
    modeNote();
    renderBoard();
    metrics();
    if (problem === 'golfers')
        void updateLongRun();
    revealPanel('.experiment');
}
function applyResult(next) {
    result = next;
    visibleRows = next.grid?.length || 0;
    week = 0;
    setStatus(next.status === 'SOLVED'
        ? 'Solved'
        : next.status === 'UNSAT'
            ? 'Unsatisfiable'
            : next.status === 'UNKNOWN'
                ? 'Unknown'
                : 'Error', next.status === 'SOLVED'
        ? 'solved'
        : next.status === 'UNSAT' || next.status === 'ERROR'
            ? 'error'
            : 'unknown');
    let message = next.message;
    if (next.status === 'UNSAT')
        message = 'The program proved that no solution exists for these parameters.';
    else if (next.optimalWeeks)
        message =
            'This valid schedule reaches the mathematical upper bound: the number of weeks is optimal.';
    setBanner(message, next.status !== 'SOLVED');
    get('raw-output').textContent = next.raw;
    get('raw-panel').hidden = false;
    get('presentation').hidden = !(next.status === 'SOLVED' && next.grid);
    renderBoard();
    metrics();
    revealPanel('.result-panel');
}
function collect() {
    const seconds = numberValue('time-limit');
    if (problem === 'latin')
        return { problem, seconds, n: numberValue('latin-size'), budget: numberValue('latin-budget') };
    if (problem === 'sudoku')
        return {
            problem,
            seconds,
            puzzle: clues.join(''),
            algorithm: value('sudoku-algorithm'),
            budget: numberValue('sudoku-budget'),
        };
    return {
        problem,
        seconds,
        g: numberValue('golf-g'),
        p: numberValue('golf-p'),
        w: numberValue('golf-w'),
        budget: numberValue('golf-budget'),
    };
}
get('solve-form').addEventListener('submit', async (event) => {
    event.preventDefault();
    if (activeJob)
        return;
    clearResult();
    get('form-error').hidden = true;
    try {
        const request = collect();
        lastRequest = request;
        busy(true);
        setStatus('Searching', 'running');
        setBanner('The C++ program is searching…');
        const firstJob = await api('/api/solve', {
            method: 'POST',
            body: JSON.stringify(request),
        });
        activeJob = firstJob.id;
        renderBoard();
        let job = firstJob;
        while (job.state === 'running') {
            await new Promise((resolve) => window.setTimeout(resolve, 250));
            job = await api(`/api/jobs/${firstJob.id}`);
            if (job.state === 'running')
                setBanner(`C++ search running… ${Math.floor((Date.now() - job.started) / 1000)} s. Time limit for this run: ${request.seconds} s.`);
        }
        activeJob = null;
        busy(false);
        if (job.result)
            applyResult(job.result);
    }
    catch (error) {
        activeJob = null;
        busy(false);
        setStatus('Error', 'error');
        setBanner('');
        const box = get('form-error');
        box.textContent = error instanceof Error ? error.message : 'The run failed.';
        box.hidden = false;
        renderBoard();
    }
});
get('cancel-button').addEventListener('click', () => {
    if (activeJob)
        void api(`/api/jobs/${activeJob}/cancel`, { method: 'POST' }).catch(() => { });
});
get('reset-button').addEventListener('click', () => {
    if (!activeJob) {
        clearResult();
        get('form-error').hidden = true;
        renderBoard();
    }
});
document.querySelector('.brand')?.addEventListener('click', (event) => {
    event.preventDefault();
    setProblem('latin');
});
document
    .querySelectorAll('[data-tab]')
    .forEach((button) => button.addEventListener('click', () => setProblem(button.dataset.tab)));
for (const id of [
    'latin-size',
    'latin-budget',
    'sudoku-algorithm',
    'sudoku-budget',
    'golf-g',
    'golf-p',
    'golf-w',
    'golf-budget',
])
    get(id).addEventListener('change', () => {
        if (id === 'golf-w' && numberValue('golf-w') >= 8) {
            if (numberValue('golf-budget') < 3000000000)
                get('golf-budget').value = '3000000000';
            if (numberValue('time-limit') < 600)
                get('time-limit').value = '600';
        }
        clearResult();
        get('latin-size-label').textContent =
            `${numberValue('latin-size')} × ${numberValue('latin-size')}`;
        modeNote();
        renderBoard();
        metrics();
    });
get('sudoku-level').addEventListener('change', () => {
    const selected = value('sudoku-level');
    if (selected !== 'custom' && examples[selected])
        clues = [...examples[selected]].map(Number);
    clearResult();
    renderBoard();
});
get('eight-preset').addEventListener('click', () => {
    if (activeJob)
        return;
    get('golf-g').value = '8';
    get('golf-p').value = '4';
    get('golf-w').value = '8';
    get('golf-budget').value = '3000000000';
    get('time-limit').value = '600';
    clearResult();
    modeNote();
    renderBoard();
});
get('time-limit').addEventListener('change', modeNote);
get('previous-step').addEventListener('click', () => {
    stopPlaying();
    visibleRows = Math.max(0, visibleRows - 1);
    renderGrid();
});
get('previous-week').addEventListener('click', () => {
    if (week > 0) {
        week--;
        renderGolf();
    }
});
get('next-week').addEventListener('click', () => {
    if (week < (result?.schedule?.length || 0) - 1) {
        week++;
        renderGolf();
    }
});
get('week-range').addEventListener('input', () => {
    week = numberValue('week-range') - 1;
    renderGolf();
});
get('play').addEventListener('click', () => {
    if (playing !== null) {
        stopPlaying();
        return;
    }
    if (!result?.grid)
        return;
    const n = result.grid.length;
    if (visibleRows >= n)
        visibleRows = 0;
    renderGrid();
    get('play').textContent = 'Pause';
    playing = window.setInterval(() => {
        visibleRows++;
        renderGrid();
        if (visibleRows >= n)
            stopPlaying();
    }, 600);
});
get('next-step').addEventListener('click', () => {
    stopPlaying();
    if (result?.grid) {
        if (visibleRows >= result.grid.length)
            visibleRows = 0;
        visibleRows++;
        renderGrid();
    }
});
get('show-all').addEventListener('click', () => {
    stopPlaying();
    visibleRows = result?.grid?.length || 0;
    renderGrid();
});
get('download').addEventListener('click', () => {
    if (!result)
        return;
    const url = URL.createObjectURL(new Blob([JSON.stringify({ request: lastRequest, result }, null, 2)], {
        type: 'application/json',
    }));
    const link = document.createElement('a');
    link.href = url;
    link.download = `search-lab-${problem}.json`;
    link.click();
    window.setTimeout(() => URL.revokeObjectURL(url), 1000);
});
async function updateLongRun() {
    try {
        longRun = await api('/api/long-run');
        const eight = longRun.completed.find((item) => item.weeks === 8 && item.verified);
        const nine = longRun.completed.find((item) => item.weeks === 9);
        const seconds = Number(longRun.progress.match(/seconds=([\d.]+)/)?.[1] || 0);
        const age = longRun.modified
            ? Math.floor((Date.now() - new Date(longRun.modified).getTime()) / 1000)
            : null;
        get('long-run-message').textContent = !longRun.available
            ? 'No record is available for the separate experiment.'
            : `${eight ? '8 weeks solved and verified in ' + eight.seconds.toFixed(1) + ' s. ' : ''}` +
                (nine
                    ? `9 weeks: ${nine.status === 'SOLVED' && nine.verified ? 'verified solution' : nine.status}.`
                    : `9 weeks: no result in the record yet (${Math.floor(seconds / 60)} min of recorded search).`) +
                (age !== null && age > 90
                    ? ` Last record: ${Math.floor(age / 60)} min old; the UI cannot confirm that the process is still running.`
                    : '');
        get('show-eight').hidden = !eight;
    }
    catch {
        get('long-run-message').textContent = 'Could not read the separate experiment record.';
    }
}
get('show-eight').addEventListener('click', () => {
    const eight = longRun?.completed.find((item) => item.weeks === 8 && item.verified);
    if (!eight || activeJob)
        return;
    clearResult();
    get('golf-g').value = '8';
    get('golf-p').value = '4';
    get('golf-w').value = '8';
    modeNote();
    lastRequest = { problem: 'golfers', g: 8, p: 4, w: 8, seconds: 30 };
    applyResult({
        status: 'SOLVED',
        grid: null,
        schedule: eight.schedule,
        verified: true,
        metrics: { 'Time (ms)': eight.seconds * 1000 },
        model: 'general',
        optimalWeeks: false,
        message: '',
        raw: JSON.stringify(eight, null, 2),
    });
    setBanner('Verified schedule loaded from the separate general DFS experiment. No new search was started.');
});
window.setInterval(() => {
    if (problem === 'golfers')
        void updateLongRun();
    revealPanel('.experiment');
}, 15000);
async function initialize() {
    try {
        const meta = await api('/api/meta');
        examples = meta.examples;
        if (examples.easy)
            clues = [...examples.easy].map(Number);
        get('connection').textContent = meta.ready ? 'Programs ready' : 'A C++ program is missing';
        get('connection-dot').classList.add(meta.ready ? 'green' : 'bad');
    }
    catch {
        get('connection').textContent = 'Server is not responding';
        get('connection-dot').classList.add('bad');
    }
    const requested = location.hash.slice(1);
    setProblem(['latin', 'sudoku', 'golfers'].includes(requested) ? requested : 'latin');
}
void initialize();
