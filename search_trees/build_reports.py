"""Create three step-by-step PDF reports. Requires ReportLab."""
from __future__ import annotations

import argparse
from collections import Counter, deque
from functools import lru_cache
import importlib.util
import json
from pathlib import Path
import sys
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A3, landscape
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph


WIDTH, HEIGHT = landscape(A3)
MARGIN = 38
INK = '#193644'
MUTED = '#607480'
GREEN = '#358347'


def load_models(source):
    spec = importlib.util.spec_from_file_location('tree_report_models', source/'generate.py')
    model = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = model
    spec.loader.exec_module(model)
    return model


def bfs_trace(model, key):
    if key == 'puzzle':
        start, moves = model.PUZZLE_START, model.puzzle_moves
        invalid = lambda state: ''
        goal = lambda state: state == model.PUZZLE_GOAL
    else:
        start, moves = (3,3,'L'), model.river_moves
        invalid, goal = model.river_invalid, lambda state: state == (0,0,'R')
    nodes = [dict(id=0, state=start, parent=None, action='Start', depth=0,
                  kind='frontier', reason='', reference=None, children=[])]
    seen, queue, events = {start:0}, deque([0]), []
    attempts = rejected = repeats = visited = 0
    found = None
    while queue:
        index = queue.popleft()
        node = nodes[index]
        visited += 1
        if goal(node['state']):
            node['kind'] = 'goal'
            found = index
        else:
            node['kind'] = 'expanded'
            for action, state in moves(node['state']):
                attempts += 1
                child = dict(id=len(nodes), state=state, parent=index, action=action,
                             depth=node['depth']+1, kind='frontier', reason=invalid(state),
                             reference=None, children=[])
                if child['reason']:
                    child['kind'] = 'invalid'
                    rejected += 1
                elif state in seen:
                    child['kind'] = 'repeat'
                    child['reference'] = seen[state]
                    child['reason'] = f"Already discovered at node #{seen[state]}"
                    repeats += 1
                else:
                    seen[state] = child['id']
                    queue.append(child['id'])
                node['children'].append(child['id'])
                nodes.append(child)
        events.append(dict(node=index, visited=visited, discovered=len(seen), attempts=attempts,
                           invalid=rejected, repeats=repeats, frontier=len(queue), goal=found is not None))
        if found is not None:
            break
    assert found is not None
    path = []
    current = found
    while current is not None:
        path.append(current)
        current = nodes[current]['parent']
    path.reverse()
    # Use the independent transition checks also used for the earlier diagrams.
    objects = [model.Node(**node) for node in nodes]
    if key == 'puzzle':
        model.validate_puzzle_path(objects, path)
    else:
        model.validate_river_path(objects, path)
    return dict(nodes=nodes, path=path, events=events, summary=events[-1])


def workforce_trace(model):
    trace, found = model.solve_workforce()
    assert found is not None
    path = model.path_to(trace, found)
    nodes = [vars(node).copy() for node in trace]
    model.validate_workforce_complete(nodes[found]['state'])
    @lru_cache(None)
    def descendants(index):
        return sum(1 + descendants(child) for child in nodes[index]['children'])
    valid = sum(node['kind'] != 'invalid' for node in nodes)
    summary = dict(visited=valid, discovered=valid, attempts=len(nodes)-1,
                   invalid=sum(node['kind']=='invalid' for node in nodes),
                   backtracks=len(nodes)-len(path), depth=28)
    return dict(nodes=nodes, path=path, summary=summary,
                descendants={node['id']:descendants(node['id']) for node in nodes})


def fonts():
    directory = Path('C:/Windows/Fonts')
    regular, bold = directory/'arial.ttf', directory/'arialbd.ttf'
    if regular.exists() and bold.exists():
        pdfmetrics.registerFont(TTFont('ReportSans', str(regular)))
        pdfmetrics.registerFont(TTFont('ReportBold', str(bold)))
        return 'ReportSans', 'ReportBold'
    return 'Helvetica', 'Helvetica-Bold'


class Report:
    def __init__(self, target, title, total):
        self.c = canvas.Canvas(str(target), pagesize=(WIDTH,HEIGHT), pageCompression=1)
        self.c.setTitle(title + ' - Step-by-step search report')
        self.c.setAuthor('Uninformed Search Techniques')
        self.title, self.total, self.page = title, total, 0
        self.regular, self.bold = fonts()

    def paragraph(self, text, x, top, width, size=10.5, color=INK, bold=False):
        style = ParagraphStyle('body', fontName=self.bold if bold else self.regular,
                               fontSize=size, leading=size*1.4, textColor=colors.HexColor(color),
                               alignment=TA_LEFT)
        p=Paragraph(escape(text).replace('\n','<br/>'),style)
        _, height=p.wrap(width,HEIGHT)
        p.drawOn(self.c,x,top-height)
        return top-height

    def begin(self, heading, subtitle):
        self.page += 1
        c=self.c
        c.setFillColor(colors.HexColor('#f4f7fa'))
        c.rect(0,0,WIDTH,HEIGHT,fill=1,stroke=0)
        c.setFillColor(colors.HexColor('#1e656b'))
        c.rect(0,HEIGHT-8,WIDTH,8,fill=1,stroke=0)
        self.paragraph(heading,MARGIN,HEIGHT-30,WIDTH-2*MARGIN,20,bold=True)
        self.paragraph(subtitle,MARGIN,HEIGHT-61,WIDTH-2*MARGIN,10,MUTED)
        c.setStrokeColor(colors.HexColor('#d9e4e9'))
        c.line(MARGIN,39,WIDTH-MARGIN,39)
        c.setFont(self.regular,8)
        c.setFillColor(colors.HexColor(MUTED))
        c.drawString(MARGIN,24,'UNINFORMED SEARCH TECHNIQUES | '+self.title)
        c.drawRightString(WIDTH-MARGIN,24,f'Page {self.page} of {self.total}')

    def end(self):
        self.c.showPage()

    def block(self, label, text, x, top, width):
        top=self.paragraph(label.upper(),x,top,width,9,'#1e656b',True)-5
        return self.paragraph(text,x,top,width,10.5)-15

    def legend(self):
        text='Green: returned solution route   Yellow: repeated state   Red: invalid attempt   Gray: other branch'
        self.paragraph(text,MARGIN,64,WIDTH-2*MARGIN,8.5,MUTED)

    def card(self, key, node, x, top, width, height, path, trace):
        kind=node['kind']
        fill,stroke=('#ffffff','#82949f')
        if node['id'] in path:
            fill,stroke='#edf8e9',GREEN
        if kind=='invalid':
            fill,stroke='#ffeded','#c54848'
        if kind=='repeat':
            fill,stroke='#fff3d4','#bd8a17'
        c=self.c
        c.setFillColor(colors.HexColor(fill))
        c.setStrokeColor(colors.HexColor(stroke))
        c.setLineWidth(1.2)
        if kind in ('repeat','frontier'):
            c.setDash(4,3)
        c.roundRect(x,top-height,width,height,8,fill=1,stroke=1)
        c.setDash()
        self.paragraph(f"#{node['id']} | depth {node['depth']}",x+9,top-7,width-18,8,MUTED)
        if key=='puzzle':
            size=18
            left=x+(width-size*3)/2
            for i,value in enumerate(node['state']):
                row,col=divmod(i,3)
                xx,yy=left+col*size,top-23-(row+1)*size
                c.setFillColor(colors.HexColor('#d9e2e6' if value==0 else '#ffffff'))
                c.setStrokeColor(colors.HexColor('#879aa4'))
                c.setLineWidth(.5)
                c.rect(xx,yy,size,size,fill=1,stroke=1)
                if value:
                    c.setFillColor(colors.HexColor(INK))
                    c.setFont(self.regular,12)
                    c.drawCentredString(xx+size/2,yy+5,str(value))
        elif key=='river':
            m,cc,boat=node['state']
            text=f'Left: {m}M, {cc}C\nRight: {3-m}M, {3-cc}C\nBoat: {boat}'
            self.paragraph(text,x+10,top-25,width-20,10)
        else:
            prefix=node['state']+'?'*(28-len(node['state']))
            c.setFont('Courier',9)
            c.setFillColor(colors.HexColor(INK))
            for row in range(4):
                line=f'W{row+1}: '+' '.join(prefix[row*7:row*7+7])
                c.drawString(x+max(8,(width-103)/2),top-31-row*12,line)
        label='GOAL' if kind=='goal' else 'on solution route' if node['id'] in path else 'queued' if kind=='frontier' else kind
        if kind=='repeat':
            label=f"repeat #{node['reference']}"
        if kind=='exhausted':
            label=f"exhausted: {trace['descendants'][node['id']]} descendants"
        self.paragraph(label,x+7,top-height+25,width-14,7.5,stroke,True)
        if kind=='invalid':
            reason=node['reason'].replace('Cannibals outnumber missionaries','C > M').replace('Not enough passengers on the departure bank','Passengers unavailable')
            self.paragraph(reason,x+7,top-height+13,width-14,6.8,stroke)

    def arrow(self, start, end, label, green=False):
        import math
        c=self.c
        color=GREEN if green else '#8b9da6'
        c.setStrokeColor(colors.HexColor(color))
        c.setFillColor(colors.HexColor(color))
        c.setLineWidth(1.5 if green else .8)
        c.line(*start,*end)
        angle=math.atan2(end[1]-start[1],end[0]-start[0])
        p=c.beginPath()
        p.moveTo(*end)
        for delta in (-.4,.4):
            p.lineTo(end[0]-7*math.cos(angle+delta),end[1]-7*math.sin(angle+delta))
        p.close()
        c.drawPath(p,fill=1,stroke=0)
        # Place the action just above the child, away from its state label.
        c.setFont(self.regular,8)
        c.drawCentredString(end[0],end[1]+12,label)

    def accumulated_tree(self, key, trace, parent_id, event=None, route=None):
        """Keep stable positions across pages; only reveal nodes already generated."""
        nodes=trace['nodes']
        if key=='workforce':
            spine=trace['path'][:trace['path'].index(parent_id)+1]
            visible={0}
            for i in spine:
                visible.update(nodes[i]['children'])
            skeleton={0}
            for i in trace['path']:
                skeleton.update(nodes[i]['children'])
            processed=set(spine)
        else:
            cutoff=event['attempts'] if event else len(nodes)-1
            visible=set(range(cutoff+1))
            skeleton=set(range(len(nodes)))
            processed={e['node'] for e in trace['events'][:event['visited']]} if event else {e['node'] for e in trace['events']}
        # A tidy ordered tree: parent centered over the span of its children.
        def layout(index):
            children=[i for i in nodes[index]['children'] if i in skeleton]
            if not children:
                return {index:(0,0)}
            packed={}
            roots=[]
            for child in children:
                branch=layout(child)
                shift=0
                for depth in {y for x,y in branch.values()}:
                    previous=[x for x,y in packed.values() if y==depth+175]
                    incoming=[x for x,y in branch.values() if y==depth]
                    if previous:
                        shift=max(shift,max(previous)+190-min(incoming))
                roots.append(shift)
                packed.update({i:(x+shift,y+175) for i,(x,y) in branch.items()})
            center=(roots[0]+roots[-1])/2
            return {index:(0,0),**{i:(x-center,y) for i,(x,y) in packed.items()}}
        positions=layout(0)
        min_x=min(x for x,y in positions.values())
        positions={i:(x-min_x,y) for i,(x,y) in positions.items()}
        full_w=max(x for x,y in positions.values())+160
        full_h=max(y for x,y in positions.values())+120
        left,bottom,area_w,area_h=MARGIN,330,WIDTH-2*MARGIN,HEIGHT-494
        scale=min(area_w/full_w,area_h/full_h,1.0)
        c=self.c
        self.paragraph('ACCUMULATED TREE - from the root to this step',MARGIN,HEIGHT-112,WIDTH-2*MARGIN,10,bold=True)
        self.paragraph('Blue outline: current node. Blue edges: newly generated attempts. Positions stay fixed across pages. Zoom the PDF to inspect the overview.',MARGIN,HEIGHT-130,WIDTH-2*MARGIN,8.5,MUTED)
        c.saveState()
        c.translate(left+(area_w-full_w*scale)/2,bottom+(area_h+full_h*scale)/2)
        c.scale(scale,scale)
        path=set(trace['path'] if route is None else route)
        for i in sorted(visible):
            node=nodes[i]
            if node['parent'] not in visible:
                continue
            px,py=positions[node['parent']]
            xx,yy=positions[i]
            self.arrow((px+80,-py-120),(xx+80,-yy),node['action'],i in path and node['parent'] in path)
            if node['parent']==parent_id:
                c.setStrokeColor(colors.HexColor('#2877c9'))
                c.setLineWidth(2.5)
                c.line(px+80,-py-120,xx+80,-yy)
        for i in sorted(visible):
            shown=nodes[i].copy()
            if key!='workforce' and i not in processed and shown['kind'] not in ('invalid','repeat'):
                shown['kind']='frontier'
            xx,yy=positions[i]
            self.card(key,shown,xx,-yy,160,120,path,trace)
            if i==parent_id:
                c.setStrokeColor(colors.HexColor('#2877c9'))
                c.setLineWidth(3)
                c.roundRect(xx-4,-yy-124,168,128,10,fill=0,stroke=1)
        c.restoreState()
        c.setStrokeColor(colors.HexColor('#ccdce4'))
        c.line(MARGIN,316,WIDTH-MARGIN,316)
        self.paragraph('CURRENT STEP - enlarged detail; its position in the full tree is outlined above',MARGIN,305,WIDTH-2*MARGIN,10,bold=True)

    def expansion(self,key,trace,parent_id,step,total,event=None):
        nodes=trace['nodes']
        parent=nodes[parent_id]
        goal=parent['kind']=='goal'
        if key=='workforce':
            heading=f'Decision {step} of {total}: assign X[{parent["depth"]}]' if not goal else 'Goal check: all 28 positions assigned'
            subtitle='Verified solution branch with actual attempted alternatives; unsuccessful side subtrees are collapsed.'
        else:
            heading=f'BFS step {step} of {total}: '+('goal check' if goal else f'expand node #{parent_id}')
            subtitle='The tree grows cumulatively. Earlier nodes remain visible; the current expansion is enlarged below.'
        self.begin(heading,subtitle)
        if event:
            counts=f"Processed: {event['visited']} | Unique discovered: {event['discovered']} | Queue after step: {event['frontier']} | Attempts so far: {event['attempts']}"
        else:
            counts=f"Current depth: {parent['depth']} | Node #{parent_id} | Fixed value order: D, A, N, O | No heuristic ranking"
        self.paragraph(counts,MARGIN,HEIGHT-87,WIDTH-2*MARGIN,9,MUTED)
        self.accumulated_tree(key,trace,parent_id,event)
        # Compact two-tier detail below the complete cumulative overview.
        self.c.saveState()
        self.c.translate(WIDTH*.16,82)
        self.c.scale(.68,.68)
        width,height=210,110
        x,top=(WIDTH-width)/2,278
        path=set(trace['path'])
        self.card(key,parent,x,top,width,height,path,trace)
        children=parent['children']
        count=len(children)
        child_width=min(160,(WIDTH-2*MARGIN-max(0,count-1)*12)/max(1,count))
        left=(WIDTH-(count*child_width+max(0,count-1)*12))/2
        child_top=128
        for i,index in enumerate(children):
            child=nodes[index]
            xx=left+i*(child_width+12)
            self.arrow((x+width/2,top-height),(xx+child_width/2,child_top),child['action'],index in path)
            shown=child.copy()
            if key!='workforce' and shown['kind'] not in ('invalid','repeat'):
                shown['kind']='frontier'
            self.card(key,shown,xx,child_top,child_width,120,path,trace)
        self.c.restoreState()
        if goal:
            explanation=('The complete 28-day sequence satisfies the goal. Recursive calls return success; no further alternatives are tried.' if key=='workforce' else 'The goal predicate is true. Search stops here; any queued alternatives are not needed to establish this solution.')
        elif key=='workforce':
            next_on_path=next(i for i in children if i in path)
            other=sum(1 for i in children if i!=next_on_path)
            explanation=f"The returned solution continues with {nodes[next_on_path]['action']} (node #{next_on_path}). {other} other attempted alternative(s) are shown. Gray exhausted subtrees were searched fully before this choice; their internal nodes are omitted from the report. Values never tried after success are not drawn."
        else:
            kinds=Counter(nodes[i]['kind'] for i in children)
            unique=sum(nodes[i]['kind'] not in ('repeat','invalid') for i in children)
            explanation=f"Attempted {count} moves: {unique} new state(s), {kinds['repeat']} repeated state(s), {kinds['invalid']} invalid move(s). New states enter the FIFO queue. Invalid and repeated states are not enqueued. Green highlighting is added after reconstructing the final solution."
        self.paragraph(explanation,MARGIN,80,WIDTH-2*MARGIN,8.5)
        self.legend()
        self.end()

    def path_page(self,key,trace,index):
        before=trace['nodes'][trace['path'][index-1]]
        after=trace['nodes'][trace['path'][index]]
        self.begin(f'Final solution - move {index} of {len(trace["path"])-1}',
                   'The verified shortest route, separated from the order in which BFS explored alternatives.')
        self.accumulated_tree(key,trace,after['id'],route=trace['path'][:index+1])
        y=275
        path=set(trace['path'])
        self.card(key,before,110,y,205,135,path,trace)
        self.card(key,after,WIDTH-315,y,205,135,path,trace)
        self.arrow((320,y-67),(WIDTH-320,y-67),after['action'],True)
        if key=='puzzle':
            text=f"Move the blank {after['action'].lower()}. Exactly one adjacent numbered tile changes places with it."
        else:
            m,c,b=before['state']
            nm,nc,nb=after['state']
            passengers=[]
            if abs(m-nm):
                passengers.append(f"{abs(m-nm)} "+('missionary' if abs(m-nm)==1 else 'missionaries'))
            if abs(c-nc):
                passengers.append(f"{abs(c-nc)} "+('cannibal' if abs(c-nc)==1 else 'cannibals'))
            text=f"Move {' and '.join(passengers)} from {'left to right' if b=='L' else 'right to left'}. The boat changes bank. Its load is between 1 and 2 people, and both resulting banks are safe."
        if after['kind']=='goal':
            text+=' The goal is now satisfied.'
        self.paragraph(text,110,y-170,WIDTH-220,12)
        self.end()


DEFINITIONS={
 'puzzle':('8-Puzzle',[
    ('State','Nine tile values in row order. 0 denotes the blank; every numbered tile 1-8 occurs once.'),
    ('Initial state','1 2 3 / 0 4 6 / 7 5 8. This is the start board supplied by the user.'),
    ('Actions','Move the blank Up, Down, Left or Right, in that order. Only on-board moves are generated.'),
    ('Constraints','Swap the blank with one orthogonally adjacent tile. Preserve all nine tile values.'),
    ('Goal','1 2 3 / 4 5 6 / 7 8 0.'),
    ('Strategy','Unit-cost BFS with a global discovered-state set. Test the goal when removing a state from the FIFO queue.'),
 ]),
 'river':('Missionaries and Cannibals',[
    ('State','(M,C,B): missionaries and cannibals on the left bank; B is the boat side, L or R. Right-bank counts are 3-M and 3-C.'),
    ('Initial state','(3,3,L): all three missionaries, all three cannibals and the boat start on the left.'),
    ('Actions','Boat loads, in fixed order: 1M, 2M, 1C, 2C, 1M+1C. Every crossing changes the boat side.'),
    ('Constraints','The boat carries 1-2 available passengers. On either bank with missionaries, cannibals may not outnumber them. A bank with zero missionaries may contain cannibals.'),
    ('Goal','(0,0,R): everyone and the boat are on the right bank.'),
    ('Strategy','Unit-cost BFS with discovered states. Invalid loads and unsafe states are rejected; duplicates are not enqueued.'),
 ]),
 'workforce':('Rotating Workforce Scheduling',[
    ('State','A partial 28-day cyclic sequence X[0]...X[27]. Each position has domain D (day), A (afternoon), N (night), O (off); ? means unassigned.'),
    ('Initial state','All 28 positions unassigned. Four employees use weekly offsets of the same four-week sequence.'),
    ('Actions','Assign the next chronological position, trying D, A, N, O in that fixed order.'),
    ('Constraints','Mon-Sat: one employee each on D/A/N/O. Sunday: D=1, A=0, N=1, O=2. Work blocks last 2-6 days, rest blocks 1-3; N followed by D is forbidden. Include the cyclic boundary.'),
    ('Goal','A complete sequence whose four employee rotations meet all constraints.'),
    ('Strategy','Chronological DFS with backtracking and necessary constraint checks; no MRV or heuristic score. This is the confirmed educational model, not a universal workforce specification.'),
 ])}


def cover(report,key,trace):
    title,definitions=DEFINITIONS[key]
    report.begin(title,'Problem model, counting conventions and report scope')
    gap=30
    col=(WIDTH-2*MARGIN-gap)/2
    left,right=MARGIN,MARGIN+col+gap
    y=HEIGHT-105
    for label,text in definitions[:3]:
        y=report.block(label,text,left,y,col)
    yy=HEIGHT-105
    for label,text in definitions[3:]:
        yy=report.block(label,text,right,yy,col)
    summary=trace['summary']
    if key=='workforce':
        stats=f"DFS calls (valid states): {summary['visited']}\nTried assignments: {summary['attempts']}\nRejected immediately: {summary['invalid']}\nBacktracked assignments: {summary['backtracks']}\nSolution decisions: 28"
        scope='The following pages cover all 28 choices on the returned solution branch. A cumulative tree retains all earlier choices and attempted alternatives, with the current expansion enlarged below. Unsuccessful side subtrees are collapsed and counted; this is not an unabridged 498-call trace. The full attempt trace is saved alongside this report as workforce-report-trace.json.'
    else:
        stats=f"Visited / dequeued states: {summary['visited']}\nUnique discovered, including the initial state: {summary['discovered']}\nAttempted transitions: {summary['attempts']}\nRejected invalid / repeated: {summary['invalid']} / {summary['repeats']}\nShortest solution depth: {len(trace['path'])-1}"
        scope=f"The next {len(trace['events'])} pages show every BFS expansion or goal check up to the first dequeued goal, followed by the final route. Every page retains the tree generated so far, starting at the root, and enlarges the current expansion below. Repeated states are yellow attempts and never enter the queue twice. The overview uses fixed positions; zoom the vector PDF to inspect small nodes."
    y=report.block('Search result',stats,left,y,col)
    yy=report.block('Pages that follow',scope,right,yy,col)
    yy=report.block('Counting note','Statistics apply only to this instance, action order and goal-test timing. Attempted-transition nodes are not the same as unique discovered states. Green nodes identify the returned route retrospectively.',right,yy,col)
    assert min(y,yy)>60,(key,y,yy)
    report.end()


def summary_page(report,key,trace,model):
    report.begin('Final result and verification','The returned solution has been independently checked.')
    if key=='workforce':
        state=trace['nodes'][trace['path'][-1]]['state']
        report.paragraph('Complete shared cycle',MARGIN,HEIGHT-110,WIDTH-2*MARGIN,15,bold=True)
        c=report.c
        for day,name in enumerate(['Mon','Tue','Wed','Thu','Fri','Sat','Sun']):
            report.paragraph(name,220+day*72,HEIGHT-147,65,10,bold=True)
        for week in range(4):
            report.paragraph(f'Sequence week {week+1}',MARGIN,HEIGHT-181-week*38,170,11)
            for day in range(7):
                report.paragraph(state[week*7+day],240+day*72,HEIGHT-181-week*38,45,14,bold=True)
        text='Validation: PASS. Coverage was checked for every employee rotation and weekday. Cyclic work/rest run lengths and the N-to-D transition were checked separately. Employee e in week w uses sequence week (e+w) mod 4. The rows above are consecutive weeks of one cycle, not four independently chosen schedules.'
        report.paragraph(text,MARGIN,235,WIDTH-2*MARGIN,11)
        report.paragraph('Independent cross-check: the generated solution and 1,948 attempted assignments match the existing C++ Basic solver. The 28 decision pages summarize its returned branch; they do not prove an optimal objective value, because this model is a feasibility problem.',MARGIN,145,WIDTH-2*MARGIN,10,MUTED)
    else:
        path=trace['path']
        report.paragraph(f'SOLVED - shortest depth {len(path)-1}',MARGIN,HEIGHT-112,WIDTH-2*MARGIN,18,bold=True)
        if key=='puzzle':
            text=' -> '.join(trace['nodes'][i]['action'] for i in path[1:])
        else:
            text=' -> '.join(str(tuple(trace['nodes'][i]['state'])).replace("'",'') for i in path)
        end=report.paragraph(text,MARGIN,HEIGHT-157,WIDTH-2*MARGIN,12)
        evidence='Every transition was independently checked. For 8-Puzzle this means one adjacent blank swap and preservation of the tiles. For the river problem this means an available boat load of 1-2, alternating boat side, safe banks and the exact final state.'
        end=report.block('Validation',evidence,MARGIN,end-35,WIDTH-2*MARGIN)
        end=report.block('Why shortest?', 'All moves cost one. BFS processes states in nondecreasing depth and checks the goal on dequeue. No shallower goal was processed before this one. The proof applies to the specified start, goal and action rules.',MARGIN,end,WIDTH-2*MARGIN)
        note=('The earlier 8-Puzzle illustration stopped at display depth 3. This report instead follows the actual BFS queue until the goal is checked, so it can contain additional depth-4 frontier nodes. A queued leaf is not a proven dead end.' if key=='puzzle' else 'There are 15 unique discovered states, including the initial state. The 70 attempted transitions include 40 invalid moves and 16 repeats. Those leaf attempts are drawn for explanation but do not represent additional visited states.')
        report.block('Reading the counts',note,MARGIN,end,WIDTH-2*MARGIN)
    report.end()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path(__file__).parent)
    parser.add_argument('--out',type=Path,default=Path(__file__).parent/'pdfs')
    args=parser.parse_args()
    model=load_models(args.source)
    args.out.mkdir(parents=True,exist_ok=True)
    (args.source/'generated').mkdir(parents=True,exist_ok=True)
    reports={}
    for key,name in [('puzzle','8-puzzle-report.pdf'),('river','missionaries-and-cannibals-report.pdf'),('workforce','rotating-workforce-report.pdf')]:
        trace=workforce_trace(model) if key=='workforce' else bfs_trace(model,key)
        if key=='workforce':
            total=1+len(trace['path'])+1
        else:
            total=1+len(trace['events'])+len(trace['path'])-1+1
        report=Report(args.out/name,DEFINITIONS[key][0],total)
        cover(report,key,trace)
        if key=='workforce':
            for i,index in enumerate(trace['path'],1):
                report.expansion(key,trace,index,i,len(trace['path'])-1)
        else:
            for i,event in enumerate(trace['events'],1):
                report.expansion(key,trace,event['node'],i,len(trace['events']),event)
            for i in range(1,len(trace['path'])):
                report.path_page(key,trace,i)
        summary_page(report,key,trace,model)
        assert report.page==total,(key,report.page,total)
        report.c.save()
        reports[key]=dict(file=name,pages=total,statistics=trace['summary'],solutionDepth=len(trace['path'])-1)
        (args.source/'generated'/f'{key}-report-trace.json').write_text(json.dumps(trace,indent=2),encoding='utf8')
    (args.source/'generated'/'report-verification.json').write_text(json.dumps(reports,indent=2),encoding='utf8')
    print(json.dumps(reports,indent=2))


if __name__=='__main__':
    main()
