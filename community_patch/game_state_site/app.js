// STFC Gamestate Viewer — loads from per-file gist exports
'use strict';

const GIST_FILES = {
  player:    'stfc_player.json',
  ships:     'stfc_ships.json',
  officers:  'stfc_officers.json',
  research:  'stfc_research.json',
  resources: 'stfc_resources.json',
  territory: 'stfc_territory.json',
};

let gistBase = null;         // set after URL param detection
const cache = {};            // filename -> parsed JSON

function el(id) { return document.getElementById(id); }
function setStatus(t, ok) {
  const s = el('status');
  s.textContent = t;
  s.className = 'status ' + (ok === false ? 'error' : ok === true ? 'ok' : '');
}

// ── URL param detection + localStorage persistence ───────────────────────────
const LS_KEY = 'stfc_gist_base';

function gistBaseFromParams() {
  try {
    const p = new URL(window.location.href).searchParams;
    const id   = p.get('gist_id') || p.get('gist');
    const user = p.get('username') || p.get('gist_user');
    if (id && user) return `https://gist.githubusercontent.com/${user}/${id}/raw`;
    if (id)         return `https://gist.githubusercontent.com/${id}/raw`;
  } catch (_) {}
  return null;
}

function saveGistBase(base) {
  try { localStorage.setItem(LS_KEY, base); } catch (_) {}
}

function loadGistBase() {
  try { return localStorage.getItem(LS_KEY) || null; } catch (_) { return null; }
}

// ── Data loading ─────────────────────────────────────────────────────────────
async function fetchFile(filename) {
  if (cache[filename]) return cache[filename];
  const res = await fetch(`${gistBase}/${filename}`, { cache: 'no-store' });
  if (!res.ok) throw new Error(`HTTP ${res.status} for ${filename}`);
  const data = await res.json();
  cache[filename] = data;
  return data;
}

// ── Render helpers ───────────────────────────────────────────────────────────
function row(label, value) {
  const d = document.createElement('div');
  d.className = 'summary-item';
  d.innerHTML = `<strong>${esc(label)}:</strong> ${esc(String(value ?? '-'))}`;
  return d;
}
function esc(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
function tag(t, cls, text) {
  const e = document.createElement(t);
  if (cls) e.className = cls;
  if (text != null) e.textContent = text;
  return e;
}

// ── Player summary ───────────────────────────────────────────────────────────
async function loadSummary() {
  setStatus('Loading player data…');
  const data = await fetchFile(GIST_FILES.player);
  const p  = data.player  || {};
  const st = data.station || {};
  const al = p.alliance   || {};
  const shield = st.peace_shield || {};
  const box = el('playerSummary');
  box.innerHTML = '';
  [
    ['Player',          p.name],
    ['Server',          p.server],
    ['Ops level',       p.ops_level],
    ['Power',           p.power?.toLocaleString()],
    ['Syndicate level', p.syndicate_level],
    ['Alliance',        al.name ? `${al.name} [${al.tag}]` : null],
    ['Alliance power',  al.power?.toLocaleString()],
    ['Home system',     st.home_system_name],
    ['Peace shield',    shield.active ? `Active — ${shield.seconds_remaining}s remaining` : 'Inactive'],
    ['Exported at',     data.exported_at],
  ].forEach(([k, v]) => box.appendChild(row(k, v)));

  el('drydocksPanel').innerHTML = '';
  (data.drydocks || []).forEach(d => {
    const s = d.ship_id
      ? `Dock ${d.letter}: ship ${d.ship_id}${d.is_at_home_station ? ' ✓ home' : ''}${d.is_damaged ? ' (damaged)' : ''}`
      : `Dock ${d.letter}: empty`;
    el('drydocksPanel').appendChild(row(d.letter, s.replace(/^Dock .:/, '')));
  });

  setStatus(`Last export: ${data.exported_at}`, true);
}

// ── Tab renderers ────────────────────────────────────────────────────────────
async function renderShips() {
  const data  = await fetchFile(GIST_FILES.ships);
  const ships = data.ships || [];
  const c = el('tabContent');
  c.innerHTML = '';
  if (!ships.length) { c.textContent = 'No ships'; return; }
  ships.forEach(s => {
    const d = tag('div', 'summary-item');
    d.innerHTML = `<strong>${esc(s.name || '?')}</strong> — Tier ${esc(s.tier)} Level ${esc(s.level)}/${esc(s.tier_max_level)}`;
    c.appendChild(d);
  });
}

async function renderOfficers() {
  const data     = await fetchFile(GIST_FILES.officers);
  const officers = data.officers || [];
  const c = el('tabContent');
  c.innerHTML = '';
  if (!officers.length) { c.textContent = 'No officers'; return; }
  officers.forEach(o => {
    const d = tag('div', 'summary-item');
    d.innerHTML = `<strong>${esc(o.name || '?')}</strong> — ${esc(o.faction || '')} ${esc(o.rarity || '')} | Lvl ${esc(o.level)} Rank ${esc(o.rank)} | ${esc(o.shards ?? 0)} shards`;
    c.appendChild(d);
  });
}

async function renderResearch() {
  const data     = await fetchFile(GIST_FILES.research);
  const research = data.research || [];
  const c = el('tabContent');
  c.innerHTML = '';
  if (!research.length) { c.textContent = 'No research'; return; }
  research.forEach(r => {
    const d = tag('div', 'summary-item');
    d.innerHTML = `<strong>${esc(r.name || r.key || '?')}</strong> — Tree: ${esc(r.tree || '-')} | Lvl ${esc(r.level ?? '-')}`;
    c.appendChild(d);
  });
}

async function renderResources() {
  const data      = await fetchFile(GIST_FILES.resources);
  const resources = data.resources || [];
  const c = el('tabContent');
  c.innerHTML = '';
  if (!resources.length) { c.textContent = 'No resources'; return; }
  resources.forEach(r => {
    const d = tag('div', 'summary-item');
    d.innerHTML = `<strong>${esc(r.name || r.key || '?')}</strong> — ${esc(Number(r.amount).toLocaleString())}`;
    c.appendChild(d);
  });
}

async function renderTerritory() {
  const data  = await fetchFile(GIST_FILES.territory);
  const terr  = data.territory || {};
  const held  = terr.held || [];
  const c = el('tabContent');
  c.innerHTML = '';
  const hdr = tag('div', 'summary-item');
  hdr.innerHTML = `<strong>Slots:</strong> ${esc(terr.used_slots ?? 0)} / ${esc(terr.total_slots ?? 0)} used`;
  c.appendChild(hdr);
  held.forEach(h => {
    const windows = (h.takeover_windows || []).map(w =>
      `${w.weekday_name} ${String(w.start_hour_utc).padStart(2,'0')}:00 UTC (${w.duration_mins}m)`
    ).join(', ');
    const d = tag('div', 'summary-item');
    d.innerHTML = `<strong>${esc(h.name || h.territory_id)}</strong> — Tier ${esc(h.tier)} | ${esc(h.state)} | Takeover: ${esc(windows || '-')}`;
    c.appendChild(d);
  });
}

const TAB_RENDERERS = {
  ships:     renderShips,
  officers:  renderOfficers,
  research:  renderResearch,
  resources: renderResources,
  territory: renderTerritory,
};

// ── Init ─────────────────────────────────────────────────────────────────────
async function switchTab(name) {
  document.querySelectorAll('.tabs button').forEach(b => b.classList.remove('active'));
  const btn = document.querySelector(`.tabs button[data-tab="${name}"]`);
  if (btn) btn.classList.add('active');
  el('tabContent').textContent = 'Loading…';
  try {
    await TAB_RENDERERS[name]();
  } catch (err) {
    el('tabContent').textContent = 'Error: ' + err.message;
    setStatus('Error loading ' + name, false);
  }
}

async function loadAll(base) {
  gistBase = base;
  saveGistBase(base);
  Object.keys(cache).forEach(k => delete cache[k]);
  try {
    await loadSummary();
    await switchTab('ships');
  } catch (err) {
    setStatus('Error: ' + err.message, false);
  }
}

window.addEventListener('DOMContentLoaded', () => {
  document.querySelectorAll('.tabs button').forEach(btn => {
    btn.addEventListener('click', () => {
      if (!gistBase) { setStatus('Load a gist first', false); return; }
      switchTab(btn.dataset.tab);
    });
  });

  el('loadBtn').addEventListener('click', () => {
    const v = el('rawBase').value.trim();
    if (!v) { setStatus('Enter a raw gist base URL', false); return; }
    loadAll(v);
  });

  // Auto-load: URL params take priority, then localStorage (remembered from last visit)
  const base = gistBaseFromParams() || loadGistBase();
  if (base) {
    el('rawBase').value = base;
    loadAll(base);
  } else {
    setStatus('Open the viewer URL from your stfc_manifest.json to auto-load your data here.');
  }
});
