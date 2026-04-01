// ==========================================
// XIAO ShiftLight - Web Bluetooth Dashboard
// ==========================================

// Globalne zmienne BLE
let bleDevice = null;
let bleServer = null;
let txCharacteristic = null;
let rxCharacteristic = null;

const SERVICE_UUID = "4fafc201-1fb5-459e-8bcc-c5c9c331914b";
const TX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const RX_CHAR_UUID = "beb5483e-36e2-4688-b7f5-ea07361b26a8";

// Buforowanie i reasemblacja danych (obsługa fragmentacji MTU)
let receiveBuffer = "";
let binBuffer = new Uint8Array(30);
let binIndex = 0;
const textDecoder = new TextDecoder("utf-8");
const textEncoder = new TextEncoder();

// Zmienne zegarów i animacji
let t_rpm = 0, c_rpm = 0;
let t_load = 0, c_load = 0;
let t_volt = 12.0, c_volt = 12.0;
let t_tps = 0, c_tps = 0;
let t_fuel = 0, c_fuel = 100;
let maxRpm = 8000;
let gHistory = new Array(120).fill(0);
let lastSpeedForG = 0, lastTimeForG = Date.now(), filteredG = 0;

// UI Elements Cache
const UI = {
  btnConnect: null,
  dotStatus: null,
  sysState: null,
  speed: null,
  temp: null,
  valLoad: null,
  valVolt: null,
  valTps: null,
  valIat: null,
  valMap: null,
  valFuel: null,
  logInd: null,
  btnLog: null,
  timerStatus: null,
  timerDisp: null,
  rpmLimit: null,
  brightness: null,
  ecoMode: null,
  buzzerMode: null,
  gaugeStatic: null,
  gaugeDynamic: null,
  gaugeLoad: null,
  gaugeVolt: null,
  gaugeTps: null,
  gaugeFuel: null,
  countdownOverlay: null,
  countdownNum: null,
  btnConnectText: null
};

let lastData = {};
let rpmGradient = null;
let cachedRpmLimit = 6000;

// ==========================================
// 1. POŁĄCZENIE BLUETOOTH
// ==========================================
function initBLEEvents() {
  UI.btnConnect.addEventListener("click", async () => {
    if (bleDevice && bleDevice.gatt.connected) { disconnectBLE(); }
    else { connectBLE(); }
  });
}

async function connectBLE() {
  try {
    console.log("Szukanie urządzenia...");
    bleDevice = await navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: "XIAO_ShiftLight" }],
      optionalServices: [SERVICE_UUID]
    });

    bleDevice.addEventListener('gattserverdisconnected', onDisconnected);
    bleServer = await bleDevice.gatt.connect();

    const service = await bleServer.getPrimaryService(SERVICE_UUID);
    txCharacteristic = await service.getCharacteristic(TX_CHAR_UUID);
    rxCharacteristic = await service.getCharacteristic(RX_CHAR_UUID);

    await txCharacteristic.startNotifications();
    txCharacteristic.addEventListener('characteristicvaluechanged', handleNotifications);

    UI.btnConnectText.innerText = "Disconnect";
    UI.btnConnect.classList.replace("btn-primary", "btn-stop");
    UI.dotStatus.className = "pulse dot-margin";
    UI.sysState.className = "sys-state state-normal";
    UI.sysState.innerText = "🔗 BLE Connected — Oczekiwanie na CAN";

    setTimeout(() => { sendCmd("cmd:req_conf"); }, 500);
  } catch (error) {
    console.error("Błąd połączenia: ", error);
    alert("Błąd połączenia: " + error);
  }
}

function disconnectBLE() {
  if (bleDevice && bleDevice.gatt.connected) bleDevice.gatt.disconnect();
}

function onDisconnected() {
  console.log("Urządzenie Bluetooth rozłączone");
  UI.btnConnectText.innerText = "BLE Connect";
  UI.btnConnect.classList.replace("btn-stop", "btn-primary");
  UI.dotStatus.className = "pulse-offline dot-margin";
  UI.sysState.className = "sys-state state-offline";
  UI.sysState.innerText = "Link Offline... Złącz BLE by rozpocząć.";
}

// ==========================================
// 2. ODBIÓR I PARSOWANIE DANYCH
// ==========================================
function handleNotifications(event) {
  const value = event.target.value;
  const dataLen = value.byteLength;
  if (dataLen === 0) return;

  if (value.getUint8(0) === 0xBB) binIndex = 0;

  if (binIndex > 0 || value.getUint8(0) === 0xBB) {
    for (let i = 0; i < dataLen; i++) {
      if (binIndex < 30) binBuffer[binIndex++] = value.getUint8(i);
    }
    if (binIndex === 30) {
      parseBinaryTelemetry(new DataView(binBuffer.buffer));
      binIndex = 0;
    }
    return;
  }

  const chunk = textDecoder.decode(value);
  receiveBuffer += chunk;
  let lines = receiveBuffer.split("\n");
  receiveBuffer = lines.pop();
  for (let line of lines) { parseTelemetryJSON(line); }
}

function parseBinaryTelemetry(view) {
  try {
    const data = {
      mode: view.getUint8(1),
      speed: view.getUint8(2),
      temp: view.getInt16(3, true),
      load: view.getUint8(5),
      volt: view.getFloat32(6, true),
      rpm: view.getUint16(10, true),
      timerResult: view.getUint32(12, true),
      cTime: view.getUint32(16, true),
      iat: view.getInt8(20),
      tps: view.getUint8(21),
      map: view.getUint16(22, true),
      fuel: view.getUint8(24),
      gforce: view.getFloat32(25, true),
      log: view.getUint8(29) === 1
    };
    applyTelemetryToUI(data);
  } catch (e) { console.error("BIN Parse Error:", e); binIndex = 0; }
}

function applyTelemetryToUI(data) {
  if (lastData.speed !== data.speed) UI.speed.innerText = data.speed;

  const curTemp = data.temp !== undefined ? data.temp : (lastData.temp !== undefined ? lastData.temp : 999);
  if (lastData.temp !== curTemp) UI.temp.innerText = (curTemp === 999) ? "--" : curTemp;

  t_rpm = data.rpm !== undefined ? data.rpm : 0;
  t_load = data.load !== undefined ? data.load : (lastData.load !== undefined ? lastData.load : 0);
  t_volt = data.volt !== undefined ? data.volt : (lastData.volt !== undefined ? lastData.volt : 12.0);

  if (lastData.load !== t_load) UI.valLoad.innerText = t_load + "%";
  if (lastData.volt !== t_volt.toFixed(1)) UI.valVolt.innerText = t_volt.toFixed(1) + "V";

  t_tps = data.tps !== undefined ? data.tps : (lastData.tps !== undefined ? lastData.tps : 0);
  if (lastData.tps !== t_tps) UI.valTps.innerText = t_tps + "%";

  const iatVal = data.iat !== undefined ? data.iat : (lastData.iat !== undefined ? lastData.iat : 999);
  if (lastData.iat !== iatVal) UI.valIat.innerText = iatVal !== 999 ? iatVal : "--";

  if (data.map !== undefined && lastData.map !== data.map) UI.valMap.innerText = data.map;

  t_fuel = data.fuel !== undefined ? data.fuel : (lastData.fuel !== undefined ? lastData.fuel : 100);
  if (lastData.fuel !== t_fuel) UI.valFuel.innerText = t_fuel + "%";

  if (lastData.log !== data.log) {
    if (data.log) { UI.logInd.classList.add('active'); UI.btnLog.classList.add('active'); }
    else { UI.logInd.classList.remove('active'); UI.btnLog.classList.remove('active'); }
  }

  calculateG(data.speed, data.gforce);

  let ecoEnabled = UI.ecoMode.checked;
  let stCls = "sys-state", stTxt = "";
  if (curTemp === 999) { stCls += " state-offline"; stTxt = "⏳ Oczekiwanie Na Dane ECU (CAN)..."; }
  else if (ecoEnabled) { stCls += " state-eco"; stTxt = "🌿 Eco Drive Aktywny"; }
  else if (curTemp < 75) {
    stCls += " state-cold";
    let limit = Math.round(cachedRpmLimit * 0.5);
    stTxt = "⚠ Silnik Zimny — Ochrona (" + limit + " RPM)";
  } else { stCls += " state-normal"; stTxt = "✓ System Online — Normal"; }

  if (lastData.stTxt !== stTxt) { UI.sysState.className = stCls; UI.sysState.innerText = stTxt; }

  const finalTimerResult = data.timerResult !== undefined ? data.timerResult : (data.time || 0);
  const finalCTime = data.cTime !== undefined ? data.cTime : (data.current_time || 0);

  if (lastData.mode !== data.mode || lastData.cTime !== finalCTime) {
    if (data.mode <= 1) { UI.timerStatus.innerText = "STANDBY"; UI.timerStatus.style.color = "#1e90ff"; }
    else if (data.mode === 2) { UI.timerStatus.innerText = "COUNTDOWN..."; UI.timerStatus.style.color = "#1e90ff"; }
    else if (data.mode === 3) { UI.timerStatus.innerText = ">>> LAUNCH READY <<<"; UI.timerStatus.style.color = "#1e90ff"; UI.timerDisp.innerText = "0.00s"; }
    else if (data.mode === 4) { UI.timerStatus.innerText = "TRACKING..."; UI.timerStatus.style.color = "#1e90ff"; UI.timerDisp.innerText = (finalCTime / 1000).toFixed(2) + "s"; }
    else if (data.mode === 5) { UI.timerStatus.innerText = "FINISHED"; UI.timerStatus.style.color = "#2ed573"; UI.timerDisp.innerText = (finalTimerResult / 1000).toFixed(2) + "s"; }
  }
  lastData = { ...data, stTxt, load: t_load, volt: t_volt.toFixed(1), cTime: finalCTime, temp: curTemp, iat: iatVal };
}

function parseTelemetryJSON(jsonStr) {
  try {
    const data = JSON.parse(jsonStr);
    if (data.type === "config") {
      UI.rpmLimit.value = data.rpm;
      updateVal('rpmVal', data.rpm);
      UI.brightness.value = data.bright;
      updateVal('brtVal', data.bright);
      UI.ecoMode.checked = data.eco;
      UI.buzzerMode.checked = data.buzzer;

      // Synchronizacja cache'u przy starcie
      cachedRpmLimit = parseInt(data.rpm);
      maxRpm = cachedRpmLimit + 500;

      drawGaugeStatic();
      return;
    }
    applyTelemetryToUI(data);
  } catch (e) { console.error("JSON Parse Error:", e); }
}

async function sendCmd(cmdStr) {
  if (!rxCharacteristic) return false;
  try {
    await rxCharacteristic.writeValue(textEncoder.encode(cmdStr));
    return true;
  } catch (error) { console.error("Write Error:", error); return false; }
}

function saveSettings() {
  let r = UI.rpmLimit.value, b = UI.brightness.value;
  let eco = UI.ecoMode.checked ? 1 : 0, buz = UI.buzzerMode.checked ? 1 : 0;
  cachedRpmLimit = parseInt(r);
  maxRpm = cachedRpmLimit + 500;
  drawGaugeStatic();
  setupGradients();
  let btn = document.getElementById('btnSave');
  btn.innerText = "TX DATA...";
  sendCmd(`cmd:save:${r}:${b}:${eco}:${buz}`).then(success => {
    let stat = document.getElementById('status');
    stat.innerText = success ? "✓ ZAPISANO" : "✕ BŁĄD";
    stat.style.color = success ? "var(--green)" : "var(--red)";
    btn.innerText = "APPLY CONFIGURATION";
    setTimeout(() => stat.innerText = "", 3000);
  });
}

function startLogging() { sendCmd("cmd:log_start"); }
function stopLogging() { sendCmd("cmd:log_stop"); }

function cancelTimer() {
  sendCmd("cmd:cancel0100");
  UI.timerStatus.innerText = "ABORTING..."; UI.timerStatus.style.color = "var(--red)";
  if (UI.countdownOverlay) UI.countdownOverlay.style.display = 'none';
}

function startTimer() { sendCmd("cmd:start0100"); showCountdownOverlay(); }

function updateVal(id, val) {
  document.getElementById(id).innerText = val;
  if (id === 'rpmVal') {
    cachedRpmLimit = parseInt(val);
    maxRpm = cachedRpmLimit + 500;
    drawGaugeStatic();
    setupGradients();
  }
}

// ==========================================
// 4. GRAFIKI I CANVAS (RENDER ENGINE)
// ==========================================
function showCountdownOverlay() {
  if (!UI.countdownOverlay) return;
  UI.countdownOverlay.style.display = 'flex'; UI.countdownOverlay.style.opacity = '1';
  UI.countdownNum.style.color = "var(--red)"; UI.countdownNum.innerText = "5";
  [4, 3, 2, 1].forEach((v, i) => setTimeout(() => UI.countdownNum.innerText = v, (i + 1) * 1000));
  setTimeout(() => {
    UI.countdownNum.innerText = "GO!"; UI.countdownNum.style.color = "var(--green)";
    setTimeout(() => {
      UI.countdownOverlay.style.opacity = '0';
      setTimeout(() => UI.countdownOverlay.style.display = 'none', 300);
    }, 800);
  }, 5000);
}

function calculateG(speedKmh, currentG = null) {
  if (currentG !== null) {
    filteredG = currentG;
    gHistory.shift(); gHistory.push(filteredG);
    return;
  }
  let now = Date.now(), dt = (now - lastTimeForG) / 1000.0;
  if (dt >= 0.05) {
    let dv = (speedKmh - lastSpeedForG) / 3.6;
    let rawG = (dv / dt) / 9.81;
    filteredG = (rawG * 0.15) + (filteredG * 0.85);
    gHistory.shift(); gHistory.push(filteredG);
    lastSpeedForG = speedKmh; lastTimeForG = now;
  }
}

function drawMountain(ctx, isAccel, colorStr, fillStr, w, baseLine, maxG, step, hist) {
  ctx.beginPath();
  for (let i = 0; i < hist.length; i++) {
    let g = hist[i], val = isAccel ? (g > 0 ? g : 0) : (g < 0 ? Math.abs(g) : 0);
    let y = baseLine - (Math.min(val, maxG) / maxG) * (baseLine - 25);
    if (i === 0) ctx.moveTo(i * step, y); else ctx.lineTo(i * step, y);
  }
  ctx.lineTo(w, baseLine); ctx.lineTo(0, baseLine);
  ctx.fillStyle = fillStr; ctx.fill();
  ctx.lineWidth = 2; ctx.strokeStyle = colorStr; ctx.stroke();
}

function setupGradients() {
  const cnv = UI.gaugeDynamic;
  if (!cnv) return;
  const ctx = cnv.getContext("2d");
  const w = cnv.width, cy = cnv.height - 20;
  rpmGradient = ctx.createLinearGradient(0, cy, w, cy);
  rpmGradient.addColorStop(0, '#1e90ff');
  rpmGradient.addColorStop(0.3, '#2ed573');
  rpmGradient.addColorStop(0.7, '#ffa502');
  rpmGradient.addColorStop(1, '#ff4757');
}

function drawGaugeStatic() {
  const cnv = UI.gaugeStatic; if (!cnv) return;
  const ctx = cnv.getContext("2d"), w = cnv.width, h = cnv.height, cx = w / 2, cy = h - 20, r = 160;
  ctx.clearRect(0, 0, w, h);
  ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, 0);
  ctx.lineWidth = 18; ctx.strokeStyle = '#111820'; ctx.stroke();
  ctx.save(); ctx.translate(cx, cy);
  ctx.fillStyle = '#484f58'; ctx.font = 'bold 11px "Segoe UI"'; ctx.textAlign = 'center';
  for (let i = 0; i <= 10; i++) {
    let angle = -Math.PI + (i / 10) * Math.PI;
    ctx.fillText(i, Math.cos(angle) * (r - 28), Math.sin(angle) * (r - 28) + 4);
    ctx.beginPath(); ctx.moveTo(Math.cos(angle) * (r - 12), Math.sin(angle) * (r - 12)); ctx.lineTo(Math.cos(angle) * (r + 12), Math.sin(angle) * (r + 12));
    ctx.lineWidth = 2; ctx.strokeStyle = '#1e2530'; ctx.stroke();
  }
  ctx.restore();
}

function renderCanvas() {
  c_rpm += (t_rpm - c_rpm) * 0.3;
  c_load += (t_load - c_load) * 0.15;
  c_volt += (t_volt - c_volt) * 0.1;
  c_tps += (t_tps - c_tps) * 0.2;
  c_fuel += (t_fuel - c_fuel) * 0.1;
  drawGaugeDynamic();
  drawMiniBar(UI.gaugeLoad, c_load / 100, '#ff6348');
  drawMiniBar(UI.gaugeVolt, (c_volt - 10) / 5, '#2ed573');
  drawMiniBar(UI.gaugeTps, c_tps / 100, '#ffa502');
  drawMiniBar(UI.gaugeFuel, c_fuel / 100, '#1e90ff');
  drawTelemetryGraph();
  requestAnimationFrame(renderCanvas);
}

function drawGaugeDynamic() {
  const cnv = UI.gaugeDynamic; if (!cnv) return;
  const ctx = cnv.getContext("2d"), w = cnv.width, h = cnv.height, cx = w / 2, cy = h - 20, r = 160;
  ctx.clearRect(0, 0, w, h);
  let fill = Math.min(c_rpm / maxRpm, 1.0);
  if (fill > 0) {
    let strokeCol = '#2ed573';
    if (c_rpm >= cachedRpmLimit) strokeCol = (Math.floor(Date.now() / 80) % 2 === 0) ? '#ff4757' : '#1a0000';
    else strokeCol = rpmGradient || '#2ed573';
    ctx.save(); ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, Math.PI + fill * Math.PI);
    ctx.lineWidth = 18; ctx.lineCap = 'round'; ctx.strokeStyle = strokeCol;
    if (c_rpm >= cachedRpmLimit * 0.8) { ctx.shadowBlur = 15; ctx.shadowColor = '#ff4757'; }
    ctx.stroke(); ctx.restore();
  }
  ctx.fillStyle = '#e6edf3'; ctx.font = 'bold 52px "Segoe UI"'; ctx.textAlign = 'center';
  ctx.fillText(Math.round(c_rpm), cx, cy - 10);
  ctx.fillStyle = '#484f58'; ctx.font = 'bold 14px "Segoe UI"'; ctx.fillText("RPM", cx, cy + 12);
}

function drawMiniBar(cnv, val, col) {
  if (!cnv) return;
  let ctx = cnv.getContext("2d"), w = cnv.width, h = cnv.height;
  ctx.clearRect(0, 0, w, h); ctx.fillStyle = '#111820'; ctx.fillRect(0, 0, w, h);
  ctx.fillStyle = col; ctx.shadowBlur = 6; ctx.shadowColor = col;
  ctx.fillRect(0, 0, w * Math.max(0, Math.min(val, 1)), h);
}

function drawTelemetryGraph() {
  let cTel = document.getElementById("telemetryCanvas"); if (!cTel) return;
  let ctx = cTel.getContext("2d"), w = cTel.width, h = cTel.height;
  ctx.clearRect(0, 0, w, h); ctx.fillStyle = "#0a0d12"; ctx.fillRect(0, 0, w, h);
  let baseLine = h - 5, maxG = 1.5, step = w / (gHistory.length - 1);
  ctx.strokeStyle = "#1e2530"; ctx.beginPath(); ctx.moveTo(0, baseLine); ctx.lineTo(w, baseLine); ctx.stroke();
  drawMountain(ctx, true, "#2ed573", "rgba(46,213,115,0.2)", w, baseLine, maxG, step, gHistory);
  drawMountain(ctx, false, "#ff4757", "rgba(255,71,87,0.2)", w, baseLine, maxG, step, gHistory);
}

document.addEventListener("DOMContentLoaded", () => {
  UI.btnConnect = document.getElementById("btnConnect");
  UI.btnConnectText = UI.btnConnect.querySelector(".connect-text");
  UI.dotStatus = document.getElementById("connStatusDot"); UI.sysState = document.getElementById("sysState");
  UI.speed = document.getElementById("speedDisplay"); UI.temp = document.getElementById("tempDisplay");
  UI.valLoad = document.getElementById("valLoad"); UI.valVolt = document.getElementById("valVolt");
  UI.valTps = document.getElementById("valTps"); UI.valIat = document.getElementById("valIat");
  UI.valMap = document.getElementById("valMap"); UI.valFuel = document.getElementById("valFuel");
  UI.logInd = document.getElementById("logIndicator"); UI.btnLog = document.getElementById("btnLogStart");
  UI.timerStatus = document.getElementById("timerStatus"); UI.timerDisp = document.getElementById("timeDisplay");
  UI.rpmLimit = document.getElementById("rpmLimit"); UI.brightness = document.getElementById("brightness");
  UI.ecoMode = document.getElementById("ecoMode"); UI.buzzerMode = document.getElementById("buzzerMode");
  UI.gaugeStatic = document.getElementById("gaugeStaticCanvas"); UI.gaugeDynamic = document.getElementById("gaugeCanvas");
  UI.gaugeLoad = document.getElementById('loadCanvas'); UI.gaugeVolt = document.getElementById('voltCanvas');
  UI.gaugeTps = document.getElementById('tpsCanvas'); UI.gaugeFuel = document.getElementById('fuelCanvas');
  UI.countdownOverlay = document.getElementById('countdownOverlay');
  UI.countdownNum = document.getElementById('countdownNum');

  initBLEEvents();
  cachedRpmLimit = parseInt(UI.rpmLimit.value) || 6000;
  setupGradients();
  drawGaugeStatic();
  maxRpm = cachedRpmLimit + 500;
  requestAnimationFrame(renderCanvas);
  let btnExp = document.querySelector('.btn-export');
  if (btnExp) btnExp.onclick = () => alert("Pobieranie logów CSV odbywa się wyłącznie bezpośrednio z pamięci ESP przez USB.");
});
