#include <Arduino.h>
#include <FastLED.h>
#include "config.h"
#include "led_controller.h"

CRGB leds[NUM_LEDS]; // Tablica do przechowywania aktualnych kolorów diod
CRGB targetLeds[NUM_LEDS]; // Tablica do przechowywania docelowych kolorów diod

// Paleta kolorów dla 8 diod LED (od zimnego niebieskiego do gorącego czerwonego)
CRGB palette[NUM_LEDS] = {
  CRGB::Blue,
  CRGB::Blue,
  CRGB::Green,
  CRGB::Green,
  CRGB::Yellow,
  CRGB::Orange,
  CRGB::Red,
  CRGB::Red
};

// Inicjalizacja diod LED i buzzera
void setupLEDs() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Wyłączenie buzzera aktywnego na starcie (brak prądu na bazie tranzystora)
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear(); // Wyczyść matrycę LED przed startem
  FastLED.show(); // Wyświetl nową czarną matrycę LED
}

// Obsługa sekwencji odliczania przed startem
void handleCountdownSequence(uint32_t now) {
  static int countdownStep = 0;
  static uint32_t countdownTimer = 0;
  
  // Zapisanie czasu startu odliczania (punkt zerowy dla stopera)
  if (countdownStep == 0) { 
    countdownTimer = now; 
    countdownStep = 1; 
  }
  
  uint32_t elapsed = now - countdownTimer; // Czas, który upłynął od startu
  FastLED.clear(); 
  
  int currentSecond = elapsed / 1000; // Licznik sekund (0, 1, 2, 3)
  
  if (currentSecond < 3) {
    // Zapalanie czerwonych diod parami od środka na zewnątrz
    int numPairs = currentSecond + 1; 
    for (int i = 0; i < numPairs; i++) {
      leds[3 - i] = CRGB::Red;
      leds[4 + i] = CRGB::Red;
    }
    
    // Krótki sygnał dźwiękowy na początku każdej sekundy
    bool beep = (elapsed % 1000 < 200);
    digitalWrite(BUZZER_PIN, (buzzerEnabled && beep) ? HIGH : LOW);
  } 
  else if (currentSecond < 4) {
    // Sygnał do startu (wszystkie diody na zielono)
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    bool longBeep = (elapsed % 1000 < 800);
    digitalWrite(BUZZER_PIN, (buzzerEnabled && longBeep) ? HIGH : LOW);
  } 
  else {
    // Koniec odliczania i powrót do trybu oczekiwania na start
    digitalWrite(BUZZER_PIN, LOW);
    currentMode = MODE_0_100_WAITING_FOR_LAUNCH; 
    countdownStep = 0; // Reset zatrzasku dla ponownego użycia
  }
  
  FastLED.show();
}

// Logika wyświetlania obrotów (Shift Light)
void handleShiftLightLogic(uint32_t now) {

  // Pamięć dla efektu mrugania (stroboskopu)
  static bool flashState = false;
  static uint32_t lastFlash = 0;

  // Ochrona zimnego silnika (ustawienie odpowiedniego limitu obrotów)
  bool isCold = (currentTemp < 75 && currentTemp != 999); 
  int activeLimit = shiftLimit;
  
  if (ecoMode) activeLimit = shiftLimit * 0.4;     // Tryb ECO to 40% głównych obrotów (np. 1600 dla diesla, 2400 dla benzyny)
  else if (isCold) activeLimit = shiftLimit * 0.5; // Zimny Silnik to 50% głównych obrotów (Idealna dynamika między dieslem a benzyną)

  if (currentRPM >= activeLimit) {
    if (now - lastFlash > 80) { // Szybkość mrugania (stroboskop)
      flashState = !flashState;
      lastFlash = now;
    }
    if (flashState) {
      CRGB flashColor = CRGB::Red; 
      for(int i=0; i<NUM_LEDS; i++) targetLeds[i] = flashColor;
      
      // Ciągły dźwięk przy limicie obrotów (W ECO działa zawsze, w sporcie tylko po rozgrzaniu)
      if (buzzerEnabled && (ecoMode || !isCold) && activeLimit > 0) digitalWrite(BUZZER_PIN, HIGH);
      else digitalWrite(BUZZER_PIN, LOW);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  } else {
    // Różna częstość dźwięku przed zmianą biegu (W ECO działa zawsze, w sporcie tylko po rozgrzaniu)
    if (buzzerEnabled && (ecoMode || !isCold) && activeLimit > 0) {
      float percent = (float)currentRPM / (float)activeLimit;
      if (percent >= 0.95) {
        if ((now / 50) % 2 == 0) digitalWrite(BUZZER_PIN, HIGH); // Szybki rytm
        else digitalWrite(BUZZER_PIN, LOW);
      } else if (percent >= 0.85) {
        if ((now / 150) % 2 == 0) digitalWrite(BUZZER_PIN, HIGH); // Średni rytm
        else digitalWrite(BUZZER_PIN, LOW);
      } else {
        digitalWrite(BUZZER_PIN, LOW); // Brak dźwięku poniżej 85% limitu obrotów
      }
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
    
    // Logika wyświetlania obrotów (Shift Light)
    if (currentRPM > 0) {
      int startRpm = ecoMode ? 1000 : 0;
      
      float range = (float)(activeLimit - startRpm);
      if (range <= 0) range = 1;

      float rpmOffset = (float)(currentRPM - startRpm);
      if (rpmOffset < 0) rpmOffset = 0;
      
      float ledPos = (rpmOffset / range) * NUM_LEDS;
      if (ledPos > NUM_LEDS) ledPos = NUM_LEDS;

      // Ustawianie kolorów dla każdej diody z palety
      for (int i = 0; i < NUM_LEDS; i++) {
        CRGB baseColor = palette[i]; 
        
        // Specjalne kolory dla trybów
        if (ecoMode) {
          // Tryb ECO: Inne kolory diod wspomagające oszczędzanie paliwa (Priorytet nad Cold)
          if (currentRPM < 1500) baseColor = CRGB::Green;       // Zbyt niskie obroty (zmień bieg w dół)
          else if (currentRPM < 2000) baseColor = CRGB::Yellow; // Czas na zmianę biegu w górę
          else baseColor = CRGB::Red;                           // Za wysokie obroty dla trybu ECO
        } else if (isCold) {
          baseColor = CRGB::Blue; // Wymuś stały na maksa zimny niebieski, znany z zegarów BMW M-Pakiet
        }
        
        // Włączanie odpowiedniej ilości diod na pasku
        if (i < (int)ledPos) {
          targetLeds[i] = baseColor;            
        } else if (i == (int)ledPos) {
          // Płynne podświetlenie ostatniej diody, by pokazać ułamkowe wartości obrotów
          float fraction = ledPos - (int)ledPos; 
          targetLeds[i] = baseColor;
          targetLeds[i].nscale8( (uint8_t)(fraction * 255.0f) ); // Wygasza diodę proporcjonalnie do ułamka obrotów
        } else {
          targetLeds[i] = CRGB::Black;          
        }
      }
    }
  }
}

// Główna pętla sterująca diodami wywoływana na Rdzeniu 1
void taskCore1(void *pvParameters) {
  while (true) {
    uint32_t now = millis();
    // Czyścimy bufor docelowy przed nowymi obliczeniami
    for(int i = 0; i < NUM_LEDS; i++) targetLeds[i] = CRGB::Black;

    // Obsługa specjalnej sekwencji odliczania 0-100
    if (currentMode == MODE_0_100_COUNTDOWN) {
      handleCountdownSequence(now);
      vTaskDelay(pdMS_TO_TICKS(10));
      continue; 
    }

    // Normalna logika Shift Light
    handleShiftLightLogic(now);
    
    // Płynne przechodzenie kolorów (efekt wygaszania dla 100 FPSów)
    for(int i=0; i<NUM_LEDS; i++) {
      leds[i] = blend(leds[i], targetLeds[i], 40); 
    }
    
    FastLED.show();
    vTaskDelay(pdMS_TO_TICKS(10)); // Częstotliwość odświeżania diod (100Hz)
  }
}
