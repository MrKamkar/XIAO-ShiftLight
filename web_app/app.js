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

// UI Elements
const btnConnect = document.getElementById("btnConnect");
const dotStatus = document.getElementById("connStatusDot");
const sysState = document.getElementById("sysState");

// ==========================================
// 1. POŁĄCZENIE BLUETOOTH
// ==========================================
btnConnect.addEventListener("click", async () => {
  if (bleDevice && bleDevice.gatt.connected) {
    disconnectBLE();
  } else {
    connectBLE();
  }
});

async function connectBLE() {
  try {
    console.log("Żądanie urządzenia Bluetooth...");
    btnConnect.innerText = "Skanowanie...";
    
    // Zapytaj użytkownika o parowanie
    bleDevice = await navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: "XIAO_ShiftLight" }],
      optionalServices: [SERVICE_UUID]
    });

    bleDevice.addEventListener('gattserverdisconnected', onDisconnected);

    console.log("Łączenie z serwerem GATT...");
    btnConnect.innerText = "Łączenie GATT...";
    bleServer = await bleDevice.gatt.connect();

    console.log("Pobieranie serwisu głównego...");
    const service = await bleServer.getPrimaryService(SERVICE_UUID);

    console.log("Pobieranie charakterystyk...");
    txCharacteristic = await service.getCharacteristic(TX_CHAR_UUID);
    rxCharacteristic = await service.getCharacteristic(RX_CHAR_UUID);

    // Włączenie notyfikacji (odbieranie telemetrii na żywo)
    await txCharacteristic.startNotifications();
    txCharacteristic.addEventListener('characteristicvaluechanged', handleNotifications);

    // Ustawienie UI
    btnConnect.innerText = "Rozłącz BLE";
    btnConnect.classList.replace("btn-primary", "btn-stop");
    dotStatus.className = "pulse dot-margin"; // Zielona pulsująca kropka
    sysState.className = "sys-state state-normal";
    sysState.innerText = "🔗 BLE Connected — Oczekiwanie na CAN";
    
    console.log("Bluetooth pomyślnie podłączony!");
    
    // Wyślij żądanie wysłania przez ESP aktualnej konfiguracji 
    sendCmd("cmd:req_conf");

  } catch (error) {
    console.error("Błąd połączenia: ", error);
    btnConnect.innerText = "BLE Connect";
    btnConnect.classList.replace("btn-stop", "btn-primary");
    alert("Błąd połączenia: " + error + "\nUpewnij się, że masz włączony Bluetooth pod protokołem HTTPS.");
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
  btnConnect.innerText = "BLE Connect";
  btnConnect.classList.replace("btn-stop", "btn-primary");
  dotStatus.className = "pulse-offline dot-margin";
  sysState.className = "sys-state state-offline";
  sysState.innerText = "Link Offline... Złącz BLE by rozpocząć.";
}

// ==========================================
// 2. ODBIÓR I PARSOWANIE DANYCH
// ==========================================
function handleNotifications(event) {
  const value = event.target.value;
  const chunk = textDecoder.decode(value);
  receiveBuffer += chunk;

  // ESP32 każdą ramkę JSON kończy znakiem \n
  let lines = receiveBuffer.split("\n");
  
  // Zachowaj powrotem w buforze ewentualną niefinalną końcówkę (jeśli MTU ucięło)
  receiveBuffer = lines.pop(); 

  for (let line of lines) {
    parseTelemetryJSON(line);
  }
}

function parseTelemetryJSON(jsonStr) {
  try {
    const data = JSON.parse(jsonStr);
    
    // Gdy zawnioskowaliśmy o konfigurację
    if (data.type === "config") {
      document.getElementById('rpmLimit').value = data.rpm;
      updateVal('rpmVal', data.rpm);
      document.getElementById('brightness').value = data.bright;
      updateVal('brtVal', data.bright);
      document.getElementById('ecoMode').checked = data.eco;
      document.getElementById('buzzerMode').checked = data.buzzer;
      return;
    }

    // Aktualizacja podstawowych wskaźników
    document.getElementById('speedDisplay').innerText = data.speed;
    document.getElementById('tempDisplay').innerText = data.temp === 999 ? "--" : data.temp;

    t_rpm = data.rpm !== undefined ? data.rpm : 0;
    
    // Load = symulowany, jeśli nie obsługiwany, albo przesyłany z ECU
    t_load = data.load !== undefined ? data.load : Math.min(100, (t_rpm / 6000) * 100 + Math.random()*8);
    t_volt = data.volt !== undefined ? data.volt : (13.8 + (Math.random()*0.3 - 0.15));

    document.getElementById('valLoad').innerText = Math.round(t_load) + "%";
    document.getElementById('valVolt').innerText = t_volt.toFixed(1) + "V";

    if (data.tps !== undefined) {
      t_tps = data.tps; 
      document.getElementById('valTps').innerText = Math.round(t_tps) + "%";
      document.getElementById('valIat').innerText = data.iat !== 999 ? data.iat : "--";
      document.getElementById('valMap').innerText = data.map;
      t_fuel = data.fuel; 
      document.getElementById('valFuel').innerText = Math.round(t_fuel) + "%";
    }

    // Logowanie flash
    let ind = document.getElementById('logIndicator');
    let btnS = document.getElementById('btnLogStart');
    if (ind && data.log !== undefined) {
      if (data.log === true || data.log === "true") {
        ind.classList.add('active');
        btnS.classList.add('active');
      } else {
        ind.classList.remove('active');
        btnS.classList.remove('active');
      }
    }

    // Oblicz przeciążenia
    calculateG(data.speed);

    // Pasek stanu ogólnego
    let ecoEnabled = document.getElementById('ecoMode').checked;
    if (data.temp === 999) {
      sysState.className = "sys-state state-offline";
      sysState.innerText = "⏳ Oczekiwanie Na Dane ECU (CAN)...";
    } else if (ecoEnabled) {
      sysState.className = "sys-state state-eco";
      sysState.innerText = "🌿 Eco Drive Aktywny";
    } else if (data.temp < 75) {
      sysState.className = "sys-state state-cold";
      let limit = Math.round(parseInt(document.getElementById('rpmLimit').value) * 0.5);
      sysState.innerText = "⚠ Silnik Zimny — Ochrona (" + limit + " RPM)";
    } else {
      sysState.className = "sys-state state-normal";
      sysState.innerText = "✓ System Online — Normal";
    }

    // Timer Drag
    let timerStatus = document.getElementById('timerStatus');
    let timerDisp = document.getElementById('timeDisplay');
    if (data.mode <= 1) {
      timerStatus.innerText = "STANDBY";
      timerStatus.style.color = "var(--blue)";
    }
    else if (data.mode === 2) {
      timerStatus.innerText = "COUNTDOWN...";
      timerStatus.style.color = "var(--blue)";
    }
    else if (data.mode === 3) {
      timerStatus.innerText = ">>> LAUNCH READY <<<";
      timerStatus.style.color = "var(--blue)";
      timerDisp.innerText = "0.00s";
    }
    else if (data.mode === 4) {
      timerStatus.innerText = "TRACKING...";
      timerStatus.style.color = "var(--blue)";
      timerDisp.innerText = (data.current_time / 1000.0).toFixed(2) + "s";
    }
    else if (data.mode === 5) {
      timerStatus.innerText = "FINISHED";
      timerStatus.style.color = "var(--green)";
      timerDisp.innerText = (data.time / 1000.0).toFixed(2) + "s";
    }

  } catch (e) {
    console.error("Błąd parsowania układu JSON:", e);
  }
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

// Interakcje UI (Dawne fetch('/start0100') zamienione na sendCmd)
function saveSettings() {
  let r = document.getElementById('rpmLimit').value;
  let b = document.getElementById('brightness').value;
  let eco = document.getElementById('ecoMode').checked ? 1 : 0;
  let buz = document.getElementById('buzzerMode').checked ? 1 : 0;
  maxRpm = parseInt(r) + 500;
  
  let btn = document.getElementById('btnSave');
  btn.innerText = "TX DATA...";
  
  sendCmd(`cmd:save:${r}:${b}:${eco}:${buz}`).then(success => {
    let stat = document.getElementById('status');
    if (success) {
      stat.innerText = "✓ ZAPISANO Z URZĄDZENIEM";
      stat.style.color = "var(--green)";
    } else {
      stat.innerText = "✕ BŁĄD KOMUNIKACJI";
      stat.style.color = "var(--red)";
    }
    btn.innerText = "APPLY CONFIGURATION";
    setTimeout(() => stat.innerText = "", 3000);
  });
}

function startLogging() { sendCmd("cmd:log_start"); }
function stopLogging() { sendCmd("cmd:log_stop"); }
// Pobieranie dalej wymagało połączenia sieciowego lub mas-storage. Można to wyłączyć lub skierować do info.

function cancelTimer() {
  sendCmd("cmd:cancel0100");
  document.getElementById('timerStatus').innerText = "ABORTING...";
  document.getElementById('timerStatus').style.color = "var(--red)";
  document.getElementById('countdownOverlay').style.display = 'none';
}

function startTimer() {
  sendCmd("cmd:start0100");
  showCountdownOverlay();
}

function updateVal(id, val) {
  document.getElementById(id).innerText = val;
  if (id === 'rpmVal') maxRpm = parseInt(val) + 500;
}

// ==========================================
// 4. GRAFIKI I CANVAS Y (RENDER LOOP)
// ==========================================

/* COUNTDOWN OVERLAY */
function showCountdownOverlay() {
  let overlay = document.getElementById('countdownOverlay');
  let num = document.getElementById('countdownNum');
  overlay.style.display = 'flex';
  overlay.style.opacity = '1';
  num.style.color = "var(--red)";
  num.style.textShadow = "0 0 80px rgba(255,71,87,0.6), 0 0 160px rgba(255,71,87,0.2)";
  num.innerText = "5";
  setTimeout(() => num.innerText = "4", 1000);
  setTimeout(() => num.innerText = "3", 2000);
  setTimeout(() => num.innerText = "2", 3000);
  setTimeout(() => num.innerText = "1", 4000);
  setTimeout(() => {
    num.innerText = "GO!";
    num.style.color = "var(--green)";
    num.style.textShadow = "0 0 80px rgba(46,213,115,0.6), 0 0 160px rgba(46,213,115,0.2)";
    setTimeout(() => {
      overlay.style.opacity = '0';
      setTimeout(() => overlay.style.display = 'none', 300);
    }, 800);
  }, 5000);
}

/* WYZNACZANIE PRZECIĄŻEŃ */
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

/* MALOWANIE TELEMETRII */
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

/* SILNIK GRAFICZNY PROCESUJĄCY KĄTY ZEGARÓW */
function renderCanvas() {
  c_rpm += (t_rpm - c_rpm) * 0.15;
  c_load += (t_load - c_load) * 0.15;
  c_volt += (t_volt - c_volt) * 0.15;
  c_tps += (t_tps - c_tps) * 0.15;
  c_fuel += (t_fuel - c_fuel) * 0.15;

  let cRpm = document.getElementById("gaugeCanvas");
  if (cRpm) {
    let ctx = cRpm.getContext("2d");
    let w = cRpm.width, h = cRpm.height;
    ctx.clearRect(0, 0, w, h);
    let cx = w / 2, cy = h - 20, r = h - 40;
    
    // Obwód tła
    ctx.beginPath(); ctx.arc(cx, cy, r, Math.PI, 0);
    ctx.lineWidth = 18; ctx.strokeStyle = 'var(--bg-card-alt)'; ctx.stroke();
    
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
      // Stroboskop
      if (c_rpm >= activeLimit) {
        let blink = (Math.floor(Date.now() / 80) % 2 === 0);
        strokeCol = blink ? 'var(--red)' : '#1a0000';
        shadowCol = blink ? 'var(--red)' : 'transparent';
      } else if (isEco) {
        if (c_rpm < 1500) strokeCol = 'var(--green)';
        else if (c_rpm < 2000) strokeCol = 'var(--yellow)';
        else strokeCol = 'var(--red)';
        shadowCol = strokeCol;
      } else if (isCold) {
        strokeCol = 'var(--blue)'; shadowCol = strokeCol;
      } else {
        // Normalny gradient
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
    
    // Obroty na środku
    ctx.fillStyle = 'var(--text-1)'; ctx.font = 'bold 52px "Segoe UI"'; ctx.textAlign = 'center';
    ctx.fillText(Math.round(c_rpm), cx, cy - 10);
    ctx.fillStyle = 'var(--text-3)'; ctx.font = 'bold 14px "Segoe UI"';
    ctx.fillText("RPM", cx, cy + 12);
  }

  // Mini paski
  function drawMini(canvasId, valFill, colorBase) {
    let cnv = document.getElementById(canvasId);
    if (!cnv) return;
    let ctx = cnv.getContext("2d"), w = cnv.width, h = cnv.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = 'var(--bg-card-alt)'; ctx.fillRect(0, 0, w, h);
    let fill = Math.max(0, Math.min(valFill, 1.0));
    ctx.fillStyle = colorBase; ctx.shadowBlur = 6; ctx.shadowColor = colorBase;
    ctx.fillRect(0, 0, w * fill, h); ctx.shadowBlur = 0;
  }

  drawMini("loadCanvas", c_load / 100, '#ff6348');
  drawMini("voltCanvas", (c_volt - 10) / 5, '#2ed573');
  drawMini("tpsCanvas", c_tps / 100, '#ffa502');
  drawMini("fuelCanvas", c_fuel / 100, '#1e90ff');

  // G-Force Telemetryczny Render
  let cTel = document.getElementById("telemetryCanvas");
  if (cTel) {
    let ctx = cTel.getContext("2d");
    let w = cTel.width, h = cTel.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = "var(--bg-card-alt)"; ctx.fillRect(0, 0, w, h);
    
    let baseLine = h - 5;
    let maxG = 1.5;
    let step = w / (gHistory.length - 1);
    
    ctx.strokeStyle = "var(--border)"; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(0, baseLine); ctx.lineTo(w, baseLine); ctx.stroke();
    
    drawMountain(ctx, true, "var(--green)", "rgba(46,213,115,0.2)", w, baseLine, maxG, step, gHistory);
    drawMountain(ctx, false, "var(--red)", "rgba(255,71,87,0.2)", w, baseLine, maxG, step, gHistory);
    
    let currentG = gHistory[gHistory.length - 1];
    ctx.fillStyle = currentG > 0.05 ? "var(--green)" : (currentG < -0.05 ? "var(--red)" : "var(--text-3)");
    ctx.font = 'bold 13px "Segoe UI"'; ctx.textAlign = 'right';
    let title = currentG > 0.05 ? "ACCEL " : (currentG < -0.05 ? "BRAKE " : "IDLE ");
    ctx.fillText(title + Math.abs(currentG).toFixed(2) + "G", w - 10, 20);
  }
  
  requestAnimationFrame(renderCanvas);
}

// Inicjalizacja zegarów
document.addEventListener("DOMContentLoaded", () => {
  maxRpm = parseInt(document.getElementById('rpmLimit').value) + 500;
  requestAnimationFrame(renderCanvas);
  
  // Zastąpmy logikę Export CSV ostrzeżeniem (Web Bluetooth nie dźwiga dużych plików)
  let btnExp = document.querySelector('.btn-export');
  if(btnExp) {
    btnExp.onclick = () => alert("Pobieranie logów CSV odbywa się wyłącznie bezpośrednio z pamięci plikowej ESP podłączając płytkę po kablu USB. Bluetooth używasz do odpalania rec!");
  }
});
