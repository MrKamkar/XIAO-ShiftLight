#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <FastLED.h>
#include "config.h"
#include "web_server.h"
#include "index_html.h"
#include <LittleFS.h>
#include "data_logger.h"

extern Preferences preferences;
WebServer server(80); // Tworzy obiekt serwera WWW na porcie 80

 // Wczytuje HTML z pliku index_html.h i podmienia znaczniki na aktualne wartości zmiennych
void handleRoot() {
  String html = String(INDEX_HTML);
  html.replace("%RPM_LIMIT%", String(shiftLimit));
  html.replace("%BRIGHTNESS%", String(brightness)); 
  html.replace("%ECO_MODE_CHECKED%", ecoMode ? "checked" : ""); 
  html.replace("%BUZZER_CHECKED%", buzzerEnabled ? "checked" : ""); 
  server.send(200, "text/html", html);
}

// Zapisuje ustawienia do pamięci NVS i fizycznie aktualizuje stan urządzenia po odebraniu sygnału z panelu WWW
void handleSave() {
  if (server.hasArg("rpm")) shiftLimit = server.arg("rpm").toInt();
  if (server.hasArg("bright")) brightness = server.arg("bright").toInt();
  if (server.hasArg("eco")) ecoMode = (server.arg("eco").toInt() == 1);
  if (server.hasArg("buzzer")) buzzerEnabled = (server.arg("buzzer").toInt() == 1);
  
  preferences.begin("shiftlight", false); // Parametr 'false' przełącza NVS w tryb zapisu
  preferences.putInt("rpm", shiftLimit);
  preferences.putInt("bright", brightness);
  preferences.putBool("eco", ecoMode);
  preferences.putBool("buzzer", buzzerEnabled);
  preferences.end();
  
  FastLED.setBrightness(brightness); // Ustawia główną jasność całego paska LED
  server.send(200, "text/plain", "OK");
}

// Uruchamia procedurę pomiaru przyspieszenia (0-100 km/h)
void handleAccelerationTest() {
  currentMode = MODE_0_100_COUNTDOWN;
  server.send(200, "text/plain", "OK");
}

// Zwraca status i wyniki pomiaru przyspieszenia (w formacie JSON)
void handleAccelerationStatus() {
  lastWebPing = millis(); // Odświeża status sesji, co zdejmuje filtrację zapytań CAN
  
  char jsonBuffer[350]; // Bufor na wszystkie potrzebne dane JSON
  uint32_t cTime = (currentMode == MODE_0_100_MEASURING) ? (millis() - dragTimerStart) : 0; // Oblicza czas rzeczywisty (delta) od startu pomiaru do teraz
  
  // Zapisanie wszystkich wartości ulotnych ze środowiska pracy Kernelowego w jeden elegancki format
  snprintf(jsonBuffer, sizeof(jsonBuffer),
           "{\"mode\":%d,\"speed\":%d,\"temp\":%d,\"load\":%d,\"volt\":%.1f,\"rpm\":%d,\"time\":%lu,\"current_time\":%lu,\"iat\":%d,\"tps\":%d,\"map\":%d,\"fuel\":%d,\"log\":%s}",
           currentMode, currentSpeed, currentTemp, currentLoad, currentVolt, currentRPM, timerResult, cTime, currentIAT, currentTPS, currentMAP, currentFuel, isLogging ? "true" : "false");
           
  server.send(200, "application/json", jsonBuffer); // Wysłanie danych w formacie JSON do przeglądarki
}

void setupWebServer() {
  server.on("/", handleRoot); // Wyświetla stronę główną
  server.on("/save", handleSave); // Zapisuje ustawienia
  server.on("/start0100", handleAccelerationTest); // Rozpoczyna pomiar przyspieszenia
  server.on("/status0100", handleAccelerationStatus); // Zwraca status pomiaru przyspieszenia

  // API DATA LOGGERA
  server.on("/log/start", HTTP_POST, []() {
    startDataLog();
    if (isLogging) {
      server.send(200, "text/plain", "OK");
    } else {
      server.send(500, "text/plain", "Flash Write Error");
    }
  });
  
  server.on("/log/stop", HTTP_POST, []() {
    stopDataLog(); 
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/log/download", HTTP_GET, []() {
    File file = LittleFS.open("/telemetry.csv", "r");
    if (!file) {
      server.send(404, "text/plain", "No logs found");
      return;
    }
    // Wymusza pobranie pliku w przeglądarce po kliknięciu przycisku "Pobierz logi"
    server.sendHeader("Content-Disposition", "attachment; filename=\"XIAO_telemetry.csv\"");
    server.streamFile(file, "text/csv");
    file.close();
  });

  server.begin(); // Uruchamia serwer WWW
}

void handleWebServerClient() {
  // Nasłuchuje pakietów HTTP i kieruje je do odpowiednich handlerów
  server.handleClient(); 
}