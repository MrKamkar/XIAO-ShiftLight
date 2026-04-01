// ==========================================
// XIAO ShiftLight - Web Bluetooth Dashboard
// ==========================================

// Globalne zmienne BLE
let bleDevice = null;
let bleServer = null;
let txCharacteristic = null; // ESP32 -> Przeglądarka (Powiadomienia)
let rxCharacteristic = null; // Przeglądarka -> ESP32 (Komendy)

const SERVICE_UUID = "4fafc201-1fb5-459e-8bcc-c5c9c331914b";
const TX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const RX_CHAR_UUID = "beb5483e-36e2-4688-b7f5-ea07361b26a8";

// Zarządzanie buforem przychodzących danych
let receiveBuffer = "";
const textDecoder = new TextDecoder("utf-8");
const textEncoder = new TextEncoder();

// Zmienne zegarów i animacji
let t_rpm = 0, c_rpm = 0;
let t_load = 0, c_load = 0;
let t_volt = 12.0, c_volt = 12.0;
let t_tps = 0, c_tps = 0;
let t_fuel = 0, c_fuel = 0;
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
  gaugeFuel: null
};

// Pamięć ostatnich wartości, by nie odświeżać DOM bez potrzeby
let lastData = {};

// ==========================================
// 1. POŁĄCZENIE BLUETOOTH
// ==========================================
function initBLEEvents() {
  UI.btnConnect.addEventListener("click", async () => {
    if (bleDevice && bleDevice.gatt.connected) {
      disconnectBLE();
    } else {
      connectBLE();
    }
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

    console.log("Łączenie z GATT Server...");
    bleServer = await bleDevice.gatt.connect();

    console.log("Szukanie usługi...");
    const service = await bleServer.getPrimaryService(SERVICE_UUID);

    console.log("Szukanie charakterystyk..");
    txCharacteristic = await service.getCharacteristic(TX_CHAR_UUID);
    rxCharacteristic = await service.getCharacteristic(RX_CHAR_UUID);

    // Subskrypcja na powiadomienia (TX)
    await txCharacteristic.startNotifications();
    txCharacteristic.addEventListener('characteristicvaluechanged', handleNotifications);

    // Ustawienie UI
    UI.btnConnect.innerText = "Rozłącz BLE";
    UI.btnConnect.classList.replace("btn-primary", "btn-stop");
    UI.dotStatus.className = "pulse dot-margin"; // Zielona pulsująca kropka
    UI.sysState.className = "sys-state state-normal";
    UI.sysState.innerText = "🔗 BLE Connected — Oczekiwanie na CAN";
    
    console.log("Bluetooth pomyślnie podłączony!");
    
    // Zapytaj o aktualną konfigurację po połączeniu
    setTimeout(() => { sendCmd("cmd:get_config"); }, 500);

  } catch (error) {
    console.error("Błąd połączenia: ", error);
    UI.btnConnect.innerText = "BLE Connect";
    UI.btnConnect.classList.replace("btn-stop", "btn-primary");
    alert("Błąd połączenia: " + error + "\nUpewnij się, że masz włączony Bluetooth i korzystasz z HTTPS.");
  }
}

function disconnectBLE() {
  if (!bleDevice) return;
  console.log("Rozłączanie...");
  if (bleDevice.gatt.connected) {
    bleDevice.gatt.disconnect();
  }
}

function onDisconnected() {
  console.log("Urządzenie Bluetooth rozłączone");
  UI.btnConnect.innerText = "BLE Connect";
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
  
  // Detekcja Magicznego Bajtu 0xBB (Protokół Binarny)
  if (value.byteLength > 0 && value.getUint8(0) === 0xBB) {
    parseBinaryTelemetry(value);
    return;
  }

  // Fallback dla JSON (np. konfiguracja)
  const chunk = textDecoder.decode(value);
  receiveBuffer += chunk;
  let lines = receiveBuffer.split("\n");
  receiveBuffer = lines.pop(); 
  for (let line of lines) {
    parseTelemetryJSON(line);
  }
}

function parseBinaryTelemetry(view) {
  try {
    const data = {
      mode:         view.getUint8(1),
      speed:        view.getUint8(2),
      temp:         view.getInt16(3, true),
      load:         view.getUint8(5),
      volt:         view.getFloat32(6, true),
      rpm:          view.getUint16(10, true),
      timerResult:  view.getUint32(12, true),
      current_time: view.getUint32(16, true),
      iat:          view.getInt8(20),
      tps:          view.getUint8(21),
      map:          view.getUint16(22, true),
      fuel:         view.getUint8(24),
      gforce:       view.getFloat32(25, true),
      log:          view.getUint8(29) === 1
    };
    applyTelemetryToUI(data);
  } catch(e) { console.error("BIN Parse Error:", e); }
}

function applyTelemetryToUI(data) {
    if (lastData.speed !== data.speed) UI.speed.innerText = data.speed;
    if (lastData.temp !== data.temp)   UI.temp.innerText = data.temp === 999 ? "--" : data.temp;

    t_rpm = data.rpm || 0;
    t_load = data.load;
    t_volt = data.volt;

    if (lastData.load !== Math.round(t_load)) UI.valLoad.innerText = Math.round(t_load) + "%";
    if (lastData.volt !== t_volt.toFixed(1))  UI.valVolt.innerText = t_volt.toFixed(1) + "V";

    t_tps = data.tps; 
    if (lastData.tps !== Math.round(t_tps)) UI.valTps.innerText = Math.round(t_tps) + "%";
    if (lastData.iat !== data.iat) UI.valIat.innerText = data.iat !== 999 ? data.iat : "--";
    if (lastData.map !== data.map) UI.valMap.innerText = data.map;
    t_fuel = data.fuel; 
    if (lastData.fuel !== Math.round(t_fuel)) UI.valFuel.innerText = Math.round(t_fuel) + "%";

    if (lastData.log !== data.log) {
      if (data.log) { UI.logInd.classList.add('active'); UI.btnLog.classList.add('active'); }
      else { UI.logInd.classList.remove('active'); UI.btnLog.classList.remove('active'); }
    }

    calculateG(data.speed, data.gforce);

    let ecoEnabled = UI.ecoMode.checked;
    let stCls = "sys-state", stTxt = "";
    if (data.temp === 999) {
      stCls += " state-offline"; stTxt = "⏳ Oczekiwanie Na Dane ECU (CAN)...";
    } else if (ecoEnabled) {
      stCls += " state-eco"; stTxt = "🌿 Eco Drive Aktywny";
    } else if (data.temp < 75) {
      stCls += " state-cold";
      let limit = Math.round(parseInt(UI.rpmLimit.value) * 0.5);
      stTxt = "⚠ Silnik Zimny — Ochrona (" + limit + " RPM)";
    } else {
      stCls += " state-normal"; stTxt = "✓ System Online — Normal";
    }
    
    if (lastData.stTxt !== stTxt) { UI.sysState.className = stCls; UI.sysState.innerText = stTxt; }

    if (lastData.mode !== data.mode || lastData.cTime !== data.current_time) {
        if (data.mode <= 1) { UI.timerStatus.innerText = "STANDBY"; UI.timerStatus.style.color = "#1e90ff"; }
        else if (data.mode === 2) { UI.timerStatus.innerText = "COUNTDOWN..."; UI.timerStatus.style.color = "#1e90ff"; }
        else if (data.mode === 3) { UI.timerStatus.innerText = ">>> LAUNCH READY <<<"; UI.timerStatus.style.color = "#1e90ff"; UI.timerDisp.innerText = "0.00s"; }
        else if (data.mode === 4) { UI.timerStatus.innerText = "TRACKING..."; UI.timerStatus.style.color = "#1e90ff"; UI.timerDisp.innerText = (data.current_time / 1000.0).toFixed(2) + "s"; }
        else if (data.mode === 5) { UI.timerStatus.innerText = "FINISHED"; UI.timerStatus.style.color = "#2ed573"; UI.timerDisp.innerText = (data.timerResult / 1000.0).toFixed(2) + "s"; }
    }
    lastData = {...data, stTxt, load: Math.round(t_load), volt: t_volt.toFixed(1), cTime: data.current_time};
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
      drawGaugeStatic();
      return;
    }
    applyTelemetryToUI(data);
  } catch (e) { console.error("JSON Parse Error:", e); }
}

// ==========================================
// 3. WYSYŁANIE KOMEND NA ESP32 (Write)
// ==========================================
async function sendCmd(cmdStr) {
  if (!rxCharacteristic) return false;
  try {
    await rxCharacteristic.writeValue(textEncoder.encode(cmdStr));
    return true;
  } catch (error) {
    console.error("Wysłanie zablokowane:", error);
    return false;
  }
}

function saveSettings() {
  let r = UI.rpmLimit.value;
  let b = UI.brightness.value;
  let eco = UI.ecoMode.checked ? 1 : 0;
  let buz = UI.buzzerMode.checked ? 1 : 0;
  maxRpm = parseInt(r) + 500;
  drawGaugeStatic();
  
  let btn = document.getElementById('btnSave');
  btn.innerText = "TX DATA...";
  
  sendCmd(`cmd:save:${r}:${b}:${eco}:${buz}`).then(success => {
    let stat = document.getElementById('status');
    if (success) { stat.innerText = "✓ ZAPISANO"; stat.style.color = "var(--green)"; }
    else { stat.innerText = "✕ BŁĄD"; stat.style.color = "var(--red)"; }
    btn.innerText = "APPLY CONFIGURATION";
    setTimeout(() => stat.innerText = "", 3000);
  });
}

function startLogging() { sendCmd("cmd:log_start"); }
function stopLogging() { sendCmd("cmd:log_stop"); }

function cancelTimer() {
  sendCmd("cmd:cancel0100");
  UI.timerStatus.innerText = "ABORTING...";
  UI.timerStatus.style.color = "var(--red)";
  document.getElementById('countdownOverlay').style.display = 'none';
}

function startTimer() {
  sendCmd("cmd:start0100");
  showCountdownOverlay();
}

function updateVal(id, val) {
  document.getElementById(id).innerText = val;
  if (id === 'rpmVal') { maxRpm = parseInt(val) + 500; drawGaugeStatic(); }
}

// ==========================================
// 4. GRAFIKI I CANVAS (RENDER ENGINE)
// ==========================================

function showCountdownOverlay() {
  let overlay = document.getElementById('countdownOverlay');
  let num = document.getElementById('countdownNum');
  overlay.style.display = 'flex';
  overlay.style.opacity = '1';
  num.style.color = "var(--red)";
  num.innerText = "5";
  [4,3,2,1].forEach((v, i) => setTimeout(() => num.innerText = v, (i+1)*1000));
  setTimeout(() => {
    num.innerText = "GO!";
    num.style.color = "var(--green)";
    setTimeout(() => { overlay.style.opacity = '0'; setTimeout(() => overlay.style.display = 'none', 300); }, 800);
  }, 5000);
}

function calculateG(speedKmh, currentG = null) {
  if (currentG !== null) {
    filteredG = currentG;
    gHistory.shift(); gHistory.push(filteredG);
    return;
  }
  let now = Date.now();
  let dt = (now - lastTimeForG) / 1000.0;
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
    let g = hist[i];
    let val = isAccel ? (g > 0 ? g : 0) : (g < 0 ? Math.abs(g) : 0);
    let y = baseLine - (Math.min(val, maxG) / maxG) * (baseLine - 25);
    if (i === 0) ctx.moveTo(i * step, y); else ctx.lineTo(i * step, y);
  }
  ctx.lineTo(w, baseLine); ctx.lineTo(0, baseLine);
  ctx.fillStyle = fillStr; ctx.fill();
  ctx.lineWidth = 2; ctx.strokeStyle = colorStr; ctx.stroke();
}

function drawGaugeStatic() {
  const cnv = UI.gaugeStatic; if (!cnv) return;
  const ctx = cnv.getContext("2d"), w = cnv.width, h = cnv.height;
  const cx = w/2, cy = h-20, r = 160;
  ctx.clearRect(0, 0, w, h);
  ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, 0);
  ctx.lineWidth = 18; ctx.strokeStyle = '#111820'; ctx.stroke();
  ctx.save(); ctx.translate(cx, cy);
  ctx.fillStyle = '#484f58'; ctx.font = 'bold 11px "Segoe UI"'; ctx.textAlign = 'center';
  for (let i = 0; i <= 10; i++) {
    let angle = -Math.PI + (i/10)*Math.PI;
    ctx.fillText(i, Math.cos(angle)*(r-28), Math.sin(angle)*(r-28)+4);
  }
  ctx.restore();
}

function renderCanvas() {
  // Interpolacja dla płynności ruchu wskazówek i barów
  c_rpm += (t_rpm - c_rpm) * 0.3;
  c_load += (t_load - c_load) * 0.15;
  c_volt += (t_volt - c_volt) * 0.1;
  c_tps += (t_tps - c_tps) * 0.2;
  c_fuel += (t_fuel - c_fuel) * 0.1;

  // 1. Warstwa Dynamiczna Zegara
  drawGaugeDynamic();
  
  // 2. Mini Wskaźniki Panelu
  drawMiniBar(UI.gaugeLoad, c_load / 100, '#ff6348');
  drawMiniBar(UI.gaugeVolt, (c_volt - 10) / 5, '#2ed573');
  drawMiniBar(UI.gaugeTps, c_tps / 100, '#ffa502');
  drawMiniBar(UI.gaugeFuel, c_fuel / 100, '#1e90ff');
  
  // 3. Wykres Przeciążeń
  drawTelemetryGraph();

  requestAnimationFrame(renderCanvas);
}

function drawGaugeDynamic() {
  const cnv = UI.gaugeDynamic; if (!cnv) return;
  const ctx = cnv.getContext("2d"), w = cnv.width, h = cnv.height, cx = w/2, cy = h-20, r = 160;
  ctx.clearRect(0, 0, w, h);
  let fill = Math.min(c_rpm / maxRpm, 1.0);
  if (fill > 0) {
    let limit = parseInt(UI.rpmLimit.value);
    let strokeCol = '#2ed573';
    if (c_rpm >= limit) strokeCol = (Math.floor(Date.now()/80)%2===0) ? '#ff4757' : '#1a0000';
    else {
      let grad = ctx.createLinearGradient(0, cy, w, cy);
      grad.addColorStop(0,'#1e90ff'); grad.addColorStop(0.3,'#2ed573'); grad.addColorStop(0.7,'#ffa502'); grad.addColorStop(1,'#ff4757');
      strokeCol = grad;
    }
    ctx.save(); ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, Math.PI + fill*Math.PI);
    ctx.lineWidth = 18; ctx.lineCap = 'round'; ctx.strokeStyle = strokeCol;
    if (c_rpm >= limit*0.8) { ctx.shadowBlur = 15; ctx.shadowColor = '#ff4757'; }
    ctx.stroke(); ctx.restore();
  }
  ctx.fillStyle = '#e6edf3'; ctx.font = 'bold 52px "Segoe UI"'; ctx.textAlign = 'center';
  ctx.fillText(Math.round(c_rpm), cx, cy-10);
  ctx.fillStyle = '#484f58'; ctx.font = 'bold 14px "Segoe UI"'; ctx.fillText("RPM", cx, cy+12);
}

function drawMiniBar(cnv, val, col) {
  if (!cnv) return;
  let ctx = cnv.getContext("2d"), w = cnv.width, h = cnv.height;
  ctx.clearRect(0, 0, w, h); ctx.fillStyle = '#111820'; ctx.fillRect(0,0,w,h);
  ctx.fillStyle = col; ctx.shadowBlur = 6; ctx.shadowColor = col;
  ctx.fillRect(0,0,w*Math.max(0,Math.min(val,1)),h);
}

function drawTelemetryGraph() {
  let cTel = document.getElementById("telemetryCanvas"); if (!cTel) return;
  let ctx = cTel.getContext("2d"), w = cTel.width, h = cTel.height;
  ctx.clearRect(0, 0, w, h); ctx.fillStyle = "#0a0d12"; ctx.fillRect(0,0,w,h);
  let baseLine = h-5, maxG = 1.5, step = w/(gHistory.length-1);
  ctx.strokeStyle = "#1e2530"; ctx.beginPath(); ctx.moveTo(0,baseLine); ctx.lineTo(w,baseLine); ctx.stroke();
  drawMountain(ctx, true, "#2ed573", "rgba(46,213,115,0.2)", w, baseLine, maxG, step, gHistory);
  drawMountain(ctx, false, "#ff4757", "rgba(255,71,87,0.2)", w, baseLine, maxG, step, gHistory);
}

document.addEventListener("DOMContentLoaded", () => {
  UI.btnConnect = document.getElementById("btnConnect");
  UI.dotStatus = document.getElementById("connStatusDot");
  UI.sysState = document.getElementById("sysState");
  UI.speed = document.getElementById("speedDisplay");
  UI.temp = document.getElementById("tempDisplay");
  UI.valLoad = document.getElementById("valLoad"); UI.valVolt = document.getElementById("valVolt");
  UI.valTps = document.getElementById("valTps"); UI.valIat = document.getElementById("valIat");
  UI.valMap = document.getElementById("valMap"); UI.valFuel = document.getElementById("valFuel");
  UI.logInd = document.getElementById("logIndicator"); UI.btnLog = document.getElementById("btnLogStart");
  UI.timerStatus = document.getElementById("timerStatus"); UI.timerDisp = document.getElementById("timeDisplay");
  UI.rpmLimit = document.getElementById("rpmLimit"); UI.brightness = document.getElementById("brightness");
  UI.ecoMode = document.getElementById("ecoMode"); UI.buzzerMode = document.getElementById("buzzerMode");
  UI.gaugeStatic = document.getElementById("gaugeStaticCanvas");
  UI.gaugeDynamic = document.getElementById("gaugeCanvas");
  UI.gaugeLoad = document.getElementById('loadCanvas'); UI.gaugeVolt = document.getElementById('voltCanvas');
  UI.gaugeTps = document.getElementById('tpsCanvas'); UI.gaugeFuel = document.getElementById('fuelCanvas');

  initBLEEvents();
  drawGaugeStatic(); 
  maxRpm = parseInt(UI.rpmLimit.value) + 500;
  requestAnimationFrame(renderCanvas);
  let btnExp = document.querySelector('.btn-export');
  if(btnExp) btnExp.onclick = () => alert("Pobieranie logów CSV odbywa się wyłącznie po kablu USB.");
});
