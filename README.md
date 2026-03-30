# 🏎️ XIAO ESP32-S3 CAN Shift Light & Performance Meter

![Project Status](https://img.shields.io/badge/Status-Active-brightgreen)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange)
![Framework](https://img.shields.io/badge/Framework-Arduino-blue)

A high-performance, dual-core automotive telemetry system built on the **Seeed Studio XIAO ESP32-S3**. This device connects to your car's **CAN Bus (OBD-II)** to provide real-time shift light animations, performance tracking (0-100 km/h), and deep telemetry logging.

---

## 🌟 Key Features

- **🚀 Real-time Shift Light**: 8x WS2812B RGB LEDs with smooth FastLED animations. Customizable RPM limits and brightness.
- **📊 Advanced Web Dashboard**: Integrated Wi-Fi Access Point hosting a modern, responsive HTML5 dashboard with:
  - Real-time Gauges (RPM, Speed, Load, Battery, TPS, MAP, Fuel).
  - **G-Force Telemetry**: Live acceleration/braking graphs.
  - **Performance Timer**: 0-100 km/h measurement with countdown sequence.
- **💾 Flash Data Logger**: Log all telemetry data to internal Flash (LittleFS) and export as **CSV** via the web interface.
- **⚡ Dual-Core Optimization**: 
  - **Core 0**: Handles CAN Bus communication, Web Server, and Data Logging.
  - **Core 1**: Dedicated to ultra-smooth LED animations (FastLED).
- **🔊 Audio Feedback**: Integrated buzzer for shift point alerts.
- **🌿 Eco & Cold Start Protection**: Special LED modes for fuel efficiency and engine warm-up protection.

---

## 🛠️ Hardware Requirements

- **Microcontroller**: [Seeed Studio XIAO ESP32-S3](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html)
- **CAN Transceiver**: SN65HVD230 or equivalent (connecting to ESP32-S3 TWAI pins).
- **LEDs**: 8x WS2812B (Neopixel) RGB LED strip/bar.
- **Buzzer**: Active or Passive buzzer (connected to D2).
- **OBD-II Connector**: To interface with the vehicle's CAN-HI and CAN-LO.

### Pinout (XIAO ESP32-S3)
| Component | Pin | Description |
|-----------|-----|-------------|
| **LED Data** | `D1` | WS2812B DIN |
| **Buzzer** | `D2` | Audio Signal |
| **CAN RX** | `D4` | CAN Receiver |
| **CAN TX** | `D5` | CAN Transmitter |

### Default Settings
- **CAN Bus Speed**: 500 kbits/s (ISO 15765-4)
- **WiFi SSID**: `Kamcore_ShiftLight`
- **WiFi Password**: `kamcore_drive`
- **IP Address**: `192.168.4.1`

---

## 🚀 Getting Started

### 1. Prerequisites
- [Arduino IDE](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/).
- Install the following libraries:
  - `FastLED`

### 2. Configuration
Open `config.h` to adjust:
- `NUM_LEDS`: Number of LEDs in your strip (default: 8).
- `DEFAULT_SHIFT_LIMIT`: Your preferred shift point RPM.
- WiFi settings (defaults to AP mode).

### 3. Upload
1. Select **XIAO ESP32-S3** as your board.
2. Enable **PSRAM** (if available) and **LittleFS** partitioning.
3. Flash the code.

---

## 📱 How to Use

1. **Installation**: Connect the device to your car's OBD-II port (VCC 5V/3.3V, GND, CAN-H, CAN-L).
2. **WiFi Dashboard**: Connect your phone/laptop to the WiFi network (e.g., `ESP32-Shift-Light`).
3. **Open Browser**: Navigate to `http://192.168.4.1` to access the dashboard.
4. **Configuration**: Use the web UI to set your RPM limits, brightness, and toggle logging.
5. **Launch!**: Use the "Initiate Launch Sequence" button for a 0-100 km/h timed run.

---

## 📂 Project Structure

- `XIAO_ShiftLight_Code.ino`: Main entry point & task initialization.
- `config.h`: Central hardware & software configuration.
- `can_bus.cpp/h`: Low-level TWAI (CAN) driver implementation.
- `led_controller.cpp/h`: Animation engine and LED logic.
- `web_server.cpp/h`: Async web server & WebSocket-like updates.
- `data_logger.cpp/h`: LittleFS management and CSV export logic.
- `core_tasks.cpp/h`: FreeRTOS task definitions for dual-core execution.

---

## 📄 License
Copyright © 2024. All rights reserved. Proprietary software.

---
*(Polish Description / Opis po polsku)*

### O projekcie
Zaawansowany system Shift Light oparty na XIAO ESP32-S3, wykorzystujący magistralę CAN (OBD2) do pobierania danych prosto z komputera silnika (ECU). Urządzenie posiada wbudowany panel dashboard dostępny przez Wi-Fi, pomiar przyspieszenia 0-100 km/h oraz logowanie danych telemetrycznych do pamięci Flash.
