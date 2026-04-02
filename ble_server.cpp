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
uint32_t fileDownloadOffset = 0;

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
      else if (cmd == "cmd:fs_wipe") {
        wipeFilesystem();
        pTxCharacteristic->setValue("fs_formatted");
        pTxCharacteristic->notify();
      }
      else if (cmd == "cmd:log_start") {
        if (!isLogging) {
          // Natychmiastowa inicjalizacja bazy zajętości przed sesją
          if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
            size_t total = LittleFS.usedBytes();
            if (LittleFS.exists("/telemetry.bin")) {
              File f = LittleFS.open("/telemetry.bin", "r");
              size_t fSize = f.size();
              f.close();
              if (total >= fSize) total -= fSize; // Odejmujemy stary plik, bo "w" go nadpisze
            }
            baseUsedBytes = total;
            currentFileSize = 0;
            xSemaphoreGive(fsMutex);
          }
        }
        startDataLog();
      } 
      else if (cmd == "cmd:log_stop") {
        stopDataLog();
      } 
      else if (cmd == "cmd:req_conf") {
        char confBuf[192];
        size_t total = 0, used = 0;
        if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
          total = LittleFS.totalBytes();
          if (isLogging) {
            used = baseUsedBytes + currentFileSize;
          } else {
            used = LittleFS.usedBytes();
          }
          xSemaphoreGive(fsMutex);
        }
        snprintf(confBuf, sizeof(confBuf), "{\"type\":\"config\",\"rpm\":%d,\"bright\":%d,\"eco\":%s,\"buzzer\":%s,\"f_used\":%u,\"f_total\":%u}\n",
                 shiftLimit, brightness, ecoMode ? "true" : "false", buzzerEnabled ? "true" : "false", (uint32_t)used, (uint32_t)total);
        pTxCharacteristic->setValue((uint8_t*)confBuf, strlen(confBuf));
        pTxCharacteristic->notify();
      }
      else if (cmd == "cmd:log_dl") {
        if (!isDownloadingFile) {
          if (isLogging) stopDataLog(); // Zatrzymanie obecnego nagrywania
          
          if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
            fileToDownload = LittleFS.open("/telemetry.bin", "r");
            if (fileToDownload) {
              fileDownloadSize = fileToDownload.size();
              fileDownloadOffset = 0;
              isDownloadingFile = true;
            }
            xSemaphoreGive(fsMutex);
          }

          if (isDownloadingFile) {
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
    if (isDownloadingFile && fileToDownload) {
      uint8_t chunk[240];
      size_t toRead = std::min((size_t)240, (size_t)(fileDownloadSize - fileDownloadOffset));
      
      size_t actuallyRead = 0;
      if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
         actuallyRead = fileToDownload.read(chunk, toRead);
         xSemaphoreGive(fsMutex);
      }
      
      if (actuallyRead > 0) {
        pTxCharacteristic->setValue(chunk, actuallyRead);
        pTxCharacteristic->notify();
        fileDownloadOffset += actuallyRead;
        
        if (fileDownloadOffset >= fileDownloadSize) {
          if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
            fileToDownload.close();
            xSemaphoreGive(fsMutex);
          }
          isDownloadingFile = false;
          fileDownloadOffset = 0;
          
          const char* endMsg = "{\"type\":\"download_end\"}\n";
          pTxCharacteristic->setValue((uint8_t*)endMsg, strlen(endMsg));
          pTxCharacteristic->notify();
        }
      }
    }
    return; // Pomiń wysyłanie telemetrii
  }

  // Tworzymy paczkę binarną (30 bajtów) zamiast 300 bajtów JSON
  static TelemetryPacket pkg;
  static uint32_t lastFSUpdate = 0;
  static bool prevLogging = false;
  static uint32_t stopTimer = 0;

  // Detekcja zmiany stanu logowania (dla wymuszenia wysyłki statusu Flash)
  bool startTrigger = (isLogging && !prevLogging); 
  if (isLogging) {
    stopTimer = 0; // Każdy START natychmiast anuluje stare raporty STOP
  } else if (prevLogging) {
    stopTimer = millis() + 300; // Planujemy finalny status za 300ms tylko przy przejściu na STOP
  }

  // Logika wysyłki statusu Flash
  bool triggerUpdate = startTrigger;
  if (isLogging && (millis() - lastFSUpdate > 5000)) triggerUpdate = true;
  if (stopTimer > 0 && millis() >= stopTimer && !isLogging) {
    triggerUpdate = true;
    stopTimer = 0; 
  }

  if (triggerUpdate) {
    lastFSUpdate = millis();
    char fsBuf[128];
    size_t total = 0, used = 0;
    if (xSemaphoreTake(fsMutex, portMAX_DELAY)) {
      total = LittleFS.totalBytes();
      if (isLogging) {
        used = baseUsedBytes + currentFileSize; 
      } else {
        used = LittleFS.usedBytes();
      }
      xSemaphoreGive(fsMutex);
    }
    snprintf(fsBuf, sizeof(fsBuf), "{\"type\":\"fs_stat\",\"f_used\":%u,\"f_total\":%u}\n", (uint32_t)used, (uint32_t)total);
    pTxCharacteristic->setValue((uint8_t*)fsBuf, strlen(fsBuf));
    pTxCharacteristic->notify();
  }
  
  prevLogging = isLogging; // Aktualizacja stanu na koniec
  
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
