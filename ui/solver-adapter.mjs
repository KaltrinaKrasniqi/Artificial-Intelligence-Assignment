import { readFile, stat } from 'node:fs/promises';
import path from 'node:path';

export function requestArgs(body) {
  const int = (value, min, max, name) => {
    if (!Number.isInteger(value) || value < min || value > max)
      throw new Error(`${name}: value not allowed.`);
    return value;
  };
  if (!body || typeof body !== 'object') throw new Error('Invalid request.');
  const seconds = int(body.seconds ?? 30, 5, 3600, 'Time');
  if (body.problem === 'latin') {
    if (body.algorithm !== undefined && body.algorithm !== 'iddfs')
      throw new Error('Algorithm not allowed.');
    return {
      program: 'latin_square',
      args: [
        String(int(body.n, 1, 8, 'n')),
        String(int(body.budget ?? 0, 0, 10000000000, 'Visits')),
      ],
      seconds,
      model: 'general',
    };
  }
  if (body.problem === 'sudoku') {
    if (!['bfs', 'backtracking'].includes(body.algorithm))
      throw new Error('Algorithm not allowed.');
    if (typeof body.puzzle !== 'string' || !/^[0-9.]{81}$/.test(body.puzzle))
      throw new Error('Sudoku must contain 81 cells.');
    const board = [...body.puzzle].map((c) => (c === '.' ? 0 : Number(c)));
    if (!checkSudoku(board, false)) throw new Error('The clues conflict in a row, column or box.');
    return {
      program: 'sudoku',
      args: [body.algorithm, body.puzzle, String(int(body.budget ?? 200000, 1, 5000000, 'Nodes'))],
      seconds,
      model: 'general',
    };
  }
  if (body.problem === 'golfers') {
    const g = int(body.g, 1, 2147483647, 'Groups'),
      p = int(body.p, 1, 2147483647, 'Players/group'),
      w = int(body.w, 1, 2147483647, 'Weeks');
    if (g * p > 2147483647) throw new Error('Player count exceeds the program indexing range.');
    return {
      program: 'social_golfers',
      args: [g, p, w, int(body.budget ?? 2000000, 1, 10000000000, 'Nodes')].map(String),
      seconds,
      model: 'general',
    };
  }
  throw new Error('Unknown problem.');
}

export function checkLatin(grid) {
  const n = grid.length;
  if (!n || grid.some((row) => !Array.isArray(row) || row.length !== n)) return false;
  const valid = (values) =>
    values.every((x) => Number.isInteger(x) && x >= 1 && x <= n) && new Set(values).size === n;
  return grid.every(valid) && grid.every((_, c) => valid(grid.map((row) => row[c])));
}
export function checkSudoku(cells, complete = true, clues) {
  if (
    cells.length !== 81 ||
    cells.some((x) => !Number.isInteger(x) || x < (complete ? 1 : 0) || x > 9)
  )
    return false;
  const good = (unit) => {
    const filled = unit.filter(Boolean);
    return new Set(filled).size === filled.length;
  };
  for (let i = 0; i < 9; i++) {
    if (!good(cells.slice(i * 9, i * 9 + 9))) return false;
    if (!good(Array.from({ length: 9 }, (_, r) => cells[r * 9 + i]))) return false;
    if (
      !good(
        Array.from(
          { length: 9 },
          (_, k) => cells[(Math.floor(i / 3) * 3 + Math.floor(k / 3)) * 9 + (i % 3) * 3 + (k % 3)],
        ),
      )
    )
      return false;
  }
  return !clues || clues.every((x, i) => !x || x === cells[i]);
}
export function checkGolfers(schedule, g, p, w) {
  if (schedule.length !== w) return false;
  const pairs = new Set();
  for (const week of schedule) {
    if (week.length !== g || week.some((group) => group.length !== p)) return false;
    const players = week.flat();
    if (
      players.some((x) => !Number.isInteger(x) || x < 0 || x >= g * p) ||
      new Set(players).size !== g * p
    )
      return false;
    for (const group of week)
      for (let i = 0; i < p; i++)
        for (let j = i + 1; j < p; j++) {
          const key = [group[i], group[j]].sort((a, b) => a - b).join(':');
          if (pairs.has(key)) return false;
          pairs.add(key);
        }
  }
  return true;
}
export function parseOutput(stdout, stderr, code, request, model, reason = null) {
  const lines = stdout.split(/\r?\n/),
    metrics = {};
  for (const line of lines) {
    const match = line.match(
      /^(Visited|Generated|Backtracks|Iterations|Maximum depth(?: \(unfixed cells\))?|Peak frontier \(BFS\)|Time \(ms\)): ([\d.eE+-]+)$/,
    );
    if (match) metrics[match[1]] = Number(match[2]);
  }
  let grid = null,
    schedule = null,
    verified = false;
  let status = reason
    ? 'UNKNOWN'
    : code === 0
      ? 'SOLVED'
      : code === 1
        ? 'UNSAT'
        : code === 3
          ? 'UNKNOWN'
          : 'ERROR';
  if (status === 'SOLVED' && request.problem === 'golfers') {
    schedule = lines
      .filter((line) => /^Week \d+:/.test(line))
      .map((line) =>
        [...line.matchAll(/\[\s*([\d\s]+)\]/g)].map((m) => m[1].trim().split(/\s+/).map(Number)),
      );
    verified = checkGolfers(schedule, request.g, request.p, request.w);
  } else if (status === 'SOLVED') {
    grid = lines
      .filter((line) => /^\s*\d+(?:\s+\d+)*\s*$/.test(line))
      .map((line) => line.trim().split(/\s+/).map(Number));
    verified =
      request.problem === 'latin'
        ? grid.length === request.n && checkLatin(grid)
        : checkSudoku(
            grid.flat(),
            true,
            [...request.puzzle].map((x) => (x === '.' ? 0 : Number(x))),
          );
  }
  if (status === 'SOLVED' && (!verified || !stdout.includes('Validation: PASS'))) status = 'ERROR';
  const bound =
    request.problem === 'golfers' && request.p > 1
      ? Math.floor((request.g * request.p - 1) / (request.p - 1))
      : null;
  return {
    status,
    grid,
    schedule,
    verified,
    metrics,
    model,
    optimalWeeks: verified && bound !== null && request.w === bound,
    message:
      reason ||
      (status === 'ERROR'
        ? stderr.trim() || 'The result failed independent UI validation.'
        : status === 'UNKNOWN'
          ? 'The search visit limit was reached. This does not prove that a solution is impossible.'
          : ''),
    raw: stdout + (stderr ? '\n' + stderr : ''),
  };
}
export async function examplesFromCpp(projectRoot) {
  const source = await readFile(path.join(projectRoot, 'problems', 'sudoku', 'solver.cpp'), 'utf8');
  return Object.fromEntries(
    [...source.matchAll(/\{\s*"(easy|medium|hard|hard_plus)"\s*,\s*"([0-9]{81})"\s*\}/g)].map(
      (m) => [m[1], m[2]],
    ),
  );
}
export async function longRunStatus(folder) {
  try {
    const [results, progress] = await Promise.all([
      readFile(path.join(folder, 'results.txt'), 'utf8'),
      readFile(path.join(folder, 'progress.log'), 'utf8'),
    ]);
    const matches = [
      ...results.matchAll(/RESULT weeks=(\d+) status=(\w+) seconds=([\d.]+)[^\n]*\n/g),
    ];
    const completed = matches.map((m) => {
      const text = results.slice(m.index + m[0].length).split(/START weeks=|RESULT weeks=/)[0];
      const schedule = text
        .split(/\r?\n/)
        .filter((line) => /^\[/.test(line))
        .map((line) =>
          [...line.matchAll(/\[\s*([\d\s]+)\]/g)].map((g) => g[1].trim().split(/\s+/).map(Number)),
        );
      const weeks = Number(m[1]);
      return {
        weeks,
        status: m[2],
        seconds: Number(m[3]),
        schedule,
        verified: m[2] === 'SOLVED' && checkGolfers(schedule, 8, 4, weeks),
      };
    });
    return {
      available: true,
      completed,
      progress: progress.trim().split(/\r?\n/).at(-1) || '',
      modified: (await stat(path.join(folder, 'progress.log'))).mtime.toISOString(),
    };
  } catch {
    return { available: false, completed: [], progress: '' };
  }
}
