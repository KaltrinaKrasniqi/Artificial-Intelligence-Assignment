import test from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  requestArgs,
  checkLatin,
  checkSudoku,
  checkGolfers,
  parseOutput,
} from '../solver-adapter.mjs';

test('Only allowed executables and numeric arguments are accepted', () => {
  assert.deepEqual(requestArgs({ problem: 'latin', n: 5 }).args, ['5', '0']);
  assert.deepEqual(
    requestArgs({ problem: 'golfers', g: 8, p: 4, w: 8, budget: 3000000000, seconds: 600 }).args,
    ['8', '4', '8', '3000000000'],
  );
  assert.deepEqual(requestArgs({ problem: 'latin', n: 5, budget: 2000000 }).args, ['5', '2000000']);
  for (const bad of [
    { problem: 'latin', n: 5, budget: -1 },
    { problem: 'latin', n: 5, budget: 10000000001 },
    { problem: 'golfers', g: 8, p: 4, w: 8, budget: 10000000001 },
    { problem: 'latin', n: 9, algorithm: 'iddfs' },
    { problem: 'latin', n: 5, algorithm: 'iddfs; calc' },
    { problem: 'latin', n: 5, algorithm: 'backtracking' },
    { problem: 'golfers', g: 2147483647, p: 2, w: 3 },
    { problem: 'sudoku', algorithm: 'bfs', puzzle: '123' },
    { problem: '../anything' },
    { problem: 'latin', n: 5, algorithm: 'iddfs', seconds: 3601 },
  ])
    assert.throws(() => requestArgs(bad));
});
test('Independent validators reject duplicates and incomplete answers', () => {
  assert.equal(
    checkLatin([
      [1, 2],
      [2, 1],
    ]),
    true,
  );
  assert.equal(
    checkLatin([
      [1, 2],
      [1, 2],
    ]),
    false,
  );
  assert.equal(checkLatin([[1, 2], [2]]), false);
  assert.equal(checkSudoku(Array(81).fill(0)), false);
  assert.equal(checkSudoku(Array(81).fill(0), false), true);
  assert.equal(
    checkGolfers(
      [
        [
          [0, 1],
          [2, 3],
        ],
        [
          [0, 2],
          [1, 3],
        ],
      ],
      2,
      2,
      2,
    ),
    true,
  );
  assert.equal(
    checkGolfers(
      [
        [
          [0, 1],
          [2, 3],
        ],
        [
          [0, 1],
          [2, 3],
        ],
      ],
      2,
      2,
      2,
    ),
    false,
  );
  assert.equal(
    checkGolfers(
      [
        [
          [0, 1],
          [2, 2],
        ],
      ],
      2,
      2,
      1,
    ),
    false,
  );
});
test('A printed success cannot bypass the independent validator', () => {
  const r = parseOutput(
    '1 1\n2 2\nValidation: PASS\n',
    '',
    0,
    { problem: 'latin', n: 2 },
    'general',
  );
  assert.equal(r.status, 'ERROR');
  assert.equal(r.verified, false);
  assert.equal(
    parseOutput('', 'Node budget reached', 3, { problem: 'latin', n: 7 }, 'general').status,
    'UNKNOWN',
  );
});
test('HTTP integration with the actual C++ programs', async (t) => {
  const directory = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
  const root = process.env.AI_PROJECT_ROOT || path.resolve(directory, '..');
  const server = spawn(process.execPath, ['server.mjs'], {
    cwd: directory,
    env: { ...process.env, AI_PROJECT_ROOT: root, PORT: '8791' },
    windowsHide: true,
  });
  t.after(() => server.kill());
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('Test server did not start')), 10000);
    server.once('error', reject);
    server.stderr.on('data', (data) => {
      clearTimeout(timer);
      reject(new Error(String(data)));
    });
    server.stdout.once('data', () => {
      clearTimeout(timer);
      resolve();
    });
  });
  const base = 'http://127.0.0.1:8791';
  const send = async (url, body) =>
    fetch(base + url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
  const meta = await (await fetch(base + '/api/meta')).json();
  assert.equal(meta.ready, true);
  assert.equal(Object.keys(meta.examples).length, 4);
  const solve = async (body) => {
    const response = await send('/api/solve', body);
    assert.equal(response.status, 202);
    let job = await response.json();
    for (let i = 0; i < 200 && job.state === 'running'; i++) {
      await new Promise((resolve) => setTimeout(resolve, 50));
      job = await (await fetch(base + '/api/jobs/' + job.id)).json();
    }
    assert.equal(job.state, 'complete');
    return job.result;
  };
  assert.equal(
    (await send('/api/solve', { problem: 'latin', n: 0, algorithm: 'iddfs' })).status,
    400,
  );
  assert.equal(
    (
      await fetch(base + '/api/solve', {
        method: 'POST',
        headers: { Origin: 'https://example.com', 'Content-Type': 'application/json' },
        body: '{}',
      })
    ).status,
    403,
  );
  const latin = await solve({ problem: 'latin', n: 5 });
  assert.equal(latin.status, 'SOLVED');
  assert.equal(latin.verified, true);
  assert.equal(latin.metrics.Iterations, 17);
  const sudoku = await solve({ problem: 'sudoku', puzzle: meta.examples.hard, algorithm: 'bfs' });
  assert.equal(sudoku.status, 'SOLVED');
  assert.equal(sudoku.verified, true);
  assert.equal(sudoku.metrics.Visited, 18885);
  const bonus = await solve({
    problem: 'sudoku',
    puzzle: meta.examples.hard_plus,
    algorithm: 'bfs',
  });
  assert.equal(bonus.status, 'SOLVED');
  assert.equal(bonus.verified, true);
  assert.equal(bonus.metrics.Visited, 117167);
  const golf = await solve({ problem: 'golfers', g: 8, p: 4, w: 3 });
  assert.equal(golf.status, 'SOLVED');
  assert.equal(golf.model, 'general');
  assert.equal(golf.verified, true);
  const dynamic = await solve({ problem: 'golfers', g: 33, p: 2, w: 2 });
  assert.equal(dynamic.status, 'SOLVED');
  assert.equal(dynamic.verified, true);
  const ten = await solve({ problem: 'golfers', g: 8, p: 4, w: 10, budget: 1000 });
  assert.equal(ten.status, 'UNKNOWN');
  assert.equal(ten.model, 'general');
  assert.equal(ten.optimalWeeks, false);
  assert.equal((await solve({ problem: 'latin', n: 7, budget: 2000000 })).status, 'UNKNOWN');
  assert.equal((await solve({ problem: 'golfers', g: 8, p: 4, w: 11 })).status, 'UNSAT');
  const slowResponse = await send('/api/solve', {
    problem: 'sudoku',
    puzzle: '0'.repeat(81),
    algorithm: 'bfs',
    budget: 5000000,
  });
  const slow = await slowResponse.json();
  assert.equal(slowResponse.status, 202);
  assert.equal(
    (await send('/api/solve', { problem: 'latin', n: 3, algorithm: 'iddfs' })).status,
    409,
  );
  assert.equal((await send('/api/jobs/' + slow.id + '/cancel', {})).status, 200);
  let stopped;
  for (let i = 0; i < 100; i++) {
    stopped = await (await fetch(base + '/api/jobs/' + slow.id)).json();
    if (stopped.state === 'complete') break;
    await new Promise((resolve) => setTimeout(resolve, 30));
  }
  assert.equal(stopped.result.status, 'UNKNOWN');
  assert.match(stopped.result.message, /user/);
  assert.equal((await fetch(base + '/api/long-run')).status, 200);
});
