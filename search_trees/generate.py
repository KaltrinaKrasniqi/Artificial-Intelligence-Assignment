"""Generate checked search-tree diagrams. No search heuristics are used."""
from __future__ import annotations

import argparse
from collections import Counter, deque
from dataclasses import dataclass, field
from html import escape
import itertools
import json
from pathlib import Path


@dataclass
class Node:
    id: int
    state: tuple | str
    parent: int | None
    action: str
    depth: int
    kind: str = 'open'
    reason: str = ''
    reference: int | None = None
    children: list[int] = field(default_factory=list)


def breadth_first(start, successors, invalid, goal, max_depth):
    nodes = [Node(0, start, None, 'Start', 0)]
    seen = {start: 0}
    queue = deque([0])
    goal_id = None
    while queue:
        node = nodes[queue.popleft()]
        if goal(node.state):
            node.kind = 'goal'
            if goal_id is None:
                goal_id = node.id
            continue
        if node.depth == max_depth:
            node.kind = 'cutoff'
            node.reason = 'Display depth reached; not expanded'
            continue
        for action, state in successors(node.state):
            child = Node(len(nodes), state, node.id, action, node.depth + 1)
            child.reason = invalid(state)
            if child.reason:
                child.kind = 'invalid'
            elif state in seen:
                child.kind = 'repeat'
                child.reference = seen[state]
                child.reason = f'Already discovered at node #{child.reference}'
            else:
                seen[state] = child.id
                queue.append(child.id)
            node.children.append(child.id)
            nodes.append(child)
    return nodes, goal_id


def path_to(nodes, goal_id):
    path = []
    while goal_id is not None:
        path.append(goal_id)
        goal_id = nodes[goal_id].parent
    return path[::-1]


PUZZLE_START = (1, 2, 3, 0, 4, 6, 7, 5, 8)
PUZZLE_GOAL = (1, 2, 3, 4, 5, 6, 7, 8, 0)


def puzzle_moves(state):
    blank = state.index(0)
    row, col = divmod(blank, 3)
    # Action names refer to movement of the blank, not the numbered tile.
    for name, dr, dc in [('Up', -1, 0), ('Down', 1, 0), ('Left', 0, -1), ('Right', 0, 1)]:
        r, c = row + dr, col + dc
        if 0 <= r < 3 and 0 <= c < 3:
            child = list(state)
            other = 3 * r + c
            child[blank], child[other] = child[other], child[blank]
            yield name, tuple(child)


LOADS = [(1, 0), (2, 0), (0, 1), (0, 2), (1, 1)]


def river_moves(state):
    m, c, boat = state
    direction = -1 if boat == 'L' else 1
    for dm, dc in LOADS:
        load = (f'{dm}M' if dm else '') + (f'{dc}C' if dc else '')
        arrow = 'L to R' if boat == 'L' else 'R to L'
        yield f'{load}: {arrow}', (m + direction * dm, c + direction * dc, 'R' if boat == 'L' else 'L')


def river_invalid(state):
    m, c, _ = state
    if not (0 <= m <= 3 and 0 <= c <= 3):
        return 'Not enough passengers on the departure bank'
    if m and c > m:
        return 'Cannibals outnumber missionaries on left bank'
    if 3 - m and 3 - c > 3 - m:
        return 'Cannibals outnumber missionaries on right bank'
    return ''


SHIFTS = 'DANO'
COVERAGE = [(1, 1, 1, 1)] * 6 + [(1, 0, 1, 2)]


def workforce_invalid(prefix):
    state = prefix + '?' * (28 - len(prefix))
    for day, required in enumerate(COVERAGE):
        column = state[day::7]
        unknown = column.count('?')
        for shift, target in zip(SHIFTS, required):
            count = column.count(shift)
            if count > target or count + unknown < target:
                return f'Day {day + 1}: required {target} {shift} shifts'
    for work, minimum, maximum in [(True, 2, 6), (False, 1, 3)]:
        matches = lambda c: c in 'DAN' if work else c == 'O'
        for start in range(28):
            if not matches(state[start]):
                continue
            length = 0
            while length < 28 and matches(state[(start + length) % 28]):
                length += 1
            name = 'Work' if work else 'Rest'
            if length > maximum:
                return f'{name} block exceeds {maximum} days'
            before, after = state[(start - 1) % 28], state[(start + length) % 28]
            if length < minimum and before != '?' and after != '?' and not matches(before):
                return f'Closed {name.lower()} block is shorter than {minimum} days'
    if any(state[i] == 'N' and state[(i + 1) % 28] == 'D' for i in range(28)):
        return 'Night immediately followed by day (N to D)'
    return ''


def workforce_moves(prefix):
    if len(prefix) < 28:
        for value in SHIFTS:
            yield f'X[{len(prefix)}] = {value}', prefix + value


def solve_workforce():
    # Fixed chronological variables and D,A,N,O values: ordinary DFS/backtracking.
    trace = [Node(0, '', None, 'Start', 0)]

    def search(index):
        current = trace[index]
        if len(current.state) == 28:
            current.kind = 'goal'
            return index
        for action, state in workforce_moves(current.state):
            child = Node(len(trace), state, index, action, current.depth + 1)
            trace.append(child)
            current.children.append(child.id)
            child.reason = workforce_invalid(state)
            if child.reason:
                child.kind = 'invalid'
                continue
            found = search(child.id)
            if found is not None:
                return found
            child.kind = 'exhausted'
        return None

    return trace, search(0)


def validate_workforce_complete(sequence):
    """Check the complete schedule independently of partial-state pruning."""
    assert len(sequence) == 28 and set(sequence) <= set(SHIFTS)
    for week in range(4):
        for day, target in enumerate(COVERAGE):
            coverage = Counter(sequence[((employee + week) % 4) * 7 + day] for employee in range(4))
            assert tuple(coverage[shift] for shift in SHIFTS) == target
    assert all(sequence[i] != 'N' or sequence[(i + 1) % 28] != 'D' for i in range(28))
    # Start each run only where the preceding day has the opposite work/rest type.
    kinds = [value != 'O' for value in sequence]
    for i, work in enumerate(kinds):
        if kinds[i - 1] == work:
            continue
        length = 1
        while length < 28 and kinds[(i + length) % 28] == work:
            length += 1
        low, high = (2, 6) if work else (1, 3)
        assert low <= length <= high
    assert len(set(kinds)) == 2  # Required coverage guarantees work and rest.


COLORS = {
    'open': ('#ffffff', '#738591'), 'cutoff': ('#f6f8fa', '#9aa6ad'),
    'repeat': ('#fff4d4', '#c88e12'), 'invalid': ('#ffeded', '#cb4949'),
    'goal': ('#e6f5e9', '#2e7e40'), 'path': ('#f0f9ed', '#358347'),
    'exhausted': ('#f5f5f5', '#909090'),
}


def node_lines(problem, state):
    if problem == 'puzzle':
        return []
    if problem == 'river':
        m, c, boat = state
        return [f'Left: {m}M, {c}C', f'Right: {3-m}M, {3-c}C', f'Boat: {boat}']
    known = len(state)
    return [' '.join(state[i:i + 7]) for i in range(0, known, 7)] + [f'{known} / 28 assigned']


def geometry(nodes, width):
    positions = {}
    cursor = 0
    def place(index):
        nonlocal cursor
        children = nodes[index].children
        if children:
            child_x = [place(child) for child in children]
            x = (child_x[0] + child_x[-1]) / 2
        else:
            x = cursor
            cursor += width + 30
        positions[index] = (x, nodes[index].depth * 185)
        return x
    place(0)
    return positions, max(cursor, width + 30), (max(n.depth for n in nodes) + 1) * 185 + 90


def diagram(problem, nodes, path, title, subtitle):
    width, height = (108, 118) if problem == 'puzzle' else (164, 118)
    positions, total_width, total_height = geometry(nodes, width)
    path = set(path)
    fragments = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{total_width:.0f}" height="{total_height}" viewBox="0 0 {total_width:.0f} {total_height}" role="img" aria-label="{escape(title)}">',
        '<defs><marker id="arrow" markerWidth="8" markerHeight="8" refX="7" refY="3" orient="auto"><path d="M0,0 L0,6 L7,3 z" fill="context-stroke"/></marker></defs>',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        f'<text x="20" y="25" font-family="Arial" font-size="18" fill="#173440">{escape(title)}</text>',
        f'<text x="20" y="47" font-family="Arial" font-size="12" fill="#5c707c">{escape(subtitle)}</text>']
    for node in nodes[1:]:
        x, y = positions[node.id]
        px, py = positions[node.parent]
        on_path = node.id in path and node.parent in path
        color = '#358347' if on_path else '#9aa6ad'
        fragments.append(f'<path d="M{px+width/2+15},{py+height+70} L{x+width/2+15},{y+70}" stroke="{color}" stroke-width="{2.5 if on_path else 1}" fill="none" marker-end="url(#arrow)"/>')
        label_y = y + 57
        fragments.append(f'<text x="{x+width/2+15}" y="{label_y}" text-anchor="middle" font-family="Arial" font-size="11" fill="#435863" stroke="white" stroke-width="4" paint-order="stroke">{escape(node.action)}</text>')
    for node in nodes:
        x, y = positions[node.id]
        x, y = x + 15, y + 70
        kind = 'path' if node.id in path and node.kind != 'goal' else node.kind
        fill, stroke = COLORS[kind]
        dashed = ' stroke-dasharray="5 4"' if node.kind in ('repeat', 'cutoff') else ''
        tip = escape(f'#{node.id} | {node.state} | {node.reason or node.kind}', quote=True)
        fragments.append(f'<g class="tree-node" data-node="{node.id}"><title>{tip}</title><rect x="{x}" y="{y}" width="{width}" height="{height}" rx="10" fill="{fill}" stroke="{stroke}" stroke-width="{2 if node.id in path else 1.3}"{dashed}/>')
        fragments.append(f'<text x="{x+width/2}" y="{y+15}" text-anchor="middle" font-family="Arial" font-size="11" fill="#687b85">#{node.id}</text>')
        if problem == 'puzzle':
            for index, value in enumerate(node.state):
                r, c = divmod(index, 3)
                cx, cy = x + 18 + c * 24, y + 23 + r * 24
                fragments.append(f'<rect x="{cx}" y="{cy}" width="24" height="24" fill="{"#dce2e5" if value == 0 else "#ffffff"}" stroke="#87969c" stroke-width=".7"/>')
                if value:
                    fragments.append(f'<text x="{cx+12}" y="{cy+17}" text-anchor="middle" font-family="Arial" font-size="15" fill="#173440">{value}</text>')
        else:
            for row, line in enumerate(node_lines(problem, node.state)):
                fragments.append(f'<text x="{x+width/2}" y="{y+36+row*17}" text-anchor="middle" font-family="Arial" font-size="13" fill="#173440">{escape(line)}</text>')
        badge = {'repeat': f'repeat #{node.reference}', 'invalid': 'INVALID', 'cutoff': 'depth cutoff', 'goal': 'GOAL'}.get(node.kind, '')
        if badge:
            fragments.append(f'<text x="{x+width/2}" y="{y+height-7}" text-anchor="middle" font-family="Arial" font-size="10" fill="{stroke}">{badge}</text>')
        fragments.append('</g>')
    return ''.join(fragments) + '</svg>'


def png_preview(problem, nodes, path, target, title):
    # PNG is optional; SVG, JSON and the HTML viewer need only Python's standard library.
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        return False
    width, height = (108, 118) if problem == 'puzzle' else (164, 118)
    positions, full_width, full_height = geometry(nodes, width)
    canvas = Image.new('RGB', (int(full_width), int(full_height)), 'white')
    draw = ImageDraw.Draw(canvas)
    font_path = Path('C:/Windows/Fonts/arial.ttf')
    def font(size):
        return ImageFont.truetype(str(font_path), size) if font_path.exists() else ImageFont.load_default()
    draw.text((20, 15), title, fill='#173440', font=font(18))
    path = set(path)
    for node in nodes[1:]:
        x, y = positions[node.id]
        px, py = positions[node.parent]
        color = '#358347' if node.id in path else '#a6afb4'
        start, end = (px+width/2+15, py+height+70), (x+width/2+15, y+70)
        draw.line((*start, *end), fill=color, width=2)
        import math
        angle = math.atan2(end[1]-start[1], end[0]-start[0])
        draw.polygon([end, (end[0]-8*math.cos(angle-.4), end[1]-8*math.sin(angle-.4)),
                      (end[0]-8*math.cos(angle+.4), end[1]-8*math.sin(angle+.4))], fill=color)
        draw.text((x+width/2+15, y+57), node.action, anchor='mm', fill='#435863', font=font(10))
    for node in nodes:
        x, y = positions[node.id]
        x, y = x+15, y+70
        kind = 'path' if node.id in path and node.kind != 'goal' else node.kind
        fill, stroke = COLORS[kind]
        draw.rounded_rectangle((x, y, x+width, y+height), radius=10, fill=fill, outline=stroke, width=2)
        draw.text((x+width/2, y+11), f'#{node.id}', anchor='mm', fill='#6d7c83', font=font(11))
        if problem == 'puzzle':
            for index, value in enumerate(node.state):
                r, c = divmod(index, 3)
                cx, cy = x+18+c*24, y+23+r*24
                draw.rectangle((cx, cy, cx+24, cy+24), fill='#dce2e5' if not value else 'white', outline='#87969c')
                if value:
                    draw.text((cx+12, cy+12), str(value), anchor='mm', fill='#173440', font=font(15))
        else:
            for row, line in enumerate(node_lines(problem, node.state)):
                draw.text((x+width/2, y+32+row*17), line, anchor='mm', fill='#173440', font=font(12))
        badge = {'repeat': f'repeat #{node.reference}', 'invalid': 'INVALID', 'cutoff': 'depth cutoff', 'goal': 'GOAL'}.get(node.kind, '')
        draw.text((x+width/2, y+height-10), badge, anchor='mm', fill=stroke, font=font(10))
    canvas.save(target)
    return True


def solution_preview(river, path, solution, out):
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        return
    font_path = Path('C:/Windows/Fonts/arial.ttf')
    font = lambda size: ImageFont.truetype(str(font_path), size) if font_path.exists() else ImageFont.load_default()
    image = Image.new('RGB', (1380, 455), 'white')
    draw = ImageDraw.Draw(image)
    draw.text((25,15), 'Missionaries and Cannibals — shortest solution: 11 crossings', fill='#173440', font=font(23))
    draw.text((25,48), 'State = (missionaries left, cannibals left, boat side). Read row 1 left to right, then row 2 right to left.', fill='#5c707c', font=font(15))
    positions = []
    for step in range(len(path)):
        row, col = divmod(step, 6)
        if row % 2:
            col = 5-col
        positions.append((25+col*225, 95+row*175))
    for step, (x,y) in enumerate(positions):
        node = river[path[step]]
        draw.rounded_rectangle((x,y,x+180,y+110), radius=12, fill='#ecf8eb', outline='#358347', width=2)
        draw.text((x+90,y+18), f'Step {step} · #{node.id}', anchor='mm', fill='#5c707c', font=font(14))
        draw.text((x+90,y+53), str(node.state).replace("'",''), anchor='mm', fill='#173440', font=font(20))
        draw.text((x+90,y+88), 'GOAL' if step == 11 else 'Start' if step == 0 else node.action.split(':')[0], anchor='mm', fill='#358347', font=font(16))
        if step:
            px,py = positions[step-1]
            if py == y:
                a,b = ((px+185,py+55),(x-5,y+55)) if x>px else ((px-5,py+55),(x+185,y+55))
                draw.line((*a,*b),fill='#358347',width=2)
                tip=1 if x>px else -1
                draw.polygon([b,(b[0]-tip*8,b[1]-5),(b[0]-tip*8,b[1]+5)],fill='#358347')
            else:
                draw.line((x+90,py+115,x+90,y-5),fill='#358347',width=2)
                draw.polygon([(x+90,y-5),(x+85,y-13),(x+95,y-13)],fill='#358347')
    image.save(out/'river_solution.png')
    image = Image.new('RGB',(1080,370),'white')
    draw=ImageDraw.Draw(image)
    draw.text((25,15),'Rotating Workforce — verified 28-day cyclic sequence',fill='#173440',font=font(23))
    draw.text((25,49),'D = Day    A = Afternoon    N = Night    O = Off',fill='#5c707c',font=font(16))
    for day,label in enumerate(['Mon','Tue','Wed','Thu','Fri','Sat','Sun']):
        draw.text((220+day*113,91),label,anchor='mm',fill='#173440',font=font(16))
    for week in range(4):
        draw.text((25,137+week*48),f'Sequence week {week+1}',fill='#173440',font=font(17))
        for day in range(7):
            x,y=185+day*113,120+week*48
            shift=solution[week*7+day]
            draw.rounded_rectangle((x,y,x+70,y+38),radius=7,fill='#fff3dc' if shift=='O' else '#e8f3f5')
            draw.text((x+35,y+19),shift,anchor='mm',fill='#173440',font=font(20))
    draw.text((25,330),'Employee e in week w uses sequence week (e+w) mod 4. All constraints include the cycle boundary.',fill='#5c707c',font=font(15))
    image.save(out/'workforce_solution.png')



def expansion_pages(problem, nodes, path, title, full_svg, out):
    """One page per expanded parent; original IDs and statuses are preserved."""
    import re
    width, height = (108, 118) if problem == 'puzzle' else (164, 118)
    positions, _, _ = geometry(nodes, width)
    parents = [n.id for n in nodes if n.children]
    parents += [n.id for n in nodes if n.kind == 'goal']
    folder=out/f'{problem}_pages'
    folder.mkdir(exist_ok=True)
    pages=[]
    scale=1.25
    bw,bh=width*scale,height*scale
    try:
        from PIL import Image, ImageDraw, ImageFont
        source=Image.open(out/f'{problem}_tree.png').convert('RGB')
        font_path=Path('C:/Windows/Fonts/arial.ttf')
        font=lambda size: ImageFont.truetype(str(font_path),size) if font_path.exists() else ImageFont.load_default()
    except ImportError:
        source=None
    for page_number,parent_id in enumerate(parents,1):
        parent=nodes[parent_id]
        children=parent.children
        count=len(children)
        local={parent_id:((1200-bw)/2,85)}
        gap=24
        left=(1200-(count*bw+max(0,count-1)*gap))/2
        for i,child in enumerate(children):
            local[child]=(left+i*(bw+gap),310)
        heading=f'{title} — page {page_number} / {len(parents)}'
        subtitle=f'Expand node #{parent_id} at depth {parent.depth}; show only its immediate children.' if children else f'Goal node #{parent_id}; no further expansion.'
        fragments=['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="520" viewBox="0 0 1200 520" role="img" aria-label="'+escape(heading)+'">',
            '<defs><marker id="arrow" markerWidth="8" markerHeight="8" refX="7" refY="3" orient="auto"><path d="M0,0 L0,6 L7,3 z" fill="context-stroke"/></marker></defs>',
            '<rect width="100%" height="100%" fill="white"/>',
            f'<text x="25" y="30" font-family="Arial" font-size="23" fill="#173440">{escape(heading)}</text>',
            f'<text x="25" y="57" font-family="Arial" font-size="14" fill="#607480">{escape(subtitle)}</text>']
        image=None
        if source is not None:
            image=Image.new('RGB',(1200,520),'white')
            draw=ImageDraw.Draw(image)
            draw.text((25,10),heading,fill='#173440',font=font(23))
            draw.text((25,44),subtitle,fill='#607480',font=font(14))
        for child in children:
            px,py=local[parent_id]
            x,y=local[child]
            color='#358347' if parent_id in path and child in path else '#899ba5'
            a,b=(px+bw/2,py+bh),(x+bw/2,y)
            fragments.append(f'<path d="M{a[0]},{a[1]} L{b[0]},{b[1]}" fill="none" stroke="{color}" stroke-width="2" marker-end="url(#arrow)"/>')
            fragments.append(f'<text x="{x+bw/2}" y="{y-14}" text-anchor="middle" font-family="Arial" font-size="14" fill="#435863" stroke="white" stroke-width="4" paint-order="stroke">{escape(nodes[child].action)}</text>')
            if image:
                import math
                draw.line((*a,*b),fill=color,width=2)
                angle=math.atan2(b[1]-a[1],b[0]-a[0])
                draw.polygon([b,(b[0]-9*math.cos(angle-.4),b[1]-9*math.sin(angle-.4)),(b[0]-9*math.cos(angle+.4),b[1]-9*math.sin(angle+.4))],fill=color)
                draw.text((x+bw/2,y-16),nodes[child].action,anchor='mm',fill='#435863',font=font(13))
        for node_id,(x,y) in local.items():
            ox,oy=positions[node_id]
            ox,oy=ox+15,oy+70
            group=re.search(r'<g class="tree-node" data-node="'+str(node_id)+r'">.*?</g>',full_svg).group()
            fragments.append(f'<g transform="translate({x} {y}) scale({scale}) translate({-ox} {-oy})">{group}</g>')
            if image:
                tile=source.crop((int(ox),int(oy),int(ox+width+2),int(oy+height+2)))
                tile=tile.resize((int(bw+2),int(bh+2)),Image.Resampling.LANCZOS)
                image.paste(tile,(round(x),round(y)))
        footer='Green: solution path/prefix  |  Yellow: repeat  |  Red: invalid  |  Cutoff: not expanded in the bounded tree'
        fragments.append(f'<text x="25" y="500" font-family="Arial" font-size="13" fill="#607480">{footer}</text></svg>')
        svg=''.join(fragments)
        filename=f'step-{page_number:02d}'
        (folder/f'{filename}.svg').write_text(svg,encoding='utf8')
        if image:
            draw.text((25,485),footer,fill='#607480',font=font(12))
            image.save(folder/f'{filename}.png')
        pages.append({'parent':parent_id,'depth':parent.depth,'svg':svg,'file':f'{problem}_pages/{filename}.svg'})
        assert set(local)=={parent_id,*children}
    figures=''.join(f'<section><h2>Page {i+1}: node #{page["parent"]}</h2>{page["svg"]}</section>' for i,page in enumerate(pages))
    book='<!doctype html><html lang="en"><meta charset="utf-8"><title>'+escape(title)+' — compact pages</title><style>body{font-family:Arial;margin:0;background:#edf2f5}section{max-width:1200px;margin:20px auto;background:white;padding:16px}svg{width:100%;height:auto}h2{font-size:18px}@media print{@page{size:landscape;margin:10mm}body{background:white}section{margin:0;padding:0;break-after:page;page-break-after:always}section:last-child{break-after:auto}h2{display:none}}</style>'+figures+'</html>'
    (folder/'index.html').write_text(book,encoding='utf8')
    return pages


def serialize(nodes):
    return [vars(node) for node in nodes]


def verify_tree(nodes, invalid, goal):
    assert [n.id for n in nodes] == list(range(len(nodes)))
    for node in nodes:
        if node.parent is not None:
            assert node.id in nodes[node.parent].children
            assert node.depth == nodes[node.parent].depth + 1
        if node.kind == 'invalid':
            assert invalid(node.state)
            assert not node.children
        else:
            assert not invalid(node.state)
        if node.kind == 'repeat':
            assert nodes[node.reference].state == node.state and node.reference < node.id
            assert not node.children
        if node.kind == 'goal':
            assert goal(node.state) and not node.children


def validate_puzzle_path(nodes, path):
    assert nodes[path[0]].state == PUZZLE_START and nodes[path[-1]].state == PUZZLE_GOAL
    for a, b in itertools.pairwise(path):
        before, after = nodes[a].state, nodes[b].state
        assert sorted(before) == sorted(after) == list(range(9))
        changed = [i for i in range(9) if before[i] != after[i]]
        assert len(changed) == 2 and before.index(0) in changed and after.index(0) in changed
        x, y = changed
        assert abs(x//3-y//3) + abs(x%3-y%3) == 1
        assert before[x] == after[y] and before[y] == after[x]
    assert len(path)-1 == 3


def validate_river_path(nodes, path):
    assert nodes[path[0]].state == (3, 3, 'L') and nodes[path[-1]].state == (0, 0, 'R')
    for a, b in itertools.pairwise(path):
        m, c, boat = nodes[a].state
        nm, nc, next_boat = nodes[b].state
        assert boat != next_boat and not river_invalid((nm, nc, next_boat))
        dm, dc = abs(nm-m), abs(nc-c)
        assert 1 <= dm+dc <= 2
        direction = -1 if boat == 'L' else 1
        assert nm-m == direction*dm and nc-c == direction*dc
        assert dm <= (m if boat == 'L' else 3-m)
        assert dc <= (c if boat == 'L' else 3-c)
    assert len(path)-1 == 11


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=Path(__file__).parent/'generated')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    puzzle, pg = breadth_first(PUZZLE_START, puzzle_moves, lambda s: '', lambda s: s == PUZZLE_GOAL, 3)
    river, rg = breadth_first((3,3,'L'), river_moves, river_invalid, lambda s: s == (0,0,'R'), 12)
    workforce, _ = breadth_first('', workforce_moves, workforce_invalid, lambda s: len(s) == 28, 3)
    trace, wg = solve_workforce()
    assert wg is not None
    wp = path_to(trace, wg)
    solution = trace[wg].state
    validate_workforce_complete(solution)
    pp, rp = path_to(puzzle, pg), path_to(river, rg)
    validate_puzzle_path(puzzle, pp)
    validate_river_path(river, rp)
    assert river_invalid((2,3,'R')) and not river_invalid((0,3,'L'))
    assert workforce_invalid('ND') and workforce_invalid('D'*7) and workforce_invalid('O'*4)
    assert workforce_invalid('D'+'?'*6+'D')
    for tree, invalid, goal in [(puzzle, lambda s: '', lambda s: s == PUZZLE_GOAL), (river, river_invalid, lambda s: s == (0,0,'R')), (workforce, workforce_invalid, lambda s: len(s) == 28)]:
        verify_tree(tree, invalid, goal)
    # Cross-check the fixed-order DFS against the independently checked C++ model.
    assert solution == 'DDDDDDOAAAAAODNNNOOANOOONNNO'
    assert len(trace)-1 == 1948
    solution_preview(river, rp, solution, args.out)
    wf_path = [node.id for node in workforce if solution.startswith(node.state) and node.kind != 'invalid']
    datasets = [
        ('puzzle', '8-Puzzle', puzzle, pp, 'BFS through depth 3; actions move the blank. Green path: Right, Down, Right.'),
        ('river', 'Missionaries and Cannibals', river, rp, 'BFS discovery tree; repeats and invalid attempts are terminal leaves. Shortest solution: 11 crossings.'),
        ('workforce', 'Rotating Workforce Scheduling', workforce, wf_path, 'BFS prefix through depth 3. Green shows the prefix of a verified 28-decision solution, not a goal at depth 3.'),
    ]
    data = {}
    for key, title, nodes, path, subtitle in datasets:
        svg = diagram(key, nodes, path, title, subtitle)
        (args.out/f'{key}_tree.svg').write_text(svg, encoding='utf8')
        png_preview(key, nodes, path, args.out/f'{key}_tree.png', title)
        pages=expansion_pages(key,nodes,path,title,svg,args.out)
        data[key] = {'title':title, 'subtitle':subtitle, 'nodes':serialize(nodes), 'path':path, 'svg':svg, 'pages':pages}
    (args.out/'workforce_dfs_trace.json').write_text(json.dumps(serialize(trace), indent=2), encoding='utf8')
    data['workforce']['solution'] = solution
    data['workforce']['solutionStates'] = [trace[i].state for i in wp]
    (args.out/'trees.json').write_text(json.dumps(data, indent=2), encoding='utf8')
    checks = {'puzzle': {'shortestMoves':3, 'path':[puzzle[i].action for i in pp[1:]], 'nodes':len(puzzle)},
              'river': {'shortestCrossings':11, 'path':[list(river[i].state) for i in rp], 'nodes':len(river)},
              'workforce': {'solution':solution, 'attempts':len(trace)-1, 'independentValidation':'PASS'},
              'treeInvariants':'PASS', 'note':'Display cutoffs do not mean UNSAT. Workforce rules are the confirmed educational instance.'}
    (args.out/'verification.json').write_text(json.dumps(checks, indent=2), encoding='utf8')
    template = (Path(__file__).parent/'viewer.html').read_text(encoding='utf8')
    (args.out/'index.html').write_text(template.replace('/* TREE_DATA */', 'const data = '+json.dumps(data).replace('</','<\\/')+';'), encoding='utf8')
    print(json.dumps(checks, indent=2))
    print(f'Open {args.out.resolve()/"index.html"}')


if __name__ == '__main__':
    main()
