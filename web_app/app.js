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
let binBuffer = new Uint8Array(31);
let binIndex = 0;
const textDecoder = new TextDecoder("utf-8");
const textEncoder = new TextEncoder();

// Zmienne zegarów i animacji
let t_rpm = 0, c_rpm = 0;
let t_load = 0, c_load = 0;
let t_volt = 10.0, c_volt = 10.0;
let t_tps = 0, c_tps = 0;
let t_fuel = 0, c_fuel = 100;
let maxRpm = 6000;
let gHistory = new Array(120).fill(0);
let lastSpeedForG = 0, lastTimeForG = Date.now(), filteredG = 0;

// Cache elementów interfejsu (DOM)
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
  btnConnectText: null,
  dlProgressBar: null,
  dlProgressFill: null,
  dlProgressText: null,
  flashFill: null,
  flashPerc: null,
  btnSaveSettings: null,
  btnLogStop: null,
  obdHz: null
};

let dlBuffer = [];
let dlExpectedSize = 0;
let dlReceivedSize = 0;

let lastData = {};
let rpmGradient = null;
let cachedRpmLimit = 6000;
let cachedEcoMode = false;

// Zmienne Peak Hold (maksymalne obroty)
let peakRpm = 0;
let peakRpmTime = 0;

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

    UI.btnConnectText.textContent = "Disconnect";
    UI.btnConnect.classList.replace("btn-primary", "btn-stop");
    UI.dotStatus.className = "pulse dot-margin";
    UI.sysState.className = "sys-state state-normal";
    UI.sysState.textContent = "🔗 BLE Connected — Oczekiwanie na CAN";

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
  UI.btnConnectText.textContent = "BLE Connect";
  UI.btnConnect.classList.replace("btn-stop", "btn-primary");
  UI.dotStatus.className = "pulse-offline dot-margin";
  UI.sysState.className = "sys-state state-offline";
  UI.sysState.textContent = "Link Offline... Złącz BLE by rozpocząć.";
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
      if (binIndex < 31) binBuffer[binIndex++] = value.getUint8(i);
    }
    if (binIndex === 31) {
      parseBinaryTelemetry(new DataView(binBuffer.buffer));
      binIndex = 0;
    }
    return;
  }

  // Obsługa pobierania plików (prefiks 0xCC)
  if (value.getUint8(0) === 0xCC) {
    const data = new Uint8Array(value.buffer, value.byteOffset + 1, dataLen - 1);
    dlBuffer.push(data);
    dlReceivedSize += data.length;
    updateDownloadProgress();
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
      log: view.getUint8(29) === 1,
      hz: view.getUint8(30)
    };
    applyTelemetryToUI(data);
  } catch (e) { console.error("BIN Parse Error:", e); binIndex = 0; }
}

function applyTelemetryToUI(data) {
  if (data.hz !== undefined) UI.obdHz.textContent = data.hz + " Hz";
  if (lastData.speed !== data.speed) UI.speed.textContent = data.speed;

  const curTemp = data.temp !== undefined ? data.temp : (lastData.temp !== undefined ? lastData.temp : 999);
  
  if (curTemp !== lastData.temp) UI.temp.textContent = (curTemp === 999) ? "--" : curTemp;

  t_rpm = data.rpm !== undefined ? data.rpm : 0;
  t_load = data.load !== undefined ? data.load : (lastData.load !== undefined ? lastData.load : 0);
  t_volt = data.volt !== undefined ? data.volt : (lastData.volt !== undefined ? lastData.volt : 12.0);

  if (lastData.load !== t_load) UI.valLoad.textContent = t_load + "%";
  if (lastData.volt !== t_volt.toFixed(1)) UI.valVolt.textContent = t_volt.toFixed(1) + "V";

  t_tps = data.tps !== undefined ? data.tps : (lastData.tps !== undefined ? lastData.tps : 0);
  if (lastData.tps !== t_tps) UI.valTps.textContent = t_tps + "%";

  const iatVal = data.iat !== undefined ? data.iat : (lastData.iat !== undefined ? lastData.iat : 999);
  if (data.map !== undefined && lastData.map !== data.map) UI.valMap.textContent = data.map;

  t_fuel = data.fuel !== undefined ? data.fuel : (lastData.fuel !== undefined ? lastData.fuel : 100);
  if (lastData.fuel !== t_fuel) UI.valFuel.textContent = t_fuel + "%";

  if (lastData.log !== data.log) {
    if (data.log) { UI.logInd.classList.add('active'); UI.btnLog.classList.add('active'); }
    else { UI.logInd.classList.remove('active'); UI.btnLog.classList.remove('active'); }
  }

  calculateG(data.speed, data.gforce);

  let stCls = "sys-state", stTxt = "";
  if (curTemp === 999) { stCls += " state-offline"; stTxt = "⏳ Oczekiwanie Na Dane ECU (CAN)..."; }
  else if (cachedEcoMode) { stCls += " state-eco"; stTxt = "🌿 Eco Drive Aktywny"; }
  else if (curTemp < 75) {
    stCls += " state-cold";
    let limit = Math.round(cachedRpmLimit * 0.5);
    stTxt = "⚠ Silnik Zimny — Ochrona (" + limit + " RPM)";
  } else { stCls += " state-normal"; stTxt = "✓ System Online — Normal"; }

  if (lastData.stTxt !== stTxt) { UI.sysState.className = stCls; UI.sysState.textContent = stTxt; }

  const finalTimerResult = data.timerResult !== undefined ? data.timerResult : (data.time || 0);
  const finalCTime = data.cTime !== undefined ? data.cTime : (data.current_time || 0);

  if (lastData.mode !== data.mode || lastData.cTime !== finalCTime) {
    if (data.mode <= 1) { UI.timerStatus.textContent = "STANDBY"; UI.timerStatus.style.color = "#1e90ff"; }
    else if (data.mode === 2) { UI.timerStatus.textContent = "COUNTDOWN..."; UI.timerStatus.style.color = "#1e90ff"; }
    else if (data.mode === 3) { UI.timerStatus.textContent = ">>> LAUNCH READY <<<"; UI.timerStatus.style.color = "#1e90ff"; UI.timerDisp.textContent = "0.00s"; }
    else if (data.mode === 4) { UI.timerStatus.textContent = "TRACKING..."; UI.timerStatus.style.color = "#1e90ff"; UI.timerDisp.textContent = (finalCTime / 1000).toFixed(2) + "s"; }
    else if (data.mode === 5) { UI.timerStatus.textContent = "FINISHED"; UI.timerStatus.style.color = "#2ed573"; UI.timerDisp.textContent = (finalTimerResult / 1000).toFixed(2) + "s"; }
  }
  lastData = { ...data, stTxt, load: t_load, volt: t_volt.toFixed(1), cTime: finalCTime, temp: curTemp, iat: iatVal, rpmTime: Date.now() };
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
      cachedEcoMode = data.eco; // Sync cache
      UI.buzzerMode.checked = data.buzzer;

      if (data.f_total > 0) {
        const perc = Math.round((data.f_used / data.f_total) * 100);
        UI.flashFill.style.width = perc + "%";
        UI.flashPerc.innerText = perc + "%";
      }

      // Synchronizacja cache'u przy starcie
      cachedRpmLimit = parseInt(data.rpm);
      maxRpm = cachedRpmLimit;

      drawGaugeStatic();
      return;
    }
    if (data.type === "download_start") {
      dlBuffer = [];
      dlReceivedSize = 0;
      dlExpectedSize = data.size;
      UI.dlProgressBar.style.display = 'block';
      toggleUIBlocking(true);
      updateDownloadProgress();
      return;
    }
    if (data.type === "download_end") {
      console.log("Download finished, converting binary to CSV...");
      
      // Konwersja binarna: scalanie wszystkich kawałków w jeden bufor
      const totalLen = dlBuffer.reduce((acc, curr) => acc + curr.length, 0);
      const combined = new Uint8Array(totalLen);
      let offset = 0;
      for (let chunk of dlBuffer) { combined.set(chunk, offset); offset += chunk.length; }

      // Parsowanie rekordów (44 bajty każdy, zgodnie z TelemetryData w C++)
      const recordSize = 44;
      const csvRows = ["Time(ms),RPM,Speed(km/h),Temp(C),Load(%),Volt(V),IAT(C),TPS(%),MAP(kPa),Fuel(%),G(g)"];
      const view = new DataView(combined.buffer);
      
      for (let i = 0; i <= totalLen - recordSize; i += recordSize) {
        let t    = view.getUint32(i, true);
        let rpm  = view.getInt32(i + 4, true);
        let spd  = view.getInt32(i + 8, true);
        let tmp  = view.getInt32(i + 12, true);
        let load = view.getInt32(i + 16, true);
        let volt = view.getFloat32(i + 20, true).toFixed(2);
        let iat  = view.getInt32(i + 24, true);
        let tps  = view.getInt32(i + 28, true);
        let map  = view.getInt32(i + 32, true);
        let fuel = view.getInt32(i + 36, true);
        let g    = view.getFloat32(i + 40, true).toFixed(2);
        
        csvRows.push(`${t},${rpm},${spd},${tmp},${load},${volt},${iat},${tps},${map},${fuel},${g}`);
      }

      const blob = new Blob([csvRows.join('\n')], { type: 'text/csv' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `telemetry_${new Date().getTime()}.csv`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
      
      UI.dlProgressBar.style.display = 'none';
      toggleUIBlocking(false);
      return;
    }
    if (data.type === "download_error") {
      alert("Błąd podczas otwierania pliku na urządzeniu.");
      UI.dlProgressBar.style.display = 'none';
      toggleUIBlocking(false);
      return;
    }
    if (data.type === "fs_stat") {
      if (data.f_total > 0) {
        const perc = Math.round((data.f_used / data.f_total) * 100);
        UI.flashFill.style.width = perc + "%";
        UI.flashPerc.textContent = perc + "%";
      }
      return;
    }
    applyTelemetryToUI(data);
  } catch (e) { console.error("JSON Parse Error:", e); }
}

function updateDownloadProgress() {
  if (dlExpectedSize === 0) return;
  const p = Math.min(100, Math.round((dlReceivedSize / dlExpectedSize) * 100));
  UI.dlProgressFill.style.width = p + "%";
  UI.dlProgressText.textContent = p + "% (" + Math.round(dlReceivedSize / 1024) + "KB)";
}

function downloadLog() {
  if (!bleDevice || !bleDevice.gatt.connected) {
    alert("Najpierw połącz się z urządzeniem przez BLE.");
    return;
  }
  sendCmd("cmd:log_dl");
}

function toggleUIBlocking(blocked) {
  const btns = [UI.btnConnect, UI.btnLog, UI.btnLogStop, UI.btnSaveSettings, UI.rpmLimit, UI.brightness, UI.ecoMode, UI.buzzerMode];
  btns.forEach(el => { if (el) el.disabled = blocked; });
  const exportBtn = document.querySelector('.btn-export');
  if (exportBtn) exportBtn.disabled = blocked;
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
  btn.textContent = "TX DATA...";
  sendCmd(`cmd:save:${r}:${b}:${eco}:${buz}`).then(success => {
    let stat = document.getElementById('status');
    stat.textContent = success ? "✓ ZAPISANO" : "✕ BŁĄD";
    stat.style.color = success ? "var(--green)" : "var(--red)";
    btn.textContent = "APPLY CONFIGURATION";
    setTimeout(() => stat.textContent = "", 3000);
  });
}

function startLogging() { sendCmd("cmd:log_start"); }
function stopLogging() { sendCmd("cmd:log_stop"); }

function cancelTimer() {
  sendCmd("cmd:cancel0100");
  UI.timerStatus.textContent = "ABORTING..."; UI.timerStatus.style.color = "var(--red)";
  if (UI.countdownOverlay) UI.countdownOverlay.style.display = 'none';
}

function startTimer() { sendCmd("cmd:start0100"); showCountdownOverlay(); }

function updateVal(id, val) {
  document.getElementById(id).textContent = val;
  if (id === 'rpmVal') {
    cachedRpmLimit = parseInt(val);
    maxRpm = cachedRpmLimit;
    drawGaugeStatic();
    setupGradients();
  }
}

// ==========================================
// 3. GRAFIKI I CANVAS (RENDER ENGINE)
// ==========================================
function showCountdownOverlay() {
  if (!UI.countdownOverlay) return;
  UI.countdownOverlay.style.display = 'flex'; UI.countdownOverlay.style.opacity = '1';
  UI.countdownNum.style.color = "var(--red)"; UI.countdownNum.textContent = "5";
  [4, 3, 2, 1].forEach((v, i) => setTimeout(() => UI.countdownNum.textContent = v, (i + 1) * 1000));
  setTimeout(() => {
    UI.countdownNum.textContent = "GO!"; UI.countdownNum.style.color = "var(--green)";
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
  const divisions = Math.ceil(maxRpm / 1000);
  for (let i = 0; i <= divisions; i++) {
    let angle = -Math.PI + (i / divisions) * Math.PI;
    ctx.fillText(i, Math.cos(angle) * (r - 28), Math.sin(angle) * (r - 28) + 4);
    ctx.beginPath(); ctx.moveTo(Math.cos(angle) * (r - 12), Math.sin(angle) * (r - 12)); 
    ctx.lineTo(Math.cos(angle) * (r + 12), Math.sin(angle) * (r + 12));
    ctx.lineWidth = 2; ctx.strokeStyle = '#1e2530'; ctx.stroke();
  }
  ctx.restore();
}

function renderCanvas() {
  const curMode = lastData.mode !== undefined ? lastData.mode : 0;
  const curTemp = lastData.temp !== undefined ? lastData.temp : 999;

  c_rpm += (t_rpm - c_rpm) * 0.3;
  c_load += (t_load - c_load) * 0.15;
  c_volt += (t_volt - c_volt) * 0.1;
  c_tps += (t_tps - c_tps) * 0.2;
  c_fuel += (t_fuel - c_fuel) * 0.1;
  drawGaugeDynamic(curMode, curTemp);
  drawMiniBar(UI.gaugeLoad, c_load / 100, '#ff6348');
  drawMiniBar(UI.gaugeVolt, (c_volt - 10) / 5, '#2ed573');
  drawMiniBar(UI.gaugeTps, c_tps / 100, '#ffa502');
  drawMiniBar(UI.gaugeFuel, c_fuel / 100, '#1e90ff');
  drawTelemetryGraph();
  requestAnimationFrame(renderCanvas);
}

function drawGaugeDynamic(mode, temp) {
  const cnv = UI.gaugeDynamic; if (!cnv) return;
  const ctx = cnv.getContext("2d"), w = cnv.width, h = cnv.height, cx = w / 2, cy = h - 20, r = 160;
  ctx.clearRect(0, 0, w, h);
  let fill = Math.min(c_rpm / maxRpm, 1.0);
  if (fill > 0) {
    let strokeCol = '#2ed573';
    if (c_rpm >= cachedRpmLimit) strokeCol = (Math.floor(Date.now() / 80) % 2 === 0) ? '#ff4757' : '#1a0000';
    else strokeCol = rpmGradient || '#2ed573';
    // Obsługa trybu TEST (mruganie tęczą/białym)
    if (mode === 6) strokeCol = (Math.floor(Date.now() / 150) % 2 === 0) ? '#ffffff' : '#2ed573';
    ctx.save(); ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, Math.PI + fill * Math.PI);
    ctx.lineWidth = 18; ctx.lineCap = 'round'; ctx.strokeStyle = strokeCol;
    if (c_rpm >= cachedRpmLimit * 0.8) { ctx.shadowBlur = 15; ctx.shadowColor = '#ff4757'; }
    ctx.stroke(); ctx.restore();
  }
  ctx.fillStyle = '#e6edf3'; ctx.font = 'bold 52px "Segoe UI"'; ctx.textAlign = 'center';
  ctx.fillText(Math.round(c_rpm), cx, cy - 10);
  ctx.fillStyle = '#484f58'; ctx.font = 'bold 14px "Segoe UI"'; ctx.fillText("RPM", cx, cy + 12);

  // LOGIKA PEAK HOLD
  if (c_rpm > peakRpm) {
    peakRpm = c_rpm;
    peakRpmTime = Date.now();
  } else if (Date.now() - peakRpmTime > 2000) {
    peakRpm = c_rpm; // Zresetuj peak do aktualnych obrotów po 2 sekundach
  }

  // RYSUJ ZNACZNIK SZCZYTOWY
  if (peakRpm > 0) {
    let peakFill = Math.min(peakRpm / maxRpm, 1.0);
    let peakAngle = Math.PI + peakFill * Math.PI;
    ctx.save();
    ctx.beginPath();
    ctx.arc(cx, cy, r, peakAngle - 0.01, peakAngle + 0.01);
    ctx.lineWidth = 18;
    ctx.strokeStyle = 'rgba(255, 71, 87, 0.6)'; // Półprzezroczysty czerwony znacznik
    ctx.stroke();
    ctx.restore();
  }

  // WIZUALNA STREFA ZIMNEGO SILNIKA
  if (temp < 75 && temp !== 999) {
    ctx.save();
    ctx.beginPath();
    let coldEnd = Math.min(3000 / maxRpm, 1.0);
    ctx.arc(cx, cy, r, Math.PI, Math.PI + coldEnd * Math.PI);
    ctx.lineWidth = 18;
    ctx.strokeStyle = 'rgba(30, 144, 255, 0.3)'; // Półprzezroczysty niebieski
    ctx.stroke();
    ctx.restore();
  }
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
  UI.dlProgressBar = document.getElementById('dlProgressBar');
  UI.dlProgressFill = document.getElementById('dlProgressFill');
  UI.dlProgressText = document.getElementById('dlProgressText');
  UI.flashFill = document.getElementById('flashFill');
  UI.flashPerc = document.getElementById('flashPerc');
  UI.btnSaveSettings = document.getElementById('btnSave');
  UI.btnLogStop = document.getElementById('btnLogStop');
  UI.obdHz = document.getElementById("obdHz");

  initBLEEvents();

  UI.ecoMode.addEventListener('change', (e) => { cachedEcoMode = e.target.checked; });

  cachedRpmLimit = parseInt(UI.rpmLimit.value) || 6000;
  setupGradients();
  drawGaugeStatic();
  maxRpm = cachedRpmLimit;
  requestAnimationFrame(renderCanvas);
  let btnExp = document.querySelector('.btn-export');
  if (btnExp) btnExp.onclick = () => alert("Pobieranie logów CSV odbywa się poprzez połączenie Bluetooth.");
});
