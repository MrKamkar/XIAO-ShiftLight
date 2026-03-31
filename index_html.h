#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pl">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>XIAO ShiftLight — Dashboard</title>
  <style>
    :root {
      --bg-body: #06080c;
      --bg-card: #0d1117;
      --bg-card-alt: #111820;
      --bg-input: #161b22;
      --bg-hover: #1c2230;
      --border: #1e2530;
      --border-accent: #2d3748;
      --text-1: #e6edf3;
      --text-2: #8b949e;
      --text-3: #484f58;
      --red: #ff4757;
      --green: #2ed573;
      --blue: #1e90ff;
      --yellow: #ffa502;
      --cyan: #00d2d3;
      --orange: #ff6348;
      --purple: #a855f7;
      --radius: 14px;
      --radius-sm: 10px;
      --shadow: 0 8px 32px rgba(0,0,0,0.45);
      --transition: 0.25s cubic-bezier(0.4,0,0.2,1);
    }
    *, *::before, *::after { margin:0; padding:0; box-sizing:border-box; }

    body {
      background: var(--bg-body);
      color: var(--text-1);
      font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, Roboto, Helvetica, Arial, sans-serif;
      -webkit-font-smoothing: antialiased;
      min-height: 100vh;
      overflow-x: hidden;
    }

    /* HEADER */
    .header {
      position: sticky; top: 0; z-index: 50;
      display: flex; align-items: center; justify-content: space-between;
      padding: 12px 16px;
      background: rgba(6,8,12,0.82);
      backdrop-filter: blur(20px) saturate(180%);
      -webkit-backdrop-filter: blur(20px) saturate(180%);
      border-bottom: 1px solid var(--border);
    }
    .header-brand { display: flex; align-items: center; gap: 10px; }
    .header-logo {
      width: 34px; height: 34px;
      background: linear-gradient(135deg, var(--red), var(--orange));
      border-radius: 9px;
      display: flex; align-items: center; justify-content: center;
      font-weight: 900; font-size: 16px; color: #fff;
      box-shadow: 0 3px 12px rgba(255,71,87,0.35);
      flex-shrink: 0;
    }
    .header-text h1 { font-size: 15px; font-weight: 700; letter-spacing: -0.3px; line-height: 1.2; }
    .header-text span { font-size: 11px; color: var(--text-3); font-weight: 500; }
    .header-status {
      display: flex; align-items: center; gap: 6px;
      font-size: 11px; font-weight: 600; color: var(--text-3);
      letter-spacing: 0.5px; text-transform: uppercase;
    }
    .header-status .pulse {
      width: 8px; height: 8px; border-radius: 50%;
      background: var(--green);
      box-shadow: 0 0 8px rgba(46,213,115,0.6);
      animation: pulse 2s ease-in-out infinite;
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.5; transform: scale(0.8); }
    }

    /* MAIN */
    .main { max-width: 920px; margin: 0 auto; padding: 16px 12px 80px; }

    /* CARD */
    .card {
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      padding: 20px;
      margin-bottom: 14px;
      position: relative;
      overflow: hidden;
      transition: var(--transition);
    }
    .card::before {
      content: '';
      position: absolute; top: 0; left: 0; right: 0;
      height: 3px;
      background: var(--card-accent, linear-gradient(90deg, var(--red), var(--orange)));
      opacity: 0.8;
    }
    .card:hover { border-color: var(--border-accent); }
    .card-title {
      font-size: 11px; font-weight: 700;
      letter-spacing: 1.5px; text-transform: uppercase;
      color: var(--text-3);
      margin-bottom: 16px;
      display: flex; align-items: center; gap: 8px;
    }
    .card-title .icon { font-size: 14px; }

    /* COCKPIT */
    .cockpit { --card-accent: linear-gradient(90deg, var(--red), transparent); text-align: center; padding-bottom: 16px; }
    .cockpit-top { display: flex; justify-content: space-between; align-items: flex-start; margin-bottom: 0; }
    .cockpit-corner { display: flex; flex-direction: column; align-items: center; gap: 2px; }
    .cockpit-corner-label { font-size: 9px; font-weight: 700; letter-spacing: 1.2px; text-transform: uppercase; color: var(--text-3); }
    .cockpit-corner-value { font-family: 'Segoe UI', monospace; font-size: 22px; font-weight: 700; line-height: 1; }
    .cockpit-corner-unit { font-size: 11px; color: var(--text-3); font-weight: 500; }
    .corner-temp .cockpit-corner-value { color: var(--blue); }
    .corner-speed .cockpit-corner-value { color: var(--text-1); }
    #gaugeCanvas { width: 100%; max-width: 340px; height: auto !important; margin: 8px auto 0; display: block; }

    /* Mini gauges grid */
    .mini-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-top: 20px; }
    .mini-item {
      text-align: center; padding: 10px 4px 8px;
      background: var(--bg-card-alt);
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      transition: var(--transition);
    }
    .mini-item:hover { border-color: var(--border-accent); }
    .mini-value { font-family: 'Segoe UI', monospace; font-size: 16px; font-weight: 700; line-height: 1; margin-bottom: 4px; }
    .mini-bar { width: 100%; height: 4px; border-radius: 2px; margin: 6px auto 4px; }
    .mini-label { font-size: 9px; font-weight: 700; letter-spacing: 1.2px; text-transform: uppercase; color: var(--text-3); }

    /* System state */
    .sys-state {
      margin-top: 16px; padding: 10px 14px;
      border-radius: var(--radius-sm);
      font-size: 12px; font-weight: 700;
      letter-spacing: 0.8px; text-transform: uppercase;
      text-align: center; transition: var(--transition);
    }
    .state-normal { background: rgba(46,213,115,0.08); color: var(--green); border: 1px solid rgba(46,213,115,0.15); }
    .state-cold { background: rgba(30,144,255,0.08); color: var(--blue); border: 1px solid rgba(30,144,255,0.15); }
    .state-eco { background: rgba(46,213,115,0.08); color: var(--green); border: 1px solid rgba(46,213,115,0.15); }
    .state-offline { background: rgba(72,79,88,0.08); color: var(--text-3); border: 1px solid rgba(72,79,88,0.15); }

    /* G-FORCE */
    .gforce-card { --card-accent: linear-gradient(90deg, var(--green), transparent); }
    #telemetryCanvas { width: 100%; height: auto !important; display: block; border-radius: var(--radius-sm); background: #0a0d12; }

    /* TIMER */
    .timer-card { --card-accent: linear-gradient(90deg, var(--blue), transparent); }
    .timer-display {
      font-family: 'Segoe UI', monospace; font-size: 48px; font-weight: 700;
      text-align: center; margin: 8px 0 16px; line-height: 1;
      color: var(--text-1);
    }
    .timer-buttons { display: flex; gap: 10px; }
    .timer-status { text-align: center; margin-top: 12px; font-size: 13px; font-weight: 700; letter-spacing: 1px; text-transform: uppercase; height: 20px; }

    /* DATA LOGGER */
    .logger-card { --card-accent: linear-gradient(90deg, var(--purple), transparent); }
    .logger-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 14px; }
    .logger-status { display: flex; align-items: center; gap: 8px; font-size: 11px; font-weight: 600; color: var(--text-3); letter-spacing: 0.5px; text-transform: uppercase; }
    .rec-dot {
      width: 12px; height: 12px; border-radius: 50%;
      background: var(--bg-input); border: 2px solid var(--border-accent);
      transition: var(--transition);
    }
    .rec-dot.active {
      background: var(--red); border-color: var(--red);
      box-shadow: 0 0 12px rgba(255,71,87,0.5);
      animation: pulse 1s ease-in-out infinite;
    }
    .logger-buttons { display: flex; gap: 8px; }

    /* SETTINGS */
    .settings-card { --card-accent: linear-gradient(90deg, var(--yellow), transparent); }
    .setting-group { margin-bottom: 20px; }
    .setting-group:last-child { margin-bottom: 0; }
    .setting-label {
      display: flex; align-items: center; justify-content: space-between;
      font-size: 13px; font-weight: 600; color: var(--text-2); margin-bottom: 8px;
    }
    .setting-value { font-family: 'Segoe UI', monospace; font-weight: 700; color: var(--text-1); }

    /* Range slider */
    input[type=range] {
      -webkit-appearance: none; appearance: none;
      width: 100%; height: 6px;
      background: var(--bg-input); border-radius: 3px; outline: none;
      transition: var(--transition);
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none; appearance: none;
      width: 20px; height: 20px;
      background: linear-gradient(135deg, var(--red), var(--orange));
      border-radius: 50%; cursor: pointer;
      box-shadow: 0 2px 8px rgba(255,71,87,0.4);
      transition: var(--transition);
    }
    input[type=range]::-webkit-slider-thumb:hover { transform: scale(1.15); box-shadow: 0 3px 12px rgba(255,71,87,0.6); }
    input[type=range]::-moz-range-thumb {
      width: 20px; height: 20px;
      background: linear-gradient(135deg, var(--red), var(--orange));
      border-radius: 50%; border: none; cursor: pointer;
    }

    /* Toggle */
    .toggle-row {
      display: flex; align-items: center; justify-content: space-between;
      padding: 12px 0; border-top: 1px solid var(--border);
    }
    .toggle-row:first-of-type { border-top: none; }
    .toggle-label { font-size: 13px; font-weight: 600; color: var(--text-1); }
    .toggle-desc { font-size: 11px; color: var(--text-3); margin-top: 2px; }
    .switch { position: relative; display: inline-block; width: 44px; height: 24px; flex-shrink: 0; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider {
      position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
      background: var(--bg-input); border: 1px solid var(--border-accent);
      transition: var(--transition); border-radius: 24px;
    }
    .slider::before {
      position: absolute; content: "";
      height: 16px; width: 16px; left: 3px; bottom: 3px;
      background: var(--text-2); transition: var(--transition); border-radius: 50%;
    }
    input:checked + .slider { background: var(--green); border-color: var(--green); }
    input:checked + .slider::before { transform: translateX(20px); background: #fff; }

    /* BUTTONS */
    .btn {
      display: inline-flex; align-items: center; justify-content: center; gap: 6px;
      padding: 11px 18px;
      border: 1px solid var(--border-accent);
      background: var(--bg-input);
      color: var(--text-1);
      border-radius: var(--radius-sm);
      font-family: inherit; font-size: 13px; font-weight: 700;
      letter-spacing: 0.5px; text-transform: uppercase;
      cursor: pointer; transition: var(--transition); width: 100%;
    }
    .btn:hover { background: var(--bg-hover); transform: translateY(-1px); }
    .btn:active { transform: translateY(0); }
    .btn-primary {
      background: linear-gradient(135deg, var(--red), #e63946);
      border-color: transparent; color: #fff;
      box-shadow: 0 4px 16px rgba(255,71,87,0.3);
    }
    .btn-primary:hover { box-shadow: 0 6px 20px rgba(255,71,87,0.45); }
    .btn-launch {
      background: linear-gradient(135deg, #1a3a5f, #0d2847);
      border-color: rgba(30,144,255,0.3); color: var(--blue);
    }
    .btn-launch:hover {
      background: linear-gradient(135deg, var(--blue), #1565c0);
      color: #fff; box-shadow: 0 4px 16px rgba(30,144,255,0.3);
    }
    .btn-cancel { background: transparent; border-color: rgba(255,71,87,0.3); color: var(--red); }
    .btn-cancel:hover { background: rgba(255,71,87,0.08); }
    .btn-rec { background: transparent; border-color: rgba(255,71,87,0.3); color: var(--red); }
    .btn-rec:hover { background: rgba(255,71,87,0.1); }
    .btn-rec.active { background: var(--red); color: #fff; border-color: var(--red); box-shadow: 0 4px 12px rgba(255,71,87,0.35); }
    .btn-stop { background: transparent; border-color: var(--border-accent); color: var(--text-2); }
    .btn-stop:hover { background: var(--bg-hover); }
    .btn-export {
      background: linear-gradient(135deg, #1565C0, #0d47a1);
      border-color: transparent; color: #fff;
      margin-top: 10px; box-shadow: 0 4px 14px rgba(21,101,192,0.3);
    }
    .btn-export:hover { box-shadow: 0 6px 20px rgba(21,101,192,0.45); }
    .btn-save { margin-top: 8px; }
    #status { text-align: center; margin-top: 10px; font-size: 12px; font-weight: 700; letter-spacing: 0.5px; height: 18px; }

    /* COUNTDOWN */
    .countdown-overlay {
      display: none; position: fixed; inset: 0;
      background: rgba(6,8,12,0.92); z-index: 9999;
      align-items: center; justify-content: center;
      backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px);
      transition: opacity 0.3s ease-out;
    }
    .countdown-num {
      font-family: 'Segoe UI', monospace; font-size: 200px; font-weight: 900;
      color: var(--red);
      text-shadow: 0 0 80px rgba(255,71,87,0.6), 0 0 160px rgba(255,71,87,0.2);
      line-height: 1;
    }

    /* TWO COLUMN LAYOUT (tablet+) */
    .grid-2col { display: grid; grid-template-columns: 1fr; gap: 14px; }

    @media (min-width: 640px) {
      .main { padding: 20px 20px 80px; }
      .card { padding: 24px; }
      .grid-2col { grid-template-columns: 1fr 1fr; }
      .cockpit-corner-value { font-size: 26px; }
      #gaugeCanvas { max-width: 380px; }
      .countdown-num { font-size: 280px; }
      .mini-grid { gap: 12px; }
    }

    @media (min-width: 1024px) {
      .main { max-width: 1060px; padding: 28px 32px 80px; }
      .header { padding: 14px 32px; }
      .mini-grid { grid-template-columns: repeat(6, 1fr); }
    }

    ::-webkit-scrollbar { width: 6px; }
    ::-webkit-scrollbar-track { background: transparent; }
    ::-webkit-scrollbar-thumb { background: var(--border-accent); border-radius: 3px; }

    @keyframes fadeUp {
      from { opacity: 0; transform: translateY(12px); }
      to { opacity: 1; transform: translateY(0); }
    }
    .card { animation: fadeUp 0.5s ease-out both; }
    .card:nth-child(2) { animation-delay: 0.05s; }
    .card:nth-child(3) { animation-delay: 0.1s; }
    .card:nth-child(4) { animation-delay: 0.15s; }
    .card:nth-child(5) { animation-delay: 0.2s; }
  </style>
</head>
<body>

  <!-- COUNTDOWN OVERLAY -->
  <div class="countdown-overlay" id="countdownOverlay">
    <div class="countdown-num" id="countdownNum">5</div>
  </div>

  <!-- HEADER -->
  <header class="header">
    <div class="header-brand">
      <div class="header-logo">X</div>
      <div class="header-text">
        <h1>ShiftLight</h1>
        <span>Acceleration Telemetry</span>
      </div>
    </div>
    <div class="header-status">
      <div class="pulse"></div>
      LIVE
    </div>
  </header>

  <!-- MAIN CONTENT -->
  <div class="main">

    <!-- COCKPIT -->
    <div class="card cockpit" id="cockpit">
      <div class="cockpit-top">
        <div class="cockpit-corner corner-temp">
          <div class="cockpit-corner-label">Coolant</div>
          <div class="cockpit-corner-value"><span id="tempDisplay">--</span><span class="cockpit-corner-unit">°C</span></div>
        </div>
        <div class="cockpit-corner corner-speed">
          <div class="cockpit-corner-label">Speed</div>
          <div class="cockpit-corner-value"><span id="speedDisplay">0</span><span class="cockpit-corner-unit">km/h</span></div>
        </div>
      </div>

      <canvas id="gaugeCanvas" width="380" height="200"></canvas>

      <div class="mini-grid">
        <div class="mini-item">
          <div class="mini-value" style="color: var(--orange);"><span id="valLoad">--</span></div>
          <canvas id="loadCanvas" width="120" height="6" class="mini-bar"></canvas>
          <div class="mini-label">Load</div>
        </div>
        <div class="mini-item">
          <div class="mini-value" style="color: var(--green);"><span id="valVolt">--</span></div>
          <canvas id="voltCanvas" width="120" height="6" class="mini-bar"></canvas>
          <div class="mini-label">Battery</div>
        </div>
        <div class="mini-item">
          <div class="mini-value" style="color: var(--yellow);"><span id="valTps">--</span></div>
          <canvas id="tpsCanvas" width="120" height="6" class="mini-bar"></canvas>
          <div class="mini-label">Throttle</div>
        </div>
        <div class="mini-item">
          <div class="mini-value" style="color: var(--text-1);"><span id="valIat">--</span><span style="font-size:11px; color:var(--text-3);">°C</span></div>
          <div class="mini-label" style="margin-top:8px;">In-Temp</div>
        </div>
        <div class="mini-item">
          <div class="mini-value" style="color: var(--text-1);"><span id="valMap">--</span><span style="font-size:11px; color:var(--text-3);">kPa</span></div>
          <div class="mini-label" style="margin-top:8px;">Boost</div>
        </div>
        <div class="mini-item">
          <div class="mini-value" style="color: var(--blue);"><span id="valFuel">--</span></div>
          <canvas id="fuelCanvas" width="120" height="6" class="mini-bar"></canvas>
          <div class="mini-label">Fuel</div>
        </div>
      </div>

      <div id="sysState" class="sys-state state-offline">Link Offline...</div>
    </div>

    <!-- G-FORCE + TIMER -->
    <div class="grid-2col">
      <div class="card gforce-card">
        <div class="card-title"><span class="icon">💥</span> G-Force Telemetry</div>
        <canvas id="telemetryCanvas" width="460" height="140"></canvas>
      </div>
      <div class="card timer-card">
        <div class="card-title"><span class="icon">🏁</span> 0–100 km/h</div>
        <div class="timer-display" id="timeDisplay">0.00s</div>
        <div class="timer-buttons">
          <button class="btn btn-launch" style="flex:2;" onclick="startTimer()">Launch Sequence</button>
          <button class="btn btn-cancel" style="flex:1;" id="btnCancel" onclick="cancelTimer()">Cancel</button>
        </div>
        <div class="timer-status" id="timerStatus" style="color: var(--blue);"></div>
      </div>
    </div>

    <!-- LOGGER + SETTINGS -->
    <div class="grid-2col">
      <div class="card logger-card">
        <div class="card-title"><span class="icon">💾</span> Flash Data Logger</div>
        <div class="logger-header">
          <div class="logger-status">
            <div class="rec-dot" id="logIndicator"></div>
            <span>Recording</span>
          </div>
        </div>
        <div class="logger-buttons">
          <button class="btn btn-rec" id="btnLogStart" style="flex:1;" onclick="startLogging()">▶ REC</button>
          <button class="btn btn-stop" id="btnLogStop" style="flex:1;" onclick="stopLogging()">⏹ Stop</button>
        </div>
        <button class="btn btn-export" onclick="downloadLogs()">📥 Export CSV</button>
      </div>

      <div class="card settings-card">
        <div class="card-title"><span class="icon">⚙️</span> Vehicle Settings</div>
        <div class="setting-group">
          <div class="setting-label">
            Shift Light RPM
            <span class="setting-value" id="rpmVal">%RPM_LIMIT%</span>
          </div>
          <input type="range" id="rpmLimit" min="1000" max="10000" step="100" value="%RPM_LIMIT%" oninput="updateVal('rpmVal', this.value)">
        </div>
        <div class="setting-group">
          <div class="setting-label">
            LED Brightness
            <span class="setting-value" id="brtVal">%BRIGHTNESS%</span>
          </div>
          <input type="range" id="brightness" min="0" max="255" step="1" value="%BRIGHTNESS%" oninput="updateVal('brtVal', this.value)">
        </div>
        <div class="toggle-row">
          <div>
            <div class="toggle-label">Eco Drive Mode</div>
            <div class="toggle-desc">Delikatna jazda, niższe RPM</div>
          </div>
          <label class="switch">
            <input type="checkbox" id="ecoMode" %ECO_MODE_CHECKED%>
            <span class="slider"></span>
          </label>
        </div>
        <div class="toggle-row">
          <div>
            <div class="toggle-label">Buzzer Audio</div>
            <div class="toggle-desc">Dźwiękowe ostrzeżenia</div>
          </div>
          <label class="switch">
            <input type="checkbox" id="buzzerMode" %BUZZER_CHECKED%>
            <span class="slider"></span>
          </label>
        </div>
        <button class="btn btn-primary btn-save" onclick="saveSettings()">Apply Configuration</button>
        <div id="status"></div>
      </div>
    </div>
  </div>

  <script>
    /* COUNTDOWN */
    function showCountdownOverlay() {
      let overlay = document.getElementById('countdownOverlay');
      let num = document.getElementById('countdownNum');
      overlay.style.display = 'flex';
      overlay.style.opacity = '1';
      num.style.color = "#ff4757";
      num.style.textShadow = "0 0 80px rgba(255,71,87,0.6), 0 0 160px rgba(255,71,87,0.2)";
      num.innerText = "5";
      setTimeout(() => num.innerText = "4", 1000);
      setTimeout(() => num.innerText = "3", 2000);
      setTimeout(() => num.innerText = "2", 3000);
      setTimeout(() => num.innerText = "1", 4000);
      setTimeout(() => {
        num.innerText = "GO!";
        num.style.color = "#2ed573";
        num.style.textShadow = "0 0 80px rgba(46,213,115,0.6), 0 0 160px rgba(46,213,115,0.2)";
        setTimeout(() => {
          overlay.style.opacity = '0';
          setTimeout(() => overlay.style.display = 'none', 300);
        }, 800);
      }, 5000);
    }

    let t_rpm = 0, c_rpm = 0;
    let t_load = 0, c_load = 0;
    let t_volt = 12.0, c_volt = 12.0;
    let t_tps = 0, c_tps = 0;
    let t_fuel = 0, c_fuel = 0;

    function drawMountain(ctx, isAccel, colorStr, fillStr, w, baseLine, maxG, step, hist) {
      ctx.beginPath();
      for (let i = 0; i < hist.length; i++) {
        let g = hist[i];
        let val = isAccel ? (g > 0 ? g : 0) : (g < 0 ? Math.abs(g) : 0);
        let y = baseLine - (Math.min(val, maxG) / maxG) * (baseLine - 25);
        if (i === 0) ctx.moveTo(i * step, y);
        else ctx.lineTo(i * step, y);
      }
      ctx.lineTo(w, baseLine); ctx.lineTo(0, baseLine);
      ctx.fillStyle = fillStr; ctx.fill();
      ctx.beginPath();
      let isDrawing = false;
      for (let i = 0; i < hist.length; i++) {
        let g = hist[i];
        let val = isAccel ? g : -g;
        if (val > 0.01) {
          let y = baseLine - (Math.min(val, maxG) / maxG) * (baseLine - 25);
          if (!isDrawing) { ctx.moveTo(Math.max(0, i - 1) * step, baseLine); isDrawing = true; }
          ctx.lineTo(i * step, y);
        } else {
          if (isDrawing) { ctx.lineTo(i * step, baseLine); isDrawing = false; }
        }
      }
      ctx.lineWidth = 2; ctx.strokeStyle = colorStr; ctx.stroke();
    }

    let maxRpm = 8000;
    let gHistory = new Array(120).fill(0);
    let lastSpeedForG = 0, lastTimeForG = Date.now(), filteredG = 0;

    function updateVal(id, val) {
      document.getElementById(id).innerText = val;
      if (id === 'rpmVal') maxRpm = parseInt(val) + 500;
    }

    function saveSettings() {
      let r = document.getElementById('rpmLimit').value;
      let b = document.getElementById('brightness').value;
      let eco = document.getElementById('ecoMode').checked ? 1 : 0;
      let buz = document.getElementById('buzzerMode').checked ? 1 : 0;
      maxRpm = parseInt(r) + 500;
      let btn = document.querySelector('.btn-save');
      btn.innerText = "TX DATA...";
      fetch(`/save?rpm=${r}&bright=${b}&eco=${eco}&buzzer=${buz}`)
        .then(res => res.text())
        .then(() => {
          document.getElementById('status').innerText = "✓ CONFIG SYNC OK";
          document.getElementById('status').style.color = "#2ed573";
          btn.innerText = "APPLY CONFIGURATION";
          setTimeout(() => document.getElementById('status').innerText = "", 3000);
        }).catch(() => {
          document.getElementById('status').innerText = "✕ SYNC FAILED";
          document.getElementById('status').style.color = "#ff4757";
          btn.innerText = "APPLY CONFIGURATION";
        });
    }

    function startLogging() {
      fetch('/log/start', { method: 'POST' }).then(r => {
        if(!r.ok) alert('Błąd: Mems układu Flash zablokowany!');
      });
    }
    function stopLogging() { fetch('/log/stop', { method: 'POST' }); }
    function downloadLogs() { window.open('/log/download', '_blank'); }

    function cancelTimer() {
      fetch('/cancel0100').then(() => {
        document.getElementById('timerStatus').innerText = "ABORTED";
        document.getElementById('timerStatus').style.color = "#ff4757";
        document.getElementById('countdownOverlay').style.display = 'none';
      });
    }

    function startTimer() {
      fetch('/start0100').then(() => {
        document.getElementById('timerStatus').innerText = "ARMING...";
        document.getElementById('timerStatus').style.color = "#1e90ff";
        showCountdownOverlay();
      });
    }

    function calculateG(speedKmh) {
      let now = Date.now();
      let dt = (now - lastTimeForG) / 1000.0;
      if (dt >= 0.1) {
        let dv = (speedKmh - lastSpeedForG) / 3.6;
        let rawG = (dv / dt) / 9.81;
        filteredG = (rawG * 0.15) + (filteredG * 0.85);
        gHistory.push(filteredG);
        gHistory.shift();
        lastSpeedForG = speedKmh;
        lastTimeForG = now;
      }
    }

    function renderCanvas() {
      c_rpm += (t_rpm - c_rpm) * 0.15;
      c_load += (t_load - c_load) * 0.15;
      c_volt += (t_volt - c_volt) * 0.15;

      let cRpm = document.getElementById("gaugeCanvas");
      if (cRpm) {
        let ctx = cRpm.getContext("2d");
        let w = cRpm.width, h = cRpm.height;
        ctx.clearRect(0, 0, w, h);
        let cx = w / 2, cy = h - 20, r = h - 40;
        ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, 0);
        ctx.lineWidth = 18; ctx.strokeStyle = '#111820'; ctx.stroke();
        let fill = Math.min(c_rpm / maxRpm, 1.0);
        if (fill > 0) {
          let isEco = document.getElementById('ecoMode').checked;
          let tempText = document.getElementById('tempDisplay').innerText;
          let isCold = tempText !== "--" && parseInt(tempText) < 75;
          let baseLimit = parseInt(document.getElementById('rpmLimit').value);
          let activeLimit = baseLimit;
          if (isEco) activeLimit = baseLimit * 0.4;
          else if (isCold) activeLimit = baseLimit * 0.5;
          let strokeCol, shadowCol;
          if (c_rpm >= activeLimit) {
            strokeCol = (Math.floor(Date.now() / 80) % 2 === 0) ? '#ff4757' : '#1a0000';
            shadowCol = (Math.floor(Date.now() / 80) % 2 === 0) ? '#ff4757' : 'transparent';
          } else if (isEco) {
            if (c_rpm < 1500) strokeCol = '#2ed573';
            else if (c_rpm < 2000) strokeCol = '#ffa502';
            else strokeCol = '#ff4757';
            shadowCol = strokeCol;
          } else if (isCold) {
            strokeCol = '#1e90ff'; shadowCol = strokeCol;
          } else {
            let grad = ctx.createLinearGradient(0, cy, w, cy);
            grad.addColorStop(0.0, '#1e90ff');
            grad.addColorStop(0.35, '#2ed573');
            grad.addColorStop(0.65, '#ffa502');
            grad.addColorStop(0.85, '#ff6348');
            grad.addColorStop(1.0, '#ff4757');
            strokeCol = grad;
            if (fill < 0.35) shadowCol = '#1e90ff';
            else if (fill < 0.65) shadowCol = '#2ed573';
            else if (fill < 0.85) shadowCol = '#ffa502';
            else shadowCol = '#ff4757';
          }
          ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, Math.PI + (fill * Math.PI));
          ctx.lineWidth = 18; ctx.strokeStyle = strokeCol;
          ctx.shadowBlur = 15; ctx.shadowColor = shadowCol;
          ctx.stroke(); ctx.shadowBlur = 0;
        }
        ctx.fillStyle = '#e6edf3'; ctx.font = 'bold 52px "Segoe UI"'; ctx.textAlign = 'center';
        ctx.fillText(Math.round(c_rpm), cx, cy - 10);
        ctx.fillStyle = '#484f58'; ctx.font = 'bold 14px "Segoe UI"';
        ctx.fillText("RPM", cx, cy + 12);
      }

      c_tps += (t_tps - c_tps) * 0.15;
      c_fuel += (t_fuel - c_fuel) * 0.15;

      let cLoad = document.getElementById("loadCanvas");
      if (cLoad) {
        let ctx = cLoad.getContext("2d"), w = cLoad.width, h = cLoad.height;
        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = '#111820'; ctx.fillRect(0, 0, w, h);
        let lFill = Math.min(c_load / 100, 1.0);
        ctx.fillStyle = '#ff6348'; ctx.shadowBlur = 6; ctx.shadowColor = '#ff6348';
        ctx.fillRect(0, 0, w * lFill, h); ctx.shadowBlur = 0;
      }
      let cVolt = document.getElementById("voltCanvas");
      if (cVolt) {
        let ctx = cVolt.getContext("2d"), w = cVolt.width, h = cVolt.height;
        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = '#111820'; ctx.fillRect(0, 0, w, h);
        let vFill = Math.max(0, Math.min((c_volt - 10) / 5, 1.0));
        ctx.fillStyle = '#2ed573'; ctx.shadowBlur = 6; ctx.shadowColor = '#2ed573';
        ctx.fillRect(0, 0, w * vFill, h); ctx.shadowBlur = 0;
      }
      let cTps = document.getElementById("tpsCanvas");
      if (cTps) {
        let ctx = cTps.getContext("2d"), w = cTps.width, h = cTps.height;
        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = '#111820'; ctx.fillRect(0, 0, w, h);
        let tFill = Math.min(c_tps / 100, 1.0);
        ctx.fillStyle = '#ffa502'; ctx.shadowBlur = 6; ctx.shadowColor = '#ffa502';
        ctx.fillRect(0, 0, w * tFill, h); ctx.shadowBlur = 0;
      }
      let cFuel = document.getElementById("fuelCanvas");
      if (cFuel) {
        let ctx = cFuel.getContext("2d"), w = cFuel.width, h = cFuel.height;
        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = '#111820'; ctx.fillRect(0, 0, w, h);
        let fFill = Math.min(c_fuel / 100, 1.0);
        ctx.fillStyle = '#1e90ff'; ctx.shadowBlur = 6; ctx.shadowColor = '#1e90ff';
        ctx.fillRect(0, 0, w * fFill, h); ctx.shadowBlur = 0;
      }

      let cTel = document.getElementById("telemetryCanvas");
      if (cTel) {
        let ctx = cTel.getContext("2d");
        let w = cTel.width, h = cTel.height;
        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = "#0a0d12"; ctx.fillRect(0, 0, w, h);
        let baseLine = h - 5;
        let maxG = 1.5;
        let step = w / (gHistory.length - 1);
        ctx.strokeStyle = "#1e2530"; ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo(0, baseLine); ctx.lineTo(w, baseLine); ctx.stroke();
        drawMountain(ctx, true, "#2ed573", "rgba(46,213,115,0.2)", w, baseLine, maxG, step, gHistory);
        drawMountain(ctx, false, "#ff4757", "rgba(255,71,87,0.2)", w, baseLine, maxG, step, gHistory);
        let currentG = gHistory[gHistory.length - 1];
        ctx.fillStyle = currentG > 0.05 ? "#2ed573" : (currentG < -0.05 ? "#ff4757" : "#484f58");
        ctx.font = 'bold 13px "Segoe UI"';
        ctx.textAlign = 'right';
        let title = currentG > 0.05 ? "ACCEL " : (currentG < -0.05 ? "BRAKE " : "IDLE ");
        ctx.fillText(title + Math.abs(currentG).toFixed(2) + "G", w - 10, 20);
      }
      requestAnimationFrame(renderCanvas);
    }

    function pollData() {
      fetch('/status0100')
        .then(res => res.json())
        .then(data => {
          document.getElementById('speedDisplay').innerText = data.speed;
          document.getElementById('tempDisplay').innerText = data.temp === 999 ? "--" : data.temp;

          t_rpm = data.rpm !== undefined ? data.rpm : 0;
          t_load = data.load !== undefined ? data.load : Math.min(100, (t_rpm / 6000) * 100 + Math.random()*8);
          t_volt = data.volt !== undefined ? data.volt : (13.8 + (Math.random()*0.3 - 0.15));

          document.getElementById('valLoad').innerText = Math.round(t_load) + "%";
          document.getElementById('valVolt').innerText = t_volt.toFixed(1) + "V";
          if (data.tps !== undefined) {
            t_tps = data.tps; document.getElementById('valTps').innerText = Math.round(t_tps) + "%";
            document.getElementById('valIat').innerText = data.iat !== 999 ? data.iat : "--";
            document.getElementById('valMap').innerText = data.map;
            t_fuel = data.fuel; document.getElementById('valFuel').innerText = Math.round(t_fuel) + "%";
          }

          let ind = document.getElementById('logIndicator');
          let btnS = document.getElementById('btnLogStart');
          if (ind && data.log !== undefined) {
            if (data.log === true) {
              ind.classList.add('active');
              btnS.classList.add('active');
            } else {
              ind.classList.remove('active');
              btnS.classList.remove('active');
            }
          }

          calculateG(data.speed);

          let stateDiv = document.getElementById('sysState');
          let ecoEnabled = document.getElementById('ecoMode').checked;
          if (data.temp === 999) {
            stateDiv.className = "sys-state state-offline";
            stateDiv.innerText = "⏳ Oczekiwanie Na Dane ECU...";
          } else if (ecoEnabled) {
            stateDiv.className = "sys-state state-eco";
            stateDiv.innerText = "🌿 Eco Drive Aktywny";
          } else if (data.temp < 75) {
            stateDiv.className = "sys-state state-cold";
            let limit = Math.round(parseInt(document.getElementById('rpmLimit').value) * 0.5);
            stateDiv.innerText = "⚠ Silnik Zimny — Ochrona (" + limit + " RPM)";
          } else {
            stateDiv.className = "sys-state state-normal";
            stateDiv.innerText = "✓ System Online — Normal";
          }

          if (data.mode <= 1) {
            document.getElementById('timerStatus').innerText = "STANDBY";
            document.getElementById('timerStatus').style.color = "#1e90ff";
          }
          else if (data.mode === 2) {
            document.getElementById('timerStatus').innerText = "COUNTDOWN...";
            document.getElementById('timerStatus').style.color = "#1e90ff";
          }
          else if (data.mode === 3) {
            document.getElementById('timerStatus').innerText = ">>> LAUNCH READY <<<";
            document.getElementById('timerStatus').style.color = "#1e90ff";
            document.getElementById('timeDisplay').innerText = "0.00s";
          }
          else if (data.mode === 4) {
            document.getElementById('timerStatus').innerText = "TRACKING...";
            document.getElementById('timerStatus').style.color = "#1e90ff";
            document.getElementById('timeDisplay').innerText = (data.current_time / 1000.0).toFixed(2) + "s";
          }
          else if (data.mode === 5) {
            document.getElementById('timerStatus').innerText = "FINISHED";
            document.getElementById('timerStatus').style.color = "#2ed573";
            document.getElementById('timeDisplay').innerText = (data.time / 1000.0).toFixed(2) + "s";
          }
        }).catch(() => {});
    }

    maxRpm = parseInt(document.getElementById('rpmLimit').value) + 500;
    requestAnimationFrame(renderCanvas);
    setInterval(pollData, 100);
  </script>
</body>
</html>
)rawliteral";
