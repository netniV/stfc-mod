// STFC Gamestate Viewer
'use strict';

// ── Config ───────────────────────────────────────────────────────────────────
const GIST_FILES = {
  player:    'stfc_player.json',
  ships:     'stfc_ships.json',
  officers:  'stfc_officers.json',
  research:  'stfc_research.json',
  resources: 'stfc_resources.json',
  territory: 'stfc_territory.json',
  buildings: 'stfc_buildings.json',
};

let gistBase   = null;
const cache    = {};
let activeTable = null;

// ── DOM helpers ──────────────────────────────────────────────────────────────
function el(id) { return document.getElementById(id); }

function setStatus(t, ok) {
  const s = el('status');
  s.textContent = t;
  s.className = 'status ' + (ok === false ? 'error' : ok === true ? 'ok' : '');
}

function esc(s) {
  return String(s ?? '').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

// ── Format helpers ───────────────────────────────────────────────────────────

// Tokens that must remain fully uppercase in ship names
const SHIP_ALLCAPS = new Set([
  'U.S.S.', 'I.K.S.', 'I.R.W.', 'R.R.W.', 'I.S.S.',
  'USS', 'IKS', 'IRW', 'RRW', 'ISS', 'ECS', 'NX', 'NCC',
]);

function titleCase(str) {
  if (!str) return '';
  return str.split(/(\s+)/).map(token => {
    if (/^\s+$/.test(token)) return token;                              // keep whitespace as-is
    if (SHIP_ALLCAPS.has(token.toUpperCase())) return token.toUpperCase(); // e.g. U.S.S., ECS
    return token.toLowerCase().replace(/(?:^|-)([a-z])/g, m => m.toUpperCase()); // title-case + hyphen parts
  }).join('');
}

function fmtDuration(secs) {
  if (secs == null || secs < 0) return '—';
  secs = Math.floor(secs);
  const h = Math.floor(secs / 3600);
  const m = Math.floor((secs % 3600) / 60);
  const s = secs % 60;
  if (h) return `${h}h ${m}m ${s}s`;
  if (m) return `${m}m ${s}s`;
  return `${s}s`;
}

function fmtDateLocal(iso) {
  if (!iso) return '—';
  try { return new Date(iso).toLocaleString(); } catch (_) { return iso; }
}

// Decode HTML entities stored literally in game data (e.g. &apos; → ')
function decodeHTML(str) {
  const d = document.createElement('div');
  d.innerHTML = str || '';
  return d.textContent;
}

function cleanResource(name) {
  return (name || '').replace(/^Resource_/, '').replace(/_/g, ' ');
}

function fmtNum(n) {
  return Number(n ?? 0).toLocaleString();
}

// "Freyda (17)" → { name: "Freyda", level: 17 }
function parseSystem(s) {
  const m = String(s || '').match(/^(.+?)\s*\((\d+)\)\s*$/);
  return m ? { name: m[1].trim(), level: +m[2] } : { name: (s || '—'), level: null };
}

function shipStatus(s) {
  if (s.repair_active) {
    const dmgPct = (100 - (s.repair_progress ?? 0)).toFixed(1);
    return `Repairing — ${dmgPct}% damage, ${fmtDuration(s.repair_seconds_remaining)} remaining`;
  }
  if (s.is_mining) return 'Mining';
  return s.status || 'Docked';
}

const RANK_NAMES = ['Not Unlocked', 'Ensign', 'Lt. JG', 'Lieutenant', 'Lt. Cmdr', 'Commander'];
function rankLabel(r) { return RANK_NAMES[r] ?? `Rank ${r}`; }

// UTC hour → local time string, noting day shift if needed
function localHourStr(hourUTC) {
  const offsetH = -new Date().getTimezoneOffset() / 60;
  const rawLocal = hourUTC + offsetH;
  const localH   = ((rawLocal % 24) + 24) % 24;
  const dayDelta  = Math.floor(rawLocal / 24);
  const note = dayDelta === 1 ? ' (+1d)' : dayDelta === -1 ? ' (−1d)' : '';
  return `${String(Math.floor(localH)).padStart(2, '0')}:00${note}`;
}

function fmtWindow(w) {
  const utc   = `${w.weekday_name} ${String(w.start_hour_utc).padStart(2, '0')}:00 UTC`;
  const local = localHourStr(w.start_hour_utc);
  return `${utc} / ${local} (${w.duration_mins}m)`;
}

// ── URL param + localStorage ─────────────────────────────────────────────────
const LS_KEY = 'stfc_gist_base';

function gistBaseFromParams() {
  try {
    const p    = new URL(window.location.href).searchParams;
    const id   = p.get('gist_id') || p.get('gist');
    const user = p.get('username') || p.get('gist_user');
    if (id && user) return `https://gist.githubusercontent.com/${user}/${id}/raw`;
    if (id)         return `https://gist.githubusercontent.com/${id}/raw`;
  } catch (_) {}
  return null;
}

function saveGistBase(base) { try { localStorage.setItem(LS_KEY, base); } catch (_) {} }
function loadGistBase()     { try { return localStorage.getItem(LS_KEY) || null; } catch (_) { return null; } }

// ── Data loading ─────────────────────────────────────────────────────────────
async function fetchFile(filename) {
  if (cache[filename]) return cache[filename];
  const res = await fetch(`${gistBase}/${filename}`, { cache: 'no-store' });
  if (!res.ok) throw new Error(`HTTP ${res.status} for ${filename}`);
  const data = await res.json();
  cache[filename] = data;
  return data;
}

// ── Summary row builder ──────────────────────────────────────────────────────
function summaryRow(label, value) {
  const d = document.createElement('div');
  d.className = 'summary-item';
  d.innerHTML = `<strong>${esc(label)}:</strong> ${esc(String(value ?? '—'))}`;
  return d;
}

// ── Tabulator helpers ────────────────────────────────────────────────────────
function destroyTable() {
  if (activeTable) {
    try { activeTable.destroy(); } catch (_) {}
    activeTable = null;
  }
}

// Standard number cell formatter for Tabulator
function numFmt(cell) { return fmtNum(cell.getValue()); }

// Build a Tabulator table inside `container`, destroying any previous instance.
// Returns the Tabulator instance. Extra UI (buttons etc.) should be added to
// `container` BEFORE calling this; the table div is appended last.
function makeTable(container, data, columns, opts = {}) {
  destroyTable();
  container.innerHTML = '';
  const div = document.createElement('div');
  container.appendChild(div);
  activeTable = new Tabulator(div, {
    data,
    columns,
    layout: 'fitColumns',
    height: 480,
    ...opts,
  });
  return activeTable;
}

// ── Player summary ───────────────────────────────────────────────────────────
async function loadSummary() {
  setStatus('Loading…');

  // Fetch player and ships in parallel so drydocks can show ship names
  const [playerData, shipsData] = await Promise.all([
    fetchFile(GIST_FILES.player),
    fetchFile(GIST_FILES.ships).catch(() => ({ ships: [] })),
  ]);

  // Ship lookup map: id (string) → ship object
  const shipMap = {};
  for (const s of (shipsData.ships || [])) shipMap[String(s.id)] = s;

  const p      = playerData.player  || {};
  const st     = playerData.station || {};
  const al     = p.alliance || {};
  const shield = st.peace_shield || {};

  // Peace shield: compute current remaining from absolute expiry
  let shieldStr = 'Inactive';
  if (shield.expires_at) {
    const remaining = Math.floor((new Date(shield.expires_at).getTime() - Date.now()) / 1000);
    if (remaining > 0) {
      shieldStr = `Active — ${fmtDuration(remaining)} remaining (until ${fmtDateLocal(shield.expires_at)})`;
    } else {
      shieldStr = `Expired (was until ${fmtDateLocal(shield.expires_at)})`;
    }
  } else if (shield.active) {
    shieldStr = `Active — ${fmtDuration(shield.seconds_remaining)} remaining (at export time)`;
  }

  // Home system: split name and level
  const sys = parseSystem(st.home_system_name);
  const sysStr = sys.level != null ? `${sys.name} (Level ${sys.level})` : sys.name;

  const box = el('playerSummary');
  box.innerHTML = '';
  [
    ['Player',          p.name],
    ['Server',          p.server],
    ['Ops level',       p.ops_level],
    ['Power',           p.power != null ? fmtNum(p.power) : null],
    ['Syndicate level', p.syndicate_level],
    ['Alliance',        al.name ? `${al.name} [${al.tag}]` : '—'],
    ['Alliance power',  al.power != null ? fmtNum(al.power) : null],
    ['Home system',     sysStr],
    ['Peace shield',    shieldStr],
    ['Exported at',     fmtDateLocal(playerData.exported_at)],
  ].forEach(([k, v]) => box.appendChild(summaryRow(k, v)));

  // Drydocks with ship names + status
  const panel = el('drydocksPanel');
  panel.innerHTML = '';
  for (const dock of (playerData.drydocks || [])) {
    let content;
    if (!dock.ship_id) {
      content = 'Empty';
    } else {
      const ship     = shipMap[String(dock.ship_id)];
      const name     = ship ? titleCase(ship.name || '?') : `Ship #${dock.ship_id}`;
      const tier     = ship ? ` — T${ship.tier} Lv${ship.level}/${ship.tier_max_level}` : '';
      const location = dock.is_at_home_station ? ' ✓ home' : ' (deployed)';
      const status   = ship ? ` | ${shipStatus(ship)}` : '';
      content = `${name}${tier}${location}${status}`;
    }
    panel.appendChild(summaryRow(`Dock ${dock.letter}`, content));
  }

  setStatus(`Last export: ${fmtDateLocal(playerData.exported_at)}`, true);
}

// ── Ships tab ────────────────────────────────────────────────────────────────
async function renderShips() {
  destroyTable();
  const data  = await fetchFile(GIST_FILES.ships);
  const rows  = (data.ships || []).map(s => ({
    name:      titleCase(s.name || '?'),
    tier:      s.tier      ?? 0,
    level:     s.level     ?? 0,
    max_level: s.tier_max_level ?? 0,
    cargo:     s.cargo_capacity ?? 0,
    status:    shipStatus(s),
  }));

  makeTable(el('tabContent'), rows, [
    { title: 'Name',      field: 'name',   sorter: 'string', headerFilter: 'input', formatter: 'plaintext' },
    { title: 'Tier',      field: 'tier',   sorter: 'number', width: 65,  hozAlign: 'right' },
    { title: 'Level',     field: 'level',  sorter: 'number', width: 100, hozAlign: 'right',
      formatter: cell => {
        const r = cell.getRow().getData();
        return `${r.level} / ${r.max_level}`;
      }
    },
    { title: 'Cargo',     field: 'cargo',  sorter: 'number', width: 100, hozAlign: 'right', formatter: numFmt },
    { title: 'Status',    field: 'status', sorter: 'string', headerFilter: 'input', formatter: 'plaintext' },
  ], { initialSort: [{ column: 'tier', dir: 'desc' }] });
}

// ── Officers tab ─────────────────────────────────────────────────────────────
async function renderOfficers() {
  destroyTable();
  const data = await fetchFile(GIST_FILES.officers);
  const rows = (data.officers || [])
    .filter(o => o.name && (o.rank ?? 0) <= 5)   // drop phantoms: missing name or rank > 5
    .map(o => ({
      name:      decodeHTML(o.name),
      faction:   o.faction  || '—',
      rarity:    o.rarity   || '—',
      level:     o.level    ?? 0,
      rank:      o.rank     ?? 0,
      rank_name: rankLabel(o.rank ?? 0),
      shards:    o.shards   ?? 0,
    }));

  const locked = rows.filter(r => r.rank === 0).length;
  let showLocked = false;

  const container = el('tabContent');
  container.innerHTML = '';

  // Toggle for not-yet-unlocked officers (rank 0)
  const bar = document.createElement('div');
  bar.className = 'table-toolbar';
  const toggleBtn = document.createElement('button');
  toggleBtn.textContent = `Show Not Unlocked (${locked})`;
  const note = document.createElement('span');
  note.className = 'status';
  note.textContent = 'Rank 0 officers are hidden by default';
  bar.appendChild(toggleBtn);
  bar.appendChild(note);
  container.appendChild(bar);

  const tableDiv = document.createElement('div');
  container.appendChild(tableDiv);

  activeTable = new Tabulator(tableDiv, {
    data: rows,
    initialFilter: [{ field: 'rank', type: '!=', value: 0 }],
    columns: [
      { title: 'Name',    field: 'name',      sorter: 'string', headerFilter: 'input', formatter: 'plaintext', minWidth: 160 },
      { title: 'Faction', field: 'faction',   sorter: 'string', headerFilter: 'input', formatter: 'plaintext', width: 110 },
      { title: 'Rarity',  field: 'rarity',    sorter: 'string', headerFilter: 'input', formatter: 'plaintext', width: 100 },
      { title: 'Level',   field: 'level',     sorter: 'number', width: 75,  hozAlign: 'right' },
      { title: 'Rank',    field: 'rank',      sorter: 'number', width: 130, hozAlign: 'right',
        formatter: cell => {
          const r = cell.getRow().getData();
          return `${r.rank_name} (${r.rank})`;
        }
      },
      { title: 'Shards (lifetime)', field: 'shards', sorter: 'number', width: 130, hozAlign: 'right', formatter: numFmt },
    ],
    layout: 'fitColumns',
    height: 450,
    initialSort: [{ column: 'name', dir: 'asc' }],
  });

  toggleBtn.addEventListener('click', () => {
    showLocked = !showLocked;
    toggleBtn.textContent = showLocked
      ? `Hide Not Unlocked (${locked})`
      : `Show Not Unlocked (${locked})`;
    if (showLocked) {
      activeTable.clearFilter();   // clears programmatic filter, keeps header filters
    } else {
      activeTable.setFilter('rank', '!=', 0);
    }
  });
}

// ── Research tab ─────────────────────────────────────────────────────────────
async function renderResearch() {
  destroyTable();
  const data = await fetchFile(GIST_FILES.research);
  const rows = (data.research || []).map(r => ({
    name:  r.name || r.key || '?',
    tree:  r.tree  || '—',
    level: r.level ?? 0,
  }));

  makeTable(el('tabContent'), rows, [
    { title: 'Research',  field: 'name',  sorter: 'string', headerFilter: 'input', formatter: 'plaintext' },
    { title: 'Tree',      field: 'tree',  sorter: 'string', headerFilter: 'input', formatter: 'plaintext', width: 160 },
    { title: 'Level',     field: 'level', sorter: 'number', width: 80, hozAlign: 'right' },
  ], { initialSort: [{ column: 'tree', dir: 'asc' }, { column: 'name', dir: 'asc' }] });
}

// ── Resources tab ────────────────────────────────────────────────────────────
async function renderResources() {
  destroyTable();
  const data = await fetchFile(GIST_FILES.resources);
  const rows = (data.resources || [])
    .filter(r => { const n = cleanResource(r.name || '').trim(); return n && n !== '?'; })
    .map(r => ({
      name:   cleanResource(r.name),
      rarity: r.rarity || '—',
      amount: r.amount ?? 0,
    }));

  makeTable(el('tabContent'), rows, [
    { title: 'Resource', field: 'name',   sorter: 'string', headerFilter: 'input', formatter: 'plaintext' },
    { title: 'Rarity',   field: 'rarity', sorter: 'string', headerFilter: 'input', formatter: 'plaintext', width: 90 },
    { title: 'Amount',   field: 'amount', sorter: 'number', hozAlign: 'right', formatter: numFmt },
  ], { initialSort: [{ column: 'amount', dir: 'desc' }] });
}

// ── Territory tab ────────────────────────────────────────────────────────────
async function renderTerritory() {
  destroyTable();
  const data  = await fetchFile(GIST_FILES.territory);
  const terr  = data.territory || {};
  const held  = terr.held || [];

  const container = el('tabContent');
  container.innerHTML = '';

  const slotBar = document.createElement('div');
  slotBar.className = 'summary-item';
  slotBar.innerHTML = `<strong>Slots:</strong> ${esc(terr.used_slots ?? 0)} / ${esc(terr.total_slots ?? 0)} used`;
  container.appendChild(slotBar);

  if (!held.length) {
    const empty = document.createElement('div');
    empty.className = 'summary-item';
    empty.textContent = 'No territory currently held.';
    container.appendChild(empty);
    return;
  }

  const rows = held.map(h => {
    const sys = parseSystem(h.name || h.territory_id || '');
    const windows = (h.takeover_windows || []).map(w => fmtWindow(w)).join('\n');
    return {
      name:      sys.name,
      sys_level: sys.level,
      tier:      h.tier  ?? '—',
      state:     h.state || '—',
      windows,
    };
  });

  const tableDiv = document.createElement('div');
  container.appendChild(tableDiv);

  activeTable = new Tabulator(tableDiv, {
    data: rows,
    columns: [
      { title: 'System',    field: 'name',      sorter: 'string', headerFilter: 'input', formatter: 'plaintext' },
      { title: 'Sys Lv',   field: 'sys_level',  sorter: 'number', width: 75, hozAlign: 'right' },
      { title: 'Tier',     field: 'tier',        sorter: 'number', width: 65, hozAlign: 'right' },
      { title: 'State',    field: 'state',       sorter: 'string', headerFilter: 'input', formatter: 'plaintext', width: 110 },
      { title: 'Takeover Windows (UTC / Local)', field: 'windows',
        formatter: 'textarea', sorter: 'string', headerFilter: 'input' },
    ],
    layout: 'fitColumns',
    height: 450,
    initialSort: [{ column: 'name', dir: 'asc' }],
  });
}

// ── Buildings tab ────────────────────────────────────────────────────────────
async function renderBuildings() {
  destroyTable();
  const data = await fetchFile(GIST_FILES.buildings);
  const rows = (data.buildings || [])
    .filter(b => b.name && b.name !== '?' && (b.id ?? 0) < 1000)  // id >= 1000 = alliance starbase modules
    .map(b => ({
      name:  b.name,
      level: b.level ?? 0,
    }));

  makeTable(el('tabContent'), rows, [
    { title: 'Building', field: 'name',  sorter: 'string', headerFilter: 'input', formatter: 'plaintext' },
    { title: 'Level',    field: 'level', sorter: 'number', width: 90, hozAlign: 'right' },
  ], { initialSort: [{ column: 'level', dir: 'desc' }] });
}

// ── Tab routing ──────────────────────────────────────────────────────────────
const TAB_RENDERERS = {
  ships:     renderShips,
  officers:  renderOfficers,
  research:  renderResearch,
  resources: renderResources,
  territory: renderTerritory,
  buildings: renderBuildings,
};

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

// ── Init ─────────────────────────────────────────────────────────────────────
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

  const base = gistBaseFromParams() || loadGistBase();
  if (base) {
    el('rawBase').value = base;
    loadAll(base);
  } else {
    setStatus('Open the viewer URL from your stfc_manifest.json to auto-load your data here.');
  }
});
