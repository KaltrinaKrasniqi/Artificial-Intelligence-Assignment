import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { readFile, access } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { randomUUID } from 'node:crypto';
import { requestArgs, parseOutput, examplesFromCpp, longRunStatus } from './solver-adapter.mjs';

const directory = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.resolve(process.env.AI_PROJECT_ROOT || path.join(directory, '..'));
const longRunFolder =
  process.env.AI_LONG_RUN || path.resolve(projectRoot, '..', 'Ushtrime_IA', 'golf_dfs_unlimited');
const port = Number(process.env.PORT || 8787);
const jobs = new Map();
let active = null;
const staticFiles = new Map([
  ['/', 'index.html'],
  ['/search-trees', 'search-trees.html'],
  ['/search-trees.js', 'search-trees.js'],
  ['/search-trees.css', 'search-trees.css'],
  ['/styles.css', 'styles.css'],
  ['/assets/app.js', 'assets/app.js'],
]);
const types = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
};
function json(res, status, body) {
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
  });
  res.end(JSON.stringify(body));
}
async function body(req) {
  if (!String(req.headers['content-type']).startsWith('application/json'))
    throw new Error('Use JSON.');
  let text = '';
  for await (const chunk of req) {
    text += chunk;
    if (text.length > 8192) throw new Error('Request is too large.');
  }
  return JSON.parse(text);
}
function publicJob(job) {
  const { child, ...data } = job;
  return data;
}
const server = http.createServer(async (req, res) => {
  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader(
    'Content-Security-Policy',
    "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; object-src 'none'; frame-ancestors 'none'; base-uri 'none'",
  );
  if (![`127.0.0.1:${port}`, `localhost:${port}`].includes(req.headers.host))
    return json(res, 403, { error: 'Host not allowed.' });
  if (
    req.headers.origin &&
    ![`http://localhost:${port}`, `http://127.0.0.1:${port}`].includes(req.headers.origin)
  )
    return json(res, 403, { error: 'Origin not allowed.' });
  const url = new URL(req.url, 'http://localhost');
  try {
    if (req.method === 'GET' && url.pathname === '/api/meta') {
      const programs = await Promise.all(
        ['latin_square', 'sudoku', 'social_golfers'].map(async (name) => {
          try {
            await access(
              path.join(projectRoot, 'bin', name + (process.platform === 'win32' ? '.exe' : '')),
            );
            return true;
          } catch {
            return false;
          }
        }),
      );
      return json(res, 200, {
        ready: programs.every(Boolean),
        examples: await examplesFromCpp(projectRoot),
      });
    }
    if (req.method === 'GET' && url.pathname === '/api/long-run')
      return json(res, 200, await longRunStatus(longRunFolder));
    if (req.method === 'POST' && url.pathname === '/api/solve') {
      if (active) return json(res, 409, { error: 'A UI experiment is already running.' });
      const request = await body(req),
        spec = requestArgs(request);
      const executable = path.join(
        projectRoot,
        'bin',
        spec.program + (process.platform === 'win32' ? '.exe' : ''),
      );
      await access(executable);
      const job = {
        id: randomUUID(),
        state: 'running',
        request,
        model: spec.model,
        started: Date.now(),
        result: null,
      };
      jobs.set(job.id, job);
      active = job.id;
      while (jobs.size > 20) {
        const oldest = jobs.keys().next().value;
        if (oldest === active) break;
        jobs.delete(oldest);
      }
      let stdout = '',
        stderr = '',
        reason = null;
      const child = spawn(executable, spec.args, {
        shell: false,
        windowsHide: true,
        cwd: projectRoot,
        stdio: ['ignore', 'pipe', 'pipe'],
      });
      job.child = child;
      const timer = setTimeout(() => {
        reason = 'The UI time limit was reached. Search stopped without a conclusion.';
        child.kill();
      }, spec.seconds * 1000);
      const collect = (which, chunk) => {
        if (which === 'out') stdout += chunk.toString();
        else stderr += chunk.toString();
        if (stdout.length + stderr.length > 512000) {
          reason = 'Program output exceeded the UI limit.';
          child.kill();
        }
      };
      child.stdout.on('data', (chunk) => collect('out', chunk));
      child.stderr.on('data', (chunk) => collect('err', chunk));
      child.on('error', (error) => {
        reason = `Could not start the program: ${error.message}`;
      });
      child.on('close', (code) => {
        clearTimeout(timer);
        job.result = parseOutput(
          stdout,
          stderr,
          code,
          request,
          spec.model,
          job.cancelled ? 'The experiment was stopped by the user.' : reason,
        );
        job.state = 'complete';
        job.finished = Date.now();
        delete job.child;
        if (active === job.id) active = null;
      });
      return json(res, 202, publicJob(job));
    }
    const match = url.pathname.match(/^\/api\/jobs\/([a-f0-9-]+)(\/cancel)?$/);
    if (match) {
      const job = jobs.get(match[1]);
      if (!job) return json(res, 404, { error: 'Experiment not found.' });
      if (req.method === 'POST' && match[2]) {
        job.cancelled = true;
        job.child?.kill();
        return json(res, 200, { ok: true });
      }
      if (req.method === 'GET' && !match[2]) return json(res, 200, publicJob(job));
    }

    // Explicit files only: recorded tree data, with no search execution.
    const treeDownloads = new Map([
      ['/search-tree-data/puzzle.json', ['puzzle-report-trace.json', 'application/json']],
      ['/search-tree-data/river.json', ['river-report-trace.json', 'application/json']],
      ['/search-tree-data/workforce.json', ['workforce-report-trace.json', 'application/json']],
    ]);
    if (req.method === 'GET' && treeDownloads.has(url.pathname)) {
      const [file, type] = treeDownloads.get(url.pathname);
      const data = await readFile(path.join(projectRoot, 'search_trees', 'generated', file));
      res.writeHead(200, { 'Content-Type': type, 'Cache-Control': 'no-store', });
      return res.end(data);
    }
    if (req.method === 'GET' && staticFiles.has(url.pathname)) {
      const file = staticFiles.get(url.pathname),
        data = await readFile(path.join(directory, 'public', file));
      res.writeHead(200, {
        'Content-Type': types[path.extname(file)],
        'Cache-Control': 'no-store',
      });
      return res.end(data);
    }
    json(res, 404, { error: 'Not found.' });
  } catch (error) {
    json(res, 400, { error: error.message || 'Invalid request.' });
  }
});
server.listen(port, '127.0.0.1', () => console.log(`AI Lab: http://127.0.0.1:${port}`));
for (const signal of ['SIGINT', 'SIGTERM'])
  process.on(signal, () => {
    for (const job of jobs.values()) job.child?.kill();
    server.close(() => process.exit(0));
  });
