const $ = (id) => document.getElementById(id);
const names = {puzzle:'8-puzzle-report',river:'missionaries-and-cannibals-report',workforce:'rotating-workforce-report'};
const models = {
  puzzle:'State: nine tiles in row order; 0 is the blank. Move the blank Up, Down, Left or Right. BFS uses a FIFO queue and rejects discovered duplicates. Goal: 1 2 3 / 4 5 6 / 7 8 0.',
  river:'State (M,C,B): missionaries and cannibals on the left; boat side L/R. Start (3,3,L), goal (0,0,R). Boat capacity 1–2. On either bank with missionaries, cannibals cannot outnumber them. FIFO BFS; repeated states are not enqueued.',
  workforce:'One shared 28-day cycle, four employees at weekly offsets. Values D/A/N/O. Mon–Sat: one of each; Sunday: D=1, A=0, N=1, O=2. Work runs 2–6 days, rest 1–3, no N→D, including the cyclic boundary. Chronological DFS with backtracking.'
};
let trace, key='puzzle', step=0, timer=null, zoom=1, width=1, height=1, loadId=0;
const esc = (s) => String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function stop(){clearInterval(timer);timer=null;$('play').textContent='Play';}
function count(){return $('mode').value==='solution'||key==='workforce'?trace.path.length:trace.events.length;}
function stateText(node){
  if(key==='puzzle')return [0,3,6].map(i=>node.state.slice(i,i+3).map(v=>v||'·').join(' ')).join('\n');
  if(key==='river'){const [m,c,b]=node.state;return `(${m}, ${c}, ${b})\nLeft: ${m}M, ${c}C\nRight: ${3-m}M, ${3-c}C`;}
  const s=node.state.padEnd(28,'?');return [0,7,14,21].map((i,w)=>`W${w+1}: ${s.slice(i,i+7).split('').join(' ')}`).join('\n');
}
function inspect(id){const n=trace.nodes[id];$('node').textContent=`Node #${id} | depth ${n.depth}\n${stateText(n)}\nAction: ${n.action}\nRecorded outcome: ${n.kind}${n.reason?'\n'+n.reason:''}${n.reference!==null?'\nOriginal node: #'+n.reference:''}${n.kind==='exhausted'?'\nCollapsed descendants: '+trace.descendants[id]:''}`;}
function scale(){ $('tree').style.width=width*zoom+'px';$('tree').style.height=height*zoom+'px';}
function fit(){zoom=Math.min(($('viewport').clientWidth-24)/width,($('viewport').clientHeight-24)/height,1);scale();}
function draw(reset=false){
  if(!trace)return;
  const solution=$('mode').value==='solution', nodes=trace.nodes;
  const current=solution||key==='workforce'?trace.path[step]:trace.events[step].node;
  let visible, skeleton, processed=new Set(), route=new Set(solution?trace.path.slice(0,step+1):trace.path);
  if(key==='workforce'){
    skeleton=new Set([0]);for(const i of trace.path)for(const c of nodes[i].children)skeleton.add(c);
    visible=new Set([0]);for(const i of trace.path.slice(0,step+1))for(const c of nodes[i].children)visible.add(c);
  }else{
    skeleton=new Set(nodes.map(n=>n.id));
    const event=solution?trace.events.at(-1):trace.events[step];
    visible=new Set(nodes.slice(0,event.attempts+1).map(n=>n.id));
    processed=new Set(trace.events.slice(0,solution?trace.events.length:step+1).map(e=>e.node));
  }
  function layout(id){
    const children=nodes[id].children.filter(i=>skeleton.has(i));if(!children.length)return new Map([[id,[0,0]]]);
    const packed=new Map(),roots=[];
    for(const child of children){const branch=layout(child);let shift=0;
      for(const [x,y] of branch.values())for(const [px,py] of packed.values())if(py===y+185)shift=Math.max(shift,px+195-x);
      roots.push(shift);for(const [i,[x,y]]of branch)packed.set(i,[x+shift,y+185]);
    }
    const center=(roots[0]+roots.at(-1))/2;return new Map([[id,[0,0]],...[...packed].map(([i,[x,y]])=>[i,[x-center,y]])]);
  }
  const pos=layout(0),minX=Math.min(...[...pos.values()].map(p=>p[0]));
  for(const p of pos.values()){p[0]+=30-minX;p[1]+=30;}
  width=Math.max(...[...pos.values()].map(p=>p[0]))+200;height=Math.max(...[...pos.values()].map(p=>p[1]))+160;
  let svg='<defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse"><path d="M 0 0 L 10 5 L 0 10 z" fill="#8199a4"/></marker></defs>';
  for(const i of visible){const n=nodes[i];if(!visible.has(n.parent))continue;const [px,py]=pos.get(n.parent),[x,y]=pos.get(i);const fresh=!solution&&n.parent===current;
    svg+=`<path class="edge ${fresh?'new':route.has(i)&&route.has(n.parent)?'route':''}" marker-end="url(#arrow)" d="M${px+85},${py+128} L${x+85},${y}"/><text x="${x+85}" y="${y-10}" text-anchor="middle" font-size="10">${esc(n.action)}</text>`;
  }
  for(const i of visible){const n=nodes[i],[x,y]=pos.get(i);let kind=n.kind;
    if(key!=='workforce'&&!processed.has(i)&&!['invalid','repeat'].includes(kind))kind='queued';
    const fill=kind==='invalid'?'#ffeded':kind==='repeat'?'#fff3d4':route.has(i)?'#edf8e9':'white';
    const border=i===current?'#2877c9':kind==='invalid'?'#c54848':kind==='repeat'?'#bd8a17':route.has(i)?'#358347':'#91a4ad';
    const label=kind==='repeat'?`repeat #${n.reference}`:kind==='exhausted'?`collapsed: ${trace.descendants[i]}`:kind;
    svg+=`<g data-node="${i}"><rect x="${x}" y="${y}" width="170" height="128" rx="8" fill="${fill}" stroke="${border}" stroke-width="${i===current?3:1.3}" ${kind==='queued'?'stroke-dasharray="4 3"':''}/><text x="${x+10}" y="${y+17}" font-size="10">#${i} · depth ${n.depth}</text>`;
    const lines=stateText(n).split('\n');for(let j=0;j<lines.length;j++)svg+=`<text x="${x+12}" y="${y+38+j*17}" font-size="${key==='workforce'?10:13}">${esc(lines[j])}</text>`;
    svg+=`<text x="${x+10}" y="${y+116}" font-size="10">${esc(label)}</text></g>`;
  }
  $('tree').setAttribute('viewBox',`0 0 ${width} ${height}`);$('tree').innerHTML=svg;
  $('step').max=count()-1;$('step').value=step;$('counter').textContent=`${step+1} / ${count()}`;
  $('png').disabled=step!==count()-1;
  $('previous').disabled=step===0;$('next').disabled=step===count()-1;
  $('explanation').textContent=solution?`Solution route: ${step} of ${trace.path.length-1} moves. The explored tree remains visible.`:key==='workforce'?`Decision ${Math.min(step+1,28)} / 28. Earlier decisions and tried alternatives remain visible; exhausted side branches are collapsed.`:`BFS step ${step+1}: node #${current}. Processed ${trace.events[step].visited} states; ${trace.events[step].frontier} remain queued.`;
  inspect(current);if(reset)fit();else scale();
}
async function load(){$('png').disabled=true;stop();const request=++loadId;key=$('problem').value;step=0;$('explanation').textContent='Loading recorded search…';
  try{const response=await fetch(`/search-tree-data/${key}.json`);if(!response.ok)throw Error('Could not load recorded search.');const data=await response.json();if(request!==loadId)return;trace=data;
    $('model').textContent=models[key];
    const s=trace.summary;$('statistics').textContent=key==='workforce'?`Verified run: ${s.visited} valid DFS calls, ${s.attempts} assignments tried, ${s.backtracks} backtracks. Solution: 28 assignments.`:`Verified run: ${s.visited} processed states, ${s.discovered} unique discovered states, ${s.attempts} attempted transitions. Shortest solution: ${trace.path.length-1} moves.`;
    $('scope').textContent=key==='workforce'?'This is the confirmed four-employee educational model. The display follows the returned branch and its attempted alternatives, not every internal DFS call.':'Green marks the final solution retrospectively. Invalid and repeated attempts are drawn for explanation, but never enqueued. Counts depend on the fixed action order and goal-check timing.';draw(true);
  }catch(e){$('explanation').textContent=e.message;}
}
// Rasterize the complete SVG, not the visible (possibly zoomed) viewport.
async function downloadPng(){
  stop();
  const button=$('png');button.disabled=true;button.textContent='Preparing PNG…';
  try{
    const svg=$('tree').cloneNode(true);
    svg.setAttribute('xmlns','http://www.w3.org/2000/svg');
    svg.setAttribute('width',width);svg.setAttribute('height',height);
    svg.removeAttribute('style');
    const style=document.createElementNS('http://www.w3.org/2000/svg','style');
    style.textContent='text{font-family:Arial,sans-serif;fill:#193644}.edge{fill:none;stroke:#9aaeb7;stroke-width:1.4}.edge.route{stroke:#358347;stroke-width:2.5}.edge.new{stroke:#2877c9;stroke-width:2.5}';
    svg.prepend(style);
    const image=new Image();
    image.src='data:image/svg+xml;charset=utf-8,'+encodeURIComponent(new XMLSerializer().serializeToString(svg));
    await image.decode();
    const factor=Math.min(2,8192/Math.max(width,height),Math.sqrt(24000000/(width*height)));
    const canvas=document.createElement('canvas');
    canvas.width=Math.ceil(width*factor);canvas.height=Math.ceil(height*factor);
    const context=canvas.getContext('2d');
    if(!context)throw Error('PNG export is unavailable in this browser.');
    context.fillStyle='white';context.fillRect(0,0,canvas.width,canvas.height);
    context.drawImage(image,0,0,canvas.width,canvas.height);
    const blob=await new Promise((resolve,reject)=>canvas.toBlob(value=>value?resolve(value):reject(Error('PNG export failed.')),'image/png'));
    const url=URL.createObjectURL(blob),link=document.createElement('a');
    link.href=url;link.download=names[key].replace('-report','')+'-final-tree.png';
    link.click();setTimeout(()=>URL.revokeObjectURL(url),10000);
  }catch(error){$('explanation').textContent=error.message;}
  finally{button.textContent='Download final tree PNG';button.disabled=step!==count()-1;}
}
$('png').onclick=downloadPng;
$('problem').addEventListener('change',load);$('mode').addEventListener('change',()=>{stop();step=0;draw(true);});
function move(delta){stop();step=Math.max(0,Math.min(count()-1,step+delta));draw();}
$('previous').onclick=()=>move(-1);$('next').onclick=()=>move(1);$('step').oninput=()=>{stop();step=Number($('step').value);draw();};
$('play').onclick=()=>{if(timer)return stop();if(step===count()-1)step=0;$('play').textContent='Pause';timer=setInterval(()=>{draw();if(step===count()-1)stop();else step++;},900);};
$('fit').onclick=fit;$('in').onclick=()=>{zoom=Math.min(3,zoom*1.4);scale();};$('out').onclick=()=>{zoom=Math.max(.04,zoom/1.4);scale();};
$('tree').onclick=e=>{const node=e.target.closest('[data-node]');if(node)inspect(Number(node.dataset.node));};
let drag=null;const view=$('viewport');view.addEventListener('pointerdown',e=>{if(e.button!==0)return;drag={x:e.clientX,y:e.clientY,left:view.scrollLeft,top:view.scrollTop};view.classList.add('dragging');});
window.addEventListener('pointermove',e=>{if(drag){view.scrollLeft=drag.left+drag.x-e.clientX;view.scrollTop=drag.top+drag.y-e.clientY;}});
window.addEventListener('pointerup',()=>{drag=null;view.classList.remove('dragging');});
load();
