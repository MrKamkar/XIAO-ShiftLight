#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <FastLED.h>
#include "config.h"
#include "ble_server.h"
#include "data_logger.h"
#include <LittleFS.h>

extern Preferences preferences;

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic = NULL;
BLECharacteristic *pRxCharacteristic = NULL;

bool isDownloadingFile = false;
File fileToDownload;
uint32_t fileDownloadSize = 0;

// Wyzwalane przy parowaniu/rozłączeniu
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConnected = true;
    lastBLEActivity = millis();
    // Odświeżamy timer, żeby nie zablokował CAN busa
  }

  void onDisconnect(BLEServer* pServer) {
    bleConnected = false;
    // Ponownie aktywujemy rozgłaszanie, by urządzenie było znów widoczne
    pServer->startAdvertising();
  }
};

// Wyzwalane gdy strona internetowa wyśle ciąg znaków
class MyRxCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String cmd = pCharacteristic->getValue();
    
    if (cmd.length() > 0) {
      cmd.trim(); // Usunięcie \n, \r, itp.
      
      lastBLEActivity = millis(); // Odświeżenie znacznika aktywności

      if (cmd.startsWith("cmd:save:")) {
        // Parsowanie stringa cmd:save:6000:50:1:1
        int s1 = cmd.indexOf(':', 9);
        int s2 = cmd.indexOf(':', s1+1);
        int s3 = cmd.indexOf(':', s2+1);
        
        if (s1 > 0 && s2 > 0 && s3 > 0) {
          shiftLimit = cmd.substring(9, s1).toInt();
          brightness = cmd.substring(s1+1, s2).toInt();
          ecoMode = (cmd.substring(s2+1, s3).toInt() == 1);
          buzzerEnabled = (cmd.substring(s3+1).toInt() == 1);
          
          preferences.begin("shiftlight", false);
          preferences.putInt("rpm", shiftLimit);
          preferences.putInt("bright", brightness);
          preferences.putBool("eco", ecoMode);
          preferences.putBool("buzzer", buzzerEnabled);
          preferences.end();
          
          FastLED.setBrightness(brightness);
          currentMode = MODE_LED_TEST; // Wyzwalanie zielonego błysku potwierdzenia
        }
      } 
      else if (cmd == "cmd:start0100") {
        currentMode = MODE_0_100_COUNTDOWN;
      } 
      else if (cmd == "cmd:cancel0100") {
        currentMode = MODE_SHIFT_LIGHT;
      } 
      else if (cmd == "cmd:log_start") {
        startDataLog();
      } 
      else if (cmd == "cmd:log_stop") {
        stopDataLog();
      } 
      else if (cmd == "cmd:req_conf") {
        // Aplikacja (app.js) zażądała wgrania aktualnej konfiguracji z ESP32 tuż po połączeniu.
        char confBuf[192];
        size_t total = LittleFS.totalBytes();
        size_t used = LittleFS.usedBytes();
        snprintf(confBuf, sizeof(confBuf), "{\"type\":\"config\",\"rpm\":%d,\"bright\":%d,\"eco\":%s,\"buzzer\":%s,\"f_used\":%u,\"f_total\":%u}\n",
                 shiftLimit, brightness, ecoMode ? "true" : "false", buzzerEnabled ? "true" : "false", used, total);
        pTxCharacteristic->setValue((uint8_t*)confBuf, strlen(confBuf));
        pTxCharacteristic->notify();
      }
      else if (cmd == "cmd:log_dl") {
        if (!isDownloadingFile) {
          if (isLogging) stopDataLog(); // Zatrzymanie obecnego nagrywania
          
          fileToDownload = LittleFS.open("/telemetry.bin", "r");
          if (fileToDownload) {
            fileDownloadSize = fileToDownload.size();
            isDownloadingFile = true;
            
            char confBuf[64];
            snprintf(confBuf, sizeof(confBuf), "{\"type\":\"download_start\",\"size\":%u}\n", fileDownloadSize);
            pTxCharacteristic->setValue((uint8_t*)confBuf, strlen(confBuf));
            pTxCharacteristic->notify();
          } else {
             // Brak pliku
            pTxCharacteristic->setValue((uint8_t*)"{\"type\":\"download_error\"}\n", 25);
            pTxCharacteristic->notify();
          }
        }
      }
    }
  }
};

void setupBLEServer() {
  BLEDevice::init("XIAO_ShiftLight");
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(BLE_SERVICE_UUID);

  // Charakterystyka TX (Notify - stąd dane płyną na zewnątrz)
  pTxCharacteristic = pService->createCharacteristic(
                        BLE_TX_CHAR_UUID,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  pTxCharacteristic->addDescriptor(new BLE2902()); // Niezbędny by telefony radziły sobie z Notification

  // Charakterystyka RX (Write - tędy przeglądarka wysyła komendy do nas)
  pRxCharacteristic = pService->createCharacteristic(
                         BLE_RX_CHAR_UUID,
                         BLECharacteristic::PROPERTY_WRITE
                       );
  pRxCharacteristic->setCallbacks(new MyRxCallbacks());

  pService->start();
  
  // Sprawienie, że urządzenie będzie widoczne przy skanowaniu.
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // Minimalizacja interwału dla iOS (min)
  pAdvertising->setMaxPreferred(0x12); // Maksymalizacja interwału dla iOS (max)
  BLEDevice::startAdvertising();
}

// Funkcja wywoływana cyklicznie przez rdzeń 0, wysyłająca paczkę BINARNĄ przez BLE
void sendBLETelemetry() {
  if (!bleConnected || pTxCharacteristic == NULL) return;

  if (isDownloadingFile) {
    if (fileToDownload && fileToDownload.available()) {
      uint8_t buffer[240]; // Bezpieczny przedział dla MTU (domyślnie ~244b iOS/Android z negocjacjami)
      buffer[0] = 0xCC;    // Prefiks pliku
      
      size_t bytesRead = fileToDownload.read(buffer + 1, sizeof(buffer) - 1);
      if (bytesRead > 0) {
        pTxCharacteristic->setValue(buffer, bytesRead + 1);
        pTxCharacteristic->notify();
        delay(15); // Oddech dla stosu BLE - zapobiega zapchaniu (queue overflow)
      }
    } else {
      // Koniec wysyłania
      if (fileToDownload) fileToDownload.close();
      isDownloadingFile = false;
      
      const char* endMsg = "{\"type\":\"download_end\"}\n";
      pTxCharacteristic->setValue((uint8_t*)endMsg, strlen(endMsg));
      pTxCharacteristic->notify();
    }
    return; // Pomiń wysyłanie telemetrii
  }

  // Tworzymy paczkę binarną (30 bajtów) zamiast 300 bajtów JSON
  static TelemetryPacket pkg;
  static uint32_t lastFSUpdate = 0;
  
  if (isLogging && (millis() - lastFSUpdate > 5000)) {
    lastFSUpdate = millis();
    char fsBuf[128];
    snprintf(fsBuf, sizeof(fsBuf), "{\"type\":\"fs_stat\",\"f_used\":%u,\"f_total\":%u}\n", (uint32_t)LittleFS.usedBytes(), (uint32_t)LittleFS.totalBytes());
    pTxCharacteristic->setValue((uint8_t*)fsBuf, strlen(fsBuf));
    pTxCharacteristic->notify();
  }
  
  pkg.mode = (uint8_t)currentMode;
  pkg.speed = (uint8_t)currentSpeed;
  pkg.temp = (int16_t)currentTemp;
  pkg.load = (uint8_t)currentLoad;
  pkg.volt = currentVolt;
  pkg.rpm = (uint16_t)currentRPM;
  pkg.timerResult = timerResult;
  pkg.currentTime = (currentMode == MODE_0_100_MEASURING) ? (millis() - dragTimerStart) : 0;
  pkg.iat = (int8_t)currentIAT;
  pkg.tps = (uint8_t)currentTPS;
  pkg.map = (uint16_t)currentMAP;
  pkg.fuel = (uint8_t)currentFuel;
  pkg.gforce = currentGForce;
  pkg.logging = isLogging ? 1 : 0;
  pkg.hz = (uint8_t)obdHz;
           
  // Wysyłka surowych bajtów struktury
  pTxCharacteristic->setValue((uint8_t*)&pkg, sizeof(TelemetryPacket));
  pTxCharacteristic->notify();
}
