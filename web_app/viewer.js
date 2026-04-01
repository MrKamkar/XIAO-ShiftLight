// ============================================================
// DATA STORE
// ============================================================
const COLORS = [
  { line: '#ff4757', bg: 'rgba(255,71,87,0.12)' },
  { line: '#1e90ff', bg: 'rgba(30,144,255,0.12)' },
  { line: '#2ed573', bg: 'rgba(46,213,115,0.12)' },
  { line: '#ffa502', bg: 'rgba(255,165,2,0.12)' },
  { line: '#a855f7', bg: 'rgba(168,85,247,0.12)' },
  { line: '#00d2d3', bg: 'rgba(0,210,211,0.12)' },
  { line: '#ff6b81', bg: 'rgba(255,107,129,0.12)' },
  { line: '#eccc68', bg: 'rgba(236,204,104,0.12)' },
];

const FIELDS = [
  { key: 'rpm', label: 'RPM', unit: '' },
  { key: 'speed', label: 'Speed', unit: 'km/h' },
  { key: 'temp', label: 'Coolant', unit: '°C' },
  { key: 'load', label: 'Load', unit: '%' },
  { key: 'volt', label: 'Voltage', unit: 'V' },
  { key: 'iat', label: 'IAT', unit: '°C' },
  { key: 'tps', label: 'Throttle', unit: '%' },
  { key: 'map', label: 'MAP', unit: 'kPa' },
  { key: 'fuel', label: 'Fuel', unit: '%' },
  { key: 'gforce', label: 'G-Force', unit: 'g' },
];

let sessions = [];
let charts = {};

// ============================================================
// CSV PARSING
// ============================================================
function parseCSV(text, filename) {
  const lines = text.trim().split('\n');
  if (lines.length < 2) throw new Error('File has no data rows');

  // Auto-detect header line
  let headerLine = lines[0];
  let dataStart = 1;
  // If first line doesn't look like a header, try to infer
  if (!headerLine.toLowerCase().includes('rpm') && !headerLine.toLowerCase().includes('time')) {
    // assume no header, use default
    headerLine = 'Time(ms),RPM,Speed(km/h),Temp(C),Load(%),Volt(V),IAT(C),TPS(%),MAP(kPa),Fuel(%),G(g)';
    dataStart = 0;
  }

  const rows = [];
  for (let i = dataStart; i < lines.length; i++) {
    const parts = lines[i].split(',');
    if (parts.length < 11) continue;
    const row = {
      time: parseInt(parts[0]) || 0,
      rpm: parseInt(parts[1]) || 0,
      speed: parseInt(parts[2]) || 0,
      temp: parseInt(parts[3]) || 0,
      load: parseInt(parts[4]) || 0,
      volt: parseFloat(parts[5]) || 0,
      iat: parseInt(parts[6]) || 0,
      tps: parseInt(parts[7]) || 0,
      map: parseInt(parts[8]) || 0,
      fuel: parseInt(parts[9]) || 0,
      gforce: parseFloat(parts[10]) || 0,
    };
    rows.push(row);
  }

  if (rows.length === 0) throw new Error('No valid data rows found');

  // Normalize time to start from 0 in seconds
  const t0 = rows[0].time;
  rows.forEach(r => r.timeS = (r.time - t0) / 1000);

  return {
    name: filename.replace(/\.csv$/i, ''),
    rows: rows,
    color: COLORS[sessions.length % COLORS.length],
    id: Date.now() + '_' + Math.random().toString(36).substr(2, 6)
  };
}

function computeStats(session) {
  const r = session.rows;
  const duration = r[r.length - 1].timeS - r[0].timeS;

  const stat = (key) => {
    const vals = r.map(row => row[key]);
    const max = Math.max(...vals);
    const min = Math.min(...vals);
    const avg = vals.reduce((a, b) => a + b, 0) / vals.length;
    return { max, min, avg };
  };

  return {
    duration,
    points: r.length,
    rpm: stat('rpm'),
    speed: stat('speed'),
    temp: stat('temp'),
    load: stat('load'),
    volt: stat('volt'),
    iat: stat('iat'),
    tps: stat('tps'),
    map: stat('map'),
    fuel: stat('fuel'),
    gforce: stat('gforce'),
  };
}

// ============================================================
// FILE HANDLING
// ============================================================
function handleFiles(files) {
  for (const file of files) {
    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const session = parseCSV(e.target.result, file.name);
        session.stats = computeStats(session);
        sessions.push(session);
        refreshUI();
      } catch (err) {
        alert(`Error parsing "${file.name}": ${err.message}`);
      }
    };
    reader.readAsText(file);
  }
}

// Drop zone events
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');

dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
dropZone.addEventListener('drop', (e) => {
  e.preventDefault();
  dropZone.classList.remove('dragover');
  handleFiles(e.dataTransfer.files);
});
fileInput.addEventListener('change', (e) => {
  handleFiles(e.target.files);
  e.target.value = '';
});

// ============================================================
// UI REFRESH
// ============================================================
function refreshUI() {
  renderSessionChips();

  if (sessions.length === 0) {
    document.getElementById('contentArea').classList.add('d-none');
    document.getElementById('btnClearAll').classList.add('d-none');
    document.getElementById('btnResetZoom').classList.add('d-none');
    dropZone.style.display = 'block';
    return;
  }

  document.getElementById('contentArea').classList.remove('d-none');
  document.getElementById('btnClearAll').classList.remove('d-none');
  document.getElementById('btnResetZoom').classList.remove('d-none');
  // Keep drop zone but smaller when sessions loaded
  dropZone.style.padding = '24px 20px';
  dropZone.querySelector('.drop-icon').style.fontSize = '28px';

  renderStats();
  renderAllCharts();
  renderCompareTable();
  renderRawDataSelector();
  renderRawData();
}

function renderSessionChips() {
  const bar = document.getElementById('sessionsBar');
  bar.innerHTML = sessions.map((s, i) => `
    <div class="session-chip" style="border-color: ${s.color.line}33;">
      <span class="dot" style="background: ${s.color.line}; box-shadow: 0 0 8px ${s.color.bg};"></span>
      <span>${s.name}</span>
      <span class="points-count">${s.rows.length} pts • ${s.stats.duration >= 60 ? Math.floor(s.stats.duration / 60) + 'm ' + Math.floor(s.stats.duration % 60) + 's' : s.stats.duration.toFixed(1) + 's'}</span>
      <button class="remove" onclick="removeSession('${s.id}')" title="Remove session">✕</button>
    </div>
  `).join('');
}

// Make removeSession accessible from HTML clicks
window.removeSession = function (id) {
  sessions = sessions.filter(s => s.id !== id);
  // Reassign colors
  sessions.forEach((s, i) => s.color = COLORS[i % COLORS.length]);
  refreshUI();
};

window.clearAllSessions = function () {
  if (!confirm('Remove all sessions?')) return;
  sessions = [];
  refreshUI();
  dropZone.style.padding = '';
  const icon = dropZone.querySelector('.drop-icon');
  if (icon) icon.style.fontSize = '';
};

// ============================================================
// STATS CARDS (latest session)
// ============================================================
function renderStats() {
  const grid = document.getElementById('statsGrid');
  const last = sessions[sessions.length - 1];
  const s = last.stats;

  const cards = [
    { label: 'Peak RPM', value: s.rpm.max, unit: '', color: 'var(--accent-red)', detail: `Avg: ${Math.round(s.rpm.avg)}` },
    { label: 'Top Speed', value: s.speed.max, unit: 'km/h', color: 'var(--accent-blue)', detail: `Avg: ${Math.round(s.speed.avg)} km/h` },
    { label: 'Max G-Force', value: s.gforce.max.toFixed(2), unit: 'g', color: 'var(--accent-green)', detail: `Min: ${s.gforce.min.toFixed(2)}g` },
    { label: 'Max Load', value: s.load.max, unit: '%', color: 'var(--accent-orange)', detail: `Avg: ${Math.round(s.load.avg)}%` },
    { label: 'Peak Temp', value: s.temp.max, unit: '°C', color: 'var(--accent-yellow)', detail: `Min: ${s.temp.min}°C` },
    { label: 'Voltage', value: s.volt.avg.toFixed(1), unit: 'V', color: 'var(--accent-cyan)', detail: `${s.volt.min.toFixed(1)}–${s.volt.max.toFixed(1)}V` },
    { label: 'Duration', value: s.duration >= 60 ? `${Math.floor(s.duration / 60)}m ${Math.floor(s.duration % 60)}` : s.duration.toFixed(1), unit: 's', color: 'var(--accent-purple)', detail: `${s.points} data points` },
    { label: 'Max Throttle', value: s.tps.max, unit: '%', color: 'var(--accent-pink)', detail: `Avg: ${Math.round(s.tps.avg)}%` },
  ];

  grid.innerHTML = cards.map(c => `
    <div class="stat-card fade-in" style="--stat-color: ${c.color};">
      <div class="stat-label">${c.label}</div>
      <div class="stat-value">${c.value}<span class="stat-unit">${c.unit}</span></div>
      <div class="stat-detail">${c.detail}</div>
    </div>
  `).join('');
}

// ============================================================
// CHART RENDERING
// ============================================================
const chartDefaults = {
  responsive: true,
  maintainAspectRatio: false,
  animation: { duration: 600, easing: 'easeOutQuart' },
  interaction: { mode: 'index', intersect: false },
  plugins: {
    zoom: {
      pan: { enabled: true, mode: 'x', threshold: 10 },
      zoom: {
        wheel: { enabled: true },
        pinch: { enabled: true },
        drag: { enabled: false },
        mode: 'x'
      },
      limits: {
        x: { min: 'original', max: 'original' }
      }
    },
    legend: {
      display: true,
      labels: {
        color: '#8b949e',
        font: { family: 'Inter', size: 11, weight: 600 },
        boxWidth: 12, boxHeight: 3,
        padding: 16,
      }
    },
    tooltip: {
      backgroundColor: 'rgba(13,17,23,0.95)',
      borderColor: '#30363d',
      borderWidth: 1,
      titleFont: { family: 'Inter', size: 12, weight: 700 },
      bodyFont: { family: 'JetBrains Mono', size: 12 },
      padding: 12,
      cornerRadius: 8,
      displayColors: true,
      callbacks: {
        title: function (context) {
          if (!context || !context.length) return '';
          return 'Time: ' + formatTime(context[0].parsed.x);
        }
      }
    }
  },
  scales: {
    x: {
      type: 'linear',
      ticks: {
        color: '#484f58',
        font: { family: 'JetBrains Mono', size: 10 },
        maxTicksLimit: 15,
        callback: function (value) { return formatTime(value); }
      },
      grid: { color: 'rgba(33,38,45,0.5)', lineWidth: 1 },
      title: { display: true, text: 'Time', color: '#484f58', font: { family: 'Inter', size: 11, weight: 600 } }
    },
    y: {
      ticks: { color: '#484f58', font: { family: 'JetBrains Mono', size: 10 } },
      grid: { color: 'rgba(33,38,45,0.5)', lineWidth: 1 },
    }
  }
};

// Smart time formatting — seconds for short sessions, m:ss for longer ones
const TIME_THRESHOLD = 60; // 1 minute — standard in pro telemetry (MoTeC, AiM)

function useMinuteFormat() {
  if (sessions.length === 0) return false;
  return sessions.some(s => s.stats.duration >= TIME_THRESHOLD);
}

function formatTime(seconds) {
  if (!useMinuteFormat()) {
    return seconds.toFixed(1) + 's';
  }
  const m = Math.floor(seconds / 60);
  const s = Math.floor(seconds % 60);
  return m + ':' + String(s).padStart(2, '0');
}

function makeDatasets(fieldKey, unit) {
  const minuteMode = useMinuteFormat();
  return sessions.map(s => {
    // Downsample if more than 600 points for performance
    let rows;
    if (s.rows.length > 600) {
      const step = Math.ceil(s.rows.length / 600);
      rows = s.rows.filter((_, i) => i % step === 0);
    } else {
      rows = s.rows;
    }
    const data = rows.map(r => ({
      x: r.timeS,
      y: r[fieldKey]
    }));
    return {
      label: s.name,
      data: data,
      borderColor: s.color.line,
      backgroundColor: s.color.bg,
      borderWidth: 2,
      pointRadius: 0,
      pointHitRadius: 6,
      tension: 0.3,
      fill: true,
    };
  });
}

function deepMerge(target, source) {
  const out = { ...target };
  for (const key of Object.keys(source)) {
    if (source[key] && typeof source[key] === 'object' && !Array.isArray(source[key])) {
      out[key] = deepMerge(out[key] || {}, source[key]);
    } else {
      out[key] = source[key];
    }
  }
  return out;
}

function createChart(canvasId, fieldKey, unit, extraOpts) {
  const canvas = document.getElementById(canvasId);
  if (!canvas) return;

  if (charts[canvasId]) {
    charts[canvasId].destroy();
  }

  let opts = deepMerge({}, chartDefaults);
  if (unit) {
    opts.scales.y.title = { display: true, text: unit, color: '#484f58', font: { family: 'Inter', size: 11, weight: 600 } };
  }
  if (sessions.length <= 1) opts.plugins.legend.display = false;
  if (extraOpts) opts = deepMerge(opts, extraOpts);

  charts[canvasId] = new Chart(canvas, {
    type: 'line',
    data: { datasets: makeDatasets(fieldKey, unit) },
    options: opts,
  });

  // Add reset zoom button to chart card
  const card = canvas.closest('.chart-card');
  if (card) {
    const header = card.querySelector('.chart-header');
    if (header && !header.querySelector('.btn-reset-zoom')) {
      const controls = document.createElement('div');
      controls.className = 'chart-controls-box';
      controls.innerHTML = `
        <span class="zoom-hint">🔍 Scroll / Pinch</span>
        <button class="btn-reset-zoom" onclick="resetChartZoom('${canvasId}')">↺ Reset Zoom</button>
      `;
      header.appendChild(controls);
    }
  }
}

window.resetChartZoom = function (chartId) {
  if (charts[chartId]) {
    charts[chartId].resetZoom();
  }
};

window.resetAllZoom = function () {
  Object.values(charts).forEach(c => { if (c.resetZoom) c.resetZoom(); });
};

function renderAllCharts() {
  // Overview charts
  createOverviewRpmSpeed();
  createChart('chartOverviewGForce', 'gforce', 'g');

  // Detail charts
  createChart('chartRPM', 'rpm', 'RPM');
  createChart('chartSpeed', 'speed', 'km/h');
  createChart('chartTemp', 'temp', '°C');
  createChart('chartVolt', 'volt', 'V');
  createChart('chartLoad', 'load', '%');
  createChart('chartTPS', 'tps', '%');
  createChart('chartIAT', 'iat', '°C');
  createChart('chartMAP', 'map', 'kPa');
  createChart('chartFuel', 'fuel', '%');
  createChart('chartGForce', 'gforce', 'g');
}

function createOverviewRpmSpeed() {
  const canvas = document.getElementById('chartOverviewRpmSpeed');
  if (!canvas) return;
  if (charts['chartOverviewRpmSpeed']) charts['chartOverviewRpmSpeed'].destroy();

  const s = sessions[sessions.length - 1]; // Show latest session
  const step = Math.max(1, Math.ceil(s.rows.length / 500));
  const sampled = s.rows.filter((_, i) => i % step === 0);
  const mm = useMinuteFormat();

  charts['chartOverviewRpmSpeed'] = new Chart(canvas, {
    type: 'line',
    data: {
      datasets: [
        {
          label: 'RPM',
          data: sampled.map(r => ({ x: r.timeS, y: r.rpm })),
          borderColor: '#ff4757',
          backgroundColor: 'rgba(255,71,87,0.08)',
          borderWidth: 2, pointRadius: 0, tension: 0.3, fill: true,
          yAxisID: 'y',
        },
        {
          label: 'Speed (km/h)',
          data: sampled.map(r => ({ x: r.timeS, y: r.speed })),
          borderColor: '#1e90ff',
          backgroundColor: 'rgba(30,144,255,0.08)',
          borderWidth: 2, pointRadius: 0, tension: 0.3, fill: true,
          yAxisID: 'y2',
        }
      ]
    },
    options: {
      ...chartDefaults,
      plugins: { ...chartDefaults.plugins, legend: { ...chartDefaults.plugins.legend, display: true } },
      scales: {
        ...chartDefaults.scales,
        y: {
          ...chartDefaults.scales.y,
          position: 'left',
          title: { display: true, text: 'RPM', color: '#ff4757', font: { family: 'Inter', size: 11, weight: 600 } },
        },
        y2: {
          ...chartDefaults.scales.y,
          position: 'right',
          title: { display: true, text: 'km/h', color: '#1e90ff', font: { family: 'Inter', size: 11, weight: 600 } },
          grid: { drawOnChartArea: false },
        }
      }
    }
  });

  // Add reset zoom button to overview chart card
  const card = canvas.closest('.chart-card');
  if (card) {
    const header = card.querySelector('.chart-header');
    if (header && !header.querySelector('.btn-reset-zoom')) {
      const controls = document.createElement('div');
      controls.className = 'chart-controls-box';
      controls.innerHTML = `
        <span class="zoom-hint">🔍 Scroll / Pinch</span>
        <button class="btn-reset-zoom" onclick="resetChartZoom('chartOverviewRpmSpeed')">↺ Reset Zoom</button>
      `;
      header.appendChild(controls);
    }
  }
}

// ============================================================
// COMPARE TABLE
// ============================================================
function renderCompareTable() {
  const container = document.getElementById('compareContent');

  if (sessions.length < 2) {
    container.innerHTML = `
      <div class="empty-state p-60-20">
        <div class="icon">🏁</div>
        <h3>Upload at least 2 sessions</h3>
        <p>Comparison requires multiple CSV files to show differences between recording sessions.</p>
      </div>`;
    return;
  }

  const metrics = [
    { label: 'Duration', fn: s => s.stats.duration >= 60 ? `${Math.floor(s.stats.duration / 60)}m ${Math.floor(s.stats.duration % 60)}s` : `${s.stats.duration.toFixed(1)}s` },
    { label: 'Data Points', fn: s => s.stats.points },
    { label: 'Peak RPM', fn: s => s.stats.rpm.max, compare: 'max' },
    { label: 'Avg RPM', fn: s => Math.round(s.stats.rpm.avg), compare: 'max' },
    { label: 'Top Speed', fn: s => s.stats.speed.max + ' km/h', numFn: s => s.stats.speed.max, compare: 'max' },
    { label: 'Avg Speed', fn: s => Math.round(s.stats.speed.avg) + ' km/h', numFn: s => s.stats.speed.avg, compare: 'max' },
    { label: 'Max G-Force', fn: s => s.stats.gforce.max.toFixed(2) + 'g', numFn: s => s.stats.gforce.max, compare: 'max' },
    { label: 'Min G-Force (Brake)', fn: s => s.stats.gforce.min.toFixed(2) + 'g', numFn: s => s.stats.gforce.min, compare: 'min' },
    { label: 'Max Load', fn: s => s.stats.load.max + '%', numFn: s => s.stats.load.max, compare: 'max' },
    { label: 'Peak Temp', fn: s => s.stats.temp.max + '°C', numFn: s => s.stats.temp.max },
    { label: 'Avg Voltage', fn: s => s.stats.volt.avg.toFixed(1) + 'V', numFn: s => s.stats.volt.avg },
    { label: 'Max Throttle', fn: s => s.stats.tps.max + '%', numFn: s => s.stats.tps.max, compare: 'max' },
    { label: 'Max MAP', fn: s => s.stats.map.max + ' kPa', numFn: s => s.stats.map.max, compare: 'max' },
  ];

  let html = `
    <div class="compare-section fade-in">
      <div class="section-title">Session Comparison</div>
      <div class="table-responsive">
      <table class="compare-table">
        <tr>
          <th>Metric</th>
          ${sessions.map(s => `<th style="color:${s.color.line};">${s.name}</th>`).join('')}
        </tr>`;

  metrics.forEach(m => {
    const numFn = m.numFn || (s => parseFloat(m.fn(s)));
    const vals = sessions.map(s => numFn(s));

    let bestIdx = -1, worstIdx = -1;
    if (m.compare === 'max') {
      bestIdx = vals.indexOf(Math.max(...vals));
      worstIdx = vals.indexOf(Math.min(...vals));
    } else if (m.compare === 'min') {
      bestIdx = vals.indexOf(Math.min(...vals));
      worstIdx = vals.indexOf(Math.max(...vals));
    }

    html += `<tr><td class="metric-name">${m.label}</td>`;
    sessions.forEach((s, i) => {
      let cls = '';
      if (sessions.length > 1 && m.compare) {
        if (i === bestIdx) cls = 'best-value';
        else if (i === worstIdx) cls = 'worst-value';
      }
      html += `<td class="${cls}">${m.fn(s)}</td>`;
    });
    html += '</tr>';
  });

  html += '</table></div></div>';
  container.innerHTML = html;
}

// ============================================================
// RAW DATA TABLE
// ============================================================
window.renderRawDataSelector = function () {
  const sel = document.getElementById('rawSessionSelect');
  sel.innerHTML = sessions.map((s, i) => `<option value="${i}">${s.name} (${s.rows.length} rows)</option>`).join('');
};

window.renderRawData = function () {
  const sel = document.getElementById('rawSessionSelect');
  const idx = parseInt(sel.value) || 0;
  const s = sessions[idx];
  if (!s) return;

  const container = document.getElementById('rawDataContainer');
  const headers = ['Time(ms)', 'Time(s)', 'RPM', 'Speed', 'Temp', 'Load', 'Volt', 'IAT', 'TPS', 'MAP', 'Fuel', 'G'];

  let html = `<div class="data-table-wrap fade-in"><table class="data-table"><tr>${headers.map(h => `<th>${h}</th>`).join('')}</tr>`;

  // Show max 2000 rows
  const maxRows = Math.min(s.rows.length, 2000);
  for (let i = 0; i < maxRows; i++) {
    const r = s.rows[i];
    html += `<tr>
      <td>${r.time}</td>
      <td>${r.timeS.toFixed(2)}</td>
      <td>${r.rpm}</td>
      <td>${r.speed}</td>
      <td>${r.temp}</td>
      <td>${r.load}</td>
      <td>${r.volt.toFixed(2)}</td>
      <td>${r.iat}</td>
      <td>${r.tps}</td>
      <td>${r.map}</td>
      <td>${r.fuel}</td>
      <td>${r.gforce.toFixed(2)}</td>
    </tr>`;
  }

  if (s.rows.length > 2000) {
    html += `<tr><td colspan="12" class="table-notice">
      Showing first 2000 of ${s.rows.length} rows. Export to CSV for full data.
    </td></tr>`;
  }

  html += '</table></div>';
  container.innerHTML = html;
};

// ============================================================
// TABS
// ============================================================
document.querySelectorAll('.tab').forEach(tab => {
  tab.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
    tab.classList.add('active');
    document.getElementById('panel-' + tab.dataset.tab).classList.add('active');

    // Re-render charts when switching to chart tabs (fixes canvas sizing)
    if (tab.dataset.tab === 'charts' || tab.dataset.tab === 'overview') {
      setTimeout(() => renderAllCharts(), 50);
    }
  });
});

// ============================================================
// DEMO DATA (optional — for testing)
// ============================================================
function generateDemoData() {
  let csv = 'Time(ms),RPM,Speed(km/h),Temp(C),Load(%),Volt(V),IAT(C),TPS(%),MAP(kPa),Fuel(%),G(g)\n';
  let t = 0, rpm = 850, speed = 0, temp = 45, load = 15, volt = 14.1, iat = 28, tps = 0, map = 101, fuel = 75, g = 0;
  let phase = 0; // 0=idle warmup, 1=accel, 2=cruise, 3=brake, 4=accel2

  for (let i = 0; i < 600; i++) {
    t += 80 + Math.round(Math.random() * 40);

    if (i < 60) { // warmup
      rpm = 850 + Math.random() * 100;
      speed = 0;
      temp = 45 + i * 0.5 + Math.random();
      load = 12 + Math.random() * 5;
      tps = 0;
    } else if (i < 180) { // acceleration
      let progress = (i - 60) / 120;
      rpm = 2000 + progress * 5500 + Math.random() * 300;
      if (rpm > 6800) rpm = 3500 + Math.random() * 500; // shift
      speed = progress * 120;
      temp = 72 + Math.random() * 3;
      load = 75 + Math.random() * 20;
      tps = 80 + Math.random() * 20;
      map = 180 + Math.random() * 40;
    } else if (i < 300) { // cruise
      rpm = 3000 + Math.random() * 200;
      speed = 110 + Math.random() * 10;
      temp = 88 + Math.random() * 3;
      load = 30 + Math.random() * 10;
      tps = 25 + Math.random() * 10;
      map = 101 + Math.random() * 10;
    } else if (i < 380) { // braking
      let progress = (i - 300) / 80;
      rpm = 3000 - progress * 2200;
      speed = 115 - progress * 100;
      temp = 90 + Math.random() * 2;
      load = 5 + Math.random() * 5;
      tps = 0;
      map = 30 + Math.random() * 10;
    } else { // second accel
      let progress = (i - 380) / 220;
      rpm = 1500 + progress * 5000 + Math.random() * 400;
      if (rpm > 6500) rpm = 3200 + Math.random() * 400;
      speed = 15 + progress * 130;
      temp = 90 + Math.random() * 2;
      load = 70 + Math.random() * 25;
      tps = 90 + Math.random() * 10;
      map = 170 + Math.random() * 50;
    }

    // G-Force calculated from speed delta
    if (i > 0) {
      g = (speed - parseFloat(csv.split('\n').slice(-1)[0].split(',')[2])) / 3.6 / (0.1) / 9.81;
      g = g * 0.3 + (parseFloat(csv.split('\n').slice(-1)[0].split(',')[10]) || 0) * 0.7;
    }

    volt = 13.8 + Math.random() * 0.4 - (load > 60 ? 0.3 : 0);
    iat = 28 + Math.random() * 5;
    fuel = 75 - i * 0.02;

    csv += `${t},${Math.round(rpm)},${Math.round(speed)},${Math.round(temp)},${Math.round(load)},${volt.toFixed(2)},${Math.round(iat)},${Math.round(tps)},${Math.round(map)},${Math.round(fuel)},${g.toFixed(2)}\n`;
  }
  return csv;
}

// Quick-load demo button (activated by URL param ?demo=true)
if (new URLSearchParams(window.location.search).get('demo') === 'true') {
  try {
    const demo1 = parseCSV(generateDemoData(), 'Session_Track_Day.csv');
    demo1.stats = computeStats(demo1);
    sessions.push(demo1);

    const demo2 = parseCSV(generateDemoData(), 'Session_Street.csv');
    demo2.stats = computeStats(demo2);
    sessions.push(demo2);

    refreshUI();
  } catch (e) { console.error('Demo generation failed:', e); }
}
