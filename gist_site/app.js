const exampleBase = 'https://gist.githubusercontent.com/DrCord/3d1bd2f24dc5ab3cc79beb9e8387b5e0/raw';

// Paths to try for an on-disk config (game install) or example config in the repo.
const repoExamplePath = '../example_community_patch_settings.toml';
const gameConfigFile = 'file:///C:/Games/Star%20Trek%20Fleet%20Command/Star%20Trek%20Fleet%20Command/default/game/community_patch_settings.toml';

function el(id){return document.getElementById(id)}
function setStatus(t){el('status').textContent = t}

async function loadFromBase(base){
  setStatus('Fetching full snapshot...')
  try{
    const fullUrl = `${base}/stfc_gamestate_full.json`;
    const deltaUrl = `${base}/stfc_gamestate_delta.json`;
    const [fullRes, deltaRes] = await Promise.all([fetch(fullUrl), fetch(deltaUrl)]);
    if(!fullRes.ok) throw new Error('Full snapshot fetch failed: '+fullRes.status)
    const full = await fullRes.json();
    let delta = [];
    if(deltaRes.ok) delta = await deltaRes.json();

    renderFull(full, delta);
    setStatus('Loaded OK')
  }catch(err){
    console.error(err);
    setStatus('Error: '+err.message)
  }
}

function renderFull(full, delta){
  const player = full.player || {};
  const summary = el('playerSummary');
  summary.innerHTML = '';
  const items = [];
  items.push(["Name", player.name || '-'])
  items.push(["Server", player.server || '-'])
  items.push(["Power", player.power || '-'])
  items.push(["Ops level", player.ops_level || '-'])
  items.push(["Alliance", (player.alliance||{}).name || '-'])
  items.push(["Buildings", (full.buildings||[]).length])
  items.push(["Ships", (full.ships||[]).length])
  items.push(["Officers", (full.officers||[]).length])
  items.push(["Research entries", (full.research||[]).length])

  for(const [k,v] of items){
    const d = document.createElement('div');
    d.className = 'summary-item';
    d.innerHTML = `<strong>${k}:</strong> ${v}`;
    summary.appendChild(d);
  }

  // store payload for tab rendering
  window.__stfc_data = { full, delta };
  renderTab('ships');
}

function renderTab(name){
  const data = window.__stfc_data;
  const container = el('tabContent');
  if(!data){ container.textContent = 'No data loaded'; return }
  const full = data.full;
  container.innerHTML = '';
  if(name==='ships'){
    const list = full.ships || [];
    if(!list.length){ container.textContent = 'No ships found'; return }
    const ul = document.createElement('div');
    ul.className = 'list';
    list.slice(0,200).forEach(s=>{
      const row = document.createElement('div');
      row.className = 'summary-item';
      row.innerHTML = `<strong>${s.name || s.type || 'Ship'}</strong> - Level: ${s.level||'-'} - ID: ${s.id||s.station_id||'-'}`;
      ul.appendChild(row);
    })
    container.appendChild(ul);
  }else if(name==='officers'){
    const list = full.officers || [];
    if(!list.length){ container.textContent = 'No officers found'; return }
    const ul = document.createElement('div');
    list.slice(0,200).forEach(o=>{
      const row = document.createElement('div');
      row.className = 'summary-item';
      row.innerHTML = `<strong>${o.name||o.template||'Officer'}</strong> - Rank: ${o.rank||o.level||'-'}`;
      ul.appendChild(row);
    })
    container.appendChild(ul);
  }else if(name==='research'){
    const list = full.research || [];
    if(!list.length){ container.textContent = 'No research entries'; return }
    const ul = document.createElement('div');
    list.forEach(r=>{
      const row = document.createElement('div');
      row.className = 'summary-item';
      row.innerHTML = `<strong>${r.key||r.name||'Research'}</strong> - Level: ${r.level||r.progress||'-'}`;
      ul.appendChild(row);
    })
    container.appendChild(ul);
  }else if(name==='resources'){
    const list = full.resources || [];
    if(!list.length){ container.textContent = 'No resources entries'; return }
    const ul = document.createElement('div');
    list.forEach(r=>{
      const row = document.createElement('div');
      row.className = 'summary-item';
      row.innerHTML = `<strong>${r.key||r.name||'Resource'}</strong> - Amount: ${r.amount||r.value||'-'}`;
      ul.appendChild(row);
    })
    container.appendChild(ul);
  }
}

function init(){
  el('exampleBtn').addEventListener('click',()=>{ el('rawBase').value = exampleBase })
  el('loadBtn').addEventListener('click', ()=>{
    const v = el('rawBase').value.trim();
    if(!v){ setStatus('Please enter a raw gist base URL or use the example'); return }
    loadFromBase(v);
  })
  document.querySelectorAll('.tabs button').forEach(btn=>{
    btn.addEventListener('click', e=>{
      document.querySelectorAll('.tabs button').forEach(b=>b.classList.remove('active'));
      btn.classList.add('active');
      renderTab(btn.dataset.tab);
    })
  })
  // Attempt to automatically detect gist id from several locations (URL param, repo example TOML,
  // or local game config). If found, auto-fill and auto-load.
  detectGistBase().then(base=>{
    if(base){
      el('rawBase').value = base;
      setStatus('Detected gist from config — auto-loading');
      loadFromBase(base);
    }else{
      // quick auto-fill example but don't auto-load
      el('rawBase').value = '';
    }
  }).catch(err=>{
    console.warn('detectGistBase error', err);
    el('rawBase').value = '';
  })
}

window.addEventListener('DOMContentLoaded', init);

// Try several strategies to find gist id and username in TOML config files or URL params.
async function detectGistBase(){
  // 1) Check URL query parameters: ?gist_id=... or ?gist=...
  try{
    const url = new URL(window.location.href);
    const q1 = url.searchParams.get('gist_id') || url.searchParams.get('gist');
    const quser = url.searchParams.get('gist_user') || url.searchParams.get('username');
    if(q1){
      if(quser) return `https://gist.githubusercontent.com/${quser}/${q1}/raw`;
      return `https://gist.githubusercontent.com/${q1}/raw`;
    }
  }catch(e){ /* ignore */ }

  // 2) Try to fetch example TOML shipped with the repo (relative path)
  const tryPaths = [repoExamplePath, gameConfigFile];
  for(const p of tryPaths){
    try{
      const res = await fetch(p, {cache: 'no-store'});
      if(!res.ok) continue;
      const txt = await res.text();
      const gist_id = parseTomlValue(txt, /gist_id\s*=\s*['"]([^'\"]+)['"]/i);
      const username = parseTomlValue(txt, /username\s*=\s*['"]([^'\"]+)['"]/i);
      if(gist_id){
        if(username) return `https://gist.githubusercontent.com/${username}/${gist_id}/raw`;
        return `https://gist.githubusercontent.com/${gist_id}/raw`;
      }
    }catch(err){
      // fetch may fail on file:// or cross-origin — ignore and continue
      // console.debug('failed to fetch', p, err)
    }
  }

  return null;
}

function parseTomlValue(text, re){
  const m = text.match(re);
  return m ? m[1].trim() : null;
}


