"use strict";
const get = (id) => {
    const element = document.getElementById(id);
    if (!element)
        throw new Error(`Missing ${id}`);
    return element;
};
const numberValue = (id) => Number(get(id).value);
const value = (id) => get(id).value;
const format = (n) => new Intl.NumberFormat('sq-AL').format(n);
let problem = 'latin', examples = {}, clues = Array(81).fill(0);
let result = null, activeJob = null, visibleRows = 0, week = 0, playing = null;
let lastRequest = null, longRun = null;
const content = {
    latin: { title: 'Latin Square', eyebrow: '01 / GJENERIM TABELASH', description: 'Çdo numër, saktësisht një herë në çdo rresht dhe kolonë.', rules: ['Numrat nga 1 deri në n', 'Pa përsëritje në rreshta', 'Pa përsëritje në kolona'] },
    sudoku: { title: 'Sudoku', eyebrow: '02 / PLOTËSIM KUFIZIMESH', description: 'Plotëso tabelën duke ruajtur numrat fillestarë dhe të gjitha kufizimet.', rules: ['Numrat 1–9 në çdo rresht', 'Numrat 1–9 në çdo kolonë e bllok', 'Të dhënat fillestare nuk ndryshojnë'] },
    golfers: { title: 'Social Golfers', eyebrow: '03 / PLANIFIKIM GRUPESH', description: 'Të gjithë luajnë çdo javë. Asnjë çift nuk luan bashkë dy herë.', rules: ['Çdo lojtar një herë në javë', 'Madhësia e çdo grupi është fikse', 'Asnjë çift i përsëritur mes javëve'] }
};
async function api(url, options = {}) {
    const response = await fetch(url, { ...options, headers: { 'Content-Type': 'application/json', ...options.headers } });
    const data = await response.json();
    if (!response.ok)
        throw new Error(data.error || 'Lidhja me serverin dështoi.');
    return data;
}
function stopPlaying() { if (playing !== null)
    window.clearInterval(playing); playing = null; get('play').textContent = 'Luaj'; }
function setBanner(text, error = false) { const banner = get('run-banner'); banner.hidden = !text; banner.textContent = text; banner.classList.toggle('error', error); }
function setStatus(text, kind = '') { const badge = get('status-badge'); badge.textContent = text; badge.className = 'status-badge ' + kind; }
function busy(yes) {
    get('solve-button').disabled = yes;
    get('reset-button').disabled = yes;
    get('cancel-button').hidden = !yes;
    document.querySelectorAll('[data-tab]').forEach(button => button.disabled = yes);
    get('solve-form').querySelectorAll('input,select').forEach(input => input.disabled = yes);
}
function metrics() {
    const m = result?.metrics || {};
    const ms = m['Time (ms)'];
    get('metric-time').textContent = ms === undefined ? '—' : ms < 1000 ? `${ms.toLocaleString('sq-AL', { maximumFractionDigits: 3 })} ms` : `${(ms / 1000).toLocaleString('sq-AL', { maximumFractionDigits: 2 })} s`;
    get('metric-nodes').textContent = m.Visited === undefined ? '—' : format(m.Visited);
    let title = 'Kthime prapa', hint = 'Caktime të zhbëra', metric = m.Backtracks;
    if (problem === 'latin' && value('latin-algorithm') === 'iddfs') {
        title = 'Iteracione IDDFS';
        hint = 'Kufij thellësie të provuar';
        metric = m.Iterations;
    }
    if (problem === 'sudoku' && value('sudoku-algorithm') === 'bfs') {
        title = 'Radhë BFS · maksimumi';
        hint = 'Gjendje në pritje njëkohësisht';
        metric = m['Peak frontier (BFS)'];
    }
    get('metric-third-title').textContent = title;
    get('metric-third-hint').textContent = hint;
    get('metric-third').textContent = metric === undefined ? '—' : format(metric);
    const verified = result?.verified && result.status === 'SOLVED';
    get('metric-validation').textContent = verified ? '✓ E vlefshme' : result ? 'Pa zgjidhje të verifikuar' : 'Në pritje';
    get('metric-validation').parentElement.classList.toggle('valid', Boolean(verified));
    get('metric-validation-hint').textContent = verified ? 'Kontrolli C++ dhe ai i ndërfaqes kaluan' : 'Kontroll i pavarur i kufizimeve';
}
function clearResult() { stopPlaying(); result = null; lastRequest = null; visibleRows = 0; week = 0; setBanner(''); setStatus('Gati'); get('presentation').hidden = true; get('raw-panel').hidden = true; metrics(); }
function modeNote() {
    const g = numberValue('golf-g'), p = numberValue('golf-p'), w = numberValue('golf-w');
    get('golf-total').textContent = format(g * p || 0);
    const specialized = g === 8 && p === 4 && w >= 8 && w <= 10;
    get('golf-mode').textContent = specialized ?
        'Model i specializuar + DFS/backtracking. Programi gjeneron 10 javë dhe merr prefiksin e kërkuar. Ky nuk është solver-i i përgjithshëm i eksperimentit në sfond.' :
        'DFS/backtracking i përgjithshëm me kontrolle kufizimesh.';
}
function renderGrid() {
    const container = get('grid-board');
    container.replaceChildren();
    const n = problem === 'sudoku' ? 9 : Math.min(8, Math.max(1, numberValue('latin-size') || 1));
    container.style.setProperty('--n', String(n));
    container.style.setProperty('--board-width', `${Math.min(n * 56, 450)}px`);
    container.setAttribute('aria-label', `${content[problem].title}, ${n} rreshta dhe ${n} kolona`);
    for (let r = 0; r < n; r++)
        for (let c = 0; c < n; c++) {
            const initial = problem === 'sudoku' ? clues[r * 9 + c] : (r === 0 ? c + 1 : c === 0 ? r + 1 : 0);
            const solution = result?.status === 'SOLVED' ? result.grid?.[r]?.[c] : undefined;
            const shown = solution !== undefined && r < visibleRows ? solution : initial;
            const cell = document.createElement('div');
            cell.className = 'cell';
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
                input.setAttribute('aria-label', `Rreshti ${r + 1}, kolona ${c + 1}`);
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
    get('board-count').textContent = problem === 'sudoku' ? `${clues.filter(Boolean).length} numra fillestarë · 81 qeliza` : `${n * n} qeliza · ${(n - 1) ** 2} vendime`;
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
        const title = document.createElement('small');
        title.textContent = `GRUPI ${String(k + 1).padStart(2, '0')}`;
        const players = document.createElement('div');
        players.className = 'group-players';
        for (let i = 0; i < p; i++) {
            const player = document.createElement('span');
            player.className = 'player';
            player.textContent = groups ? String(groups[k][i]) : '·';
            players.append(player);
        }
        card.append(title, players);
        container.append(card);
    }
    const totalWeeks = result?.schedule?.length || 0;
    get('golf-week-controls').hidden = !totalWeeks;
    get('week-range').max = String(totalWeeks || 1);
    get('week-range').value = String(week + 1);
    get('week-label').textContent = `Java ${week + 1} / ${totalWeeks}`;
    get('previous-week').disabled = week === 0;
    get('next-week').disabled = week >= totalWeeks - 1;
    get('board-count').textContent = `${g * p} lojtarë · ${totalWeeks || numberValue('golf-w')} javë`;
}
function renderBoard() {
    get('grid-board').hidden = problem === 'golfers';
    get('golf-board').hidden = problem !== 'golfers';
    get('golf-week-controls').hidden = true;
    get('board-title').textContent = problem === 'golfers' ? 'Orari i lojtarëve' : 'Tabela e problemit';
    get('board-subtitle').textContent = problem === 'golfers' ? 'ID-të e lojtarëve fillojnë nga 0. Secili luan një herë në javë.' : problem === 'sudoku' ? 'Numrat fillestarë ruhen. Mund ta personalizosh tabelën para ekzekutimit.' : 'Rreshti dhe kolona e parë janë të fiksuara nga simetria.';
    get('board-legend').textContent = problem === 'golfers' ? 'Çdo rreth përfaqëson një lojtar' : '■  Qelizat e fiksuara dallohen me ngjyrë';
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
    const info = content[problem];
    get('problem-title').textContent = info.title;
    get('problem-description').textContent = info.description;
    get('problem-eyebrow').textContent = info.eyebrow;
    for (const name of ['latin', 'sudoku', 'golfers'])
        get(`${name}-controls`).hidden = problem !== name;
    document.querySelectorAll('[data-tab]').forEach(button => { const selected = button.dataset.tab === problem; button.classList.toggle('active', selected); button.setAttribute('aria-pressed', String(selected)); });
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
}
function applyResult(next) {
    result = next;
    visibleRows = next.grid?.length || 0;
    week = 0;
    setStatus(next.status === 'SOLVED' ? 'U zgjidh' : next.status === 'UNSAT' ? 'E pamundur' : next.status === 'UNKNOWN' ? 'Pa përfundim' : 'Gabim', next.status === 'SOLVED' ? 'solved' : next.status === 'UNSAT' || next.status === 'ERROR' ? 'error' : 'unknown');
    let message = next.message;
    if (next.status === 'UNSAT')
        message = 'Programi provoi se nuk ka zgjidhje për këto parametra.';
    if (next.status === 'SOLVED' && next.model === 'specialized')
        message = 'U përdor model i specializuar + DFS/backtracking, jo DFS-i i përgjithshëm. ' + (next.optimalWeeks ? 'Numri i javëve arrin kufirin matematikor.' : '');
    else if (next.optimalWeeks)
        message = 'Orari arrin kufirin matematikor: numri i javëve është optimal.';
    setBanner(message, next.status !== 'SOLVED');
    get('raw-output').textContent = next.raw;
    get('raw-panel').hidden = false;
    get('presentation').hidden = !(next.status === 'SOLVED' && next.grid);
    renderBoard();
    metrics();
}
function collect() {
    const seconds = numberValue('time-limit');
    if (problem === 'latin')
        return { problem, seconds, n: numberValue('latin-size'), algorithm: value('latin-algorithm') };
    if (problem === 'sudoku')
        return { problem, seconds, puzzle: clues.join(''), algorithm: value('sudoku-algorithm'), budget: numberValue('sudoku-budget') };
    return { problem, seconds, g: numberValue('golf-g'), p: numberValue('golf-p'), w: numberValue('golf-w'), budget: numberValue('golf-budget') };
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
        setStatus('Në kërkim', 'running');
        setBanner('Programi C++ po kryen kërkimin…');
        const firstJob = await api('/api/solve', { method: 'POST', body: JSON.stringify(request) });
        activeJob = firstJob.id;
        renderBoard();
        let job = firstJob;
        while (job.state === 'running') {
            await new Promise(resolve => window.setTimeout(resolve, 250));
            job = await api(`/api/jobs/${firstJob.id}`);
            if (job.state === 'running')
                setBanner(`Programi C++ po kërkon… ${Math.floor((Date.now() - job.started) / 1000)} s. Kufiri i këtij ekzekutimi: ${request.seconds} s.`);
        }
        activeJob = null;
        busy(false);
        if (job.result)
            applyResult(job.result);
    }
    catch (error) {
        activeJob = null;
        busy(false);
        setStatus('Gabim', 'error');
        setBanner('');
        const box = get('form-error');
        box.textContent = error instanceof Error ? error.message : 'Gabim gjatë ekzekutimit.';
        box.hidden = false;
        renderBoard();
    }
});
get('cancel-button').addEventListener('click', () => { if (activeJob)
    void api(`/api/jobs/${activeJob}/cancel`, { method: 'POST' }).catch(() => { }); });
get('reset-button').addEventListener('click', () => { if (!activeJob) {
    clearResult();
    get('form-error').hidden = true;
    renderBoard();
} });
document.querySelector('.brand')?.addEventListener('click', event => { event.preventDefault(); setProblem('latin'); });
document.querySelectorAll('[data-tab]').forEach(button => button.addEventListener('click', () => setProblem(button.dataset.tab)));
for (const id of ['latin-size', 'latin-algorithm', 'sudoku-algorithm', 'sudoku-budget', 'golf-g', 'golf-p', 'golf-w', 'golf-budget'])
    get(id).addEventListener('change', () => {
        clearResult();
        get('latin-size-label').textContent = `${numberValue('latin-size')} × ${numberValue('latin-size')}`;
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
get('previous-week').addEventListener('click', () => { if (week > 0) {
    week--;
    renderGolf();
} });
get('next-week').addEventListener('click', () => { if (week < (result?.schedule?.length || 0) - 1) {
    week++;
    renderGolf();
} });
get('week-range').addEventListener('input', () => { week = numberValue('week-range') - 1; renderGolf(); });
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
    get('play').textContent = 'Pauzo';
    playing = window.setInterval(() => { visibleRows++; renderGrid(); if (visibleRows >= n)
        stopPlaying(); }, 600);
});
get('next-step').addEventListener('click', () => { stopPlaying(); if (result?.grid) {
    if (visibleRows >= result.grid.length)
        visibleRows = 0;
    visibleRows++;
    renderGrid();
} });
get('show-all').addEventListener('click', () => { stopPlaying(); visibleRows = result?.grid?.length || 0; renderGrid(); });
get('download').addEventListener('click', () => {
    if (!result)
        return;
    const url = URL.createObjectURL(new Blob([JSON.stringify({ request: lastRequest, result }, null, 2)], { type: 'application/json' }));
    const link = document.createElement('a');
    link.href = url;
    link.download = `search-lab-${problem}.json`;
    link.click();
    window.setTimeout(() => URL.revokeObjectURL(url), 1000);
});
async function updateLongRun() {
    try {
        longRun = await api('/api/long-run');
        const eight = longRun.completed.find(item => item.weeks === 8 && item.verified);
        const nine = longRun.completed.find(item => item.weeks === 9);
        const seconds = Number(longRun.progress.match(/seconds=([\d.]+)/)?.[1] || 0);
        const age = longRun.modified ? Math.floor((Date.now() - new Date(longRun.modified).getTime()) / 1000) : null;
        get('long-run-message').textContent = !longRun.available ? 'Nuk ka regjistër të disponueshëm për eksperimentin e veçantë.' :
            `${eight ? '8 javë u zgjidhën dhe u verifikuan në ' + eight.seconds.toFixed(1) + ' s. ' : ''}` +
                (nine ? `9 javë: ${nine.status === 'SOLVED' && nine.verified ? 'zgjidhje e verifikuar' : nine.status}.` :
                    `9 javë: ende pa rezultat në regjistër (${Math.floor(seconds / 60)} min kërkim).`) +
                (age !== null && age > 90 ? ` Regjistrimi i fundit është ${Math.floor(age / 60)} min i vjetër; gjallëria e procesit nuk konfirmohet nga UI.` : '');
        get('show-eight').hidden = !eight;
    }
    catch {
        get('long-run-message').textContent = 'Statusi i eksperimentit në sfond nuk mund të lexohet.';
    }
}
get('show-eight').addEventListener('click', () => {
    const eight = longRun?.completed.find(item => item.weeks === 8 && item.verified);
    if (!eight || activeJob)
        return;
    clearResult();
    get('golf-g').value = '8';
    get('golf-p').value = '4';
    get('golf-w').value = '8';
    modeNote();
    lastRequest = { problem: 'golfers', g: 8, p: 4, w: 8, seconds: 30 };
    applyResult({ status: 'SOLVED', grid: null, schedule: eight.schedule, verified: true, metrics: { 'Time (ms)': eight.seconds * 1000 }, model: 'general', optimalWeeks: false, message: '', raw: JSON.stringify(eight, null, 2) });
    setBanner('Orar nga eksperimenti i veçantë me DFS të përgjithshëm. Nuk u nis një ekzekutim i ri.');
});
window.setInterval(() => { if (problem === 'golfers')
    void updateLongRun(); }, 15000);
async function initialize() {
    try {
        const meta = await api('/api/meta');
        examples = meta.examples;
        if (examples.easy)
            clues = [...examples.easy].map(Number);
        get('connection').textContent = meta.ready ? 'Programet janë gati' : 'Mungon ndonjë program C++';
        get('connection-dot').classList.add(meta.ready ? 'green' : 'bad');
    }
    catch {
        get('connection').textContent = 'Serveri nuk përgjigjet';
        get('connection-dot').classList.add('bad');
    }
    const requested = location.hash.slice(1);
    setProblem(['latin', 'sudoku', 'golfers'].includes(requested) ? requested : 'latin');
}
void initialize();
