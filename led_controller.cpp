#include <Arduino.h>
#include <FastLED.h>
#include "config.h"
#include "led_controller.h"

CRGB leds[NUM_LEDS]; // Tablica do przechowywania aktualnych kolorów diod
CRGB targetLeds[NUM_LEDS]; // Tablica do przechowywania docelowych kolorów diod

// Paleta kolorów dla 8 diod LED (od zimnego niebieskiego do gorącego czerwonego)
const CRGB palette[NUM_LEDS] = {
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
  digitalWrite(BUZZER_PIN, LOW); 
  
  // Wymuszenie sterownika sprzętowego RMT (odpornego na przerwania BLE)
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  
  // CAŁKOWITE wyłączenie ditheringu czasowego (eliminuje szum świetlny przy małej jasności)
  FastLED.setDither(DISABLE_DITHER); 
  
  FastLED.clear(); 
  FastLED.show(); 
}

// Obsługa sekwencji odliczania przed startem (pisze do targetLeds[], blending w updateLEDs)
void handleCountdownSequence(uint32_t now) {
  static int countdownStep = 0;
  static uint32_t countdownTimer = 0;
  
  // Zapisanie czasu startu odliczania (punkt zerowy dla stopera)
  if (countdownStep == 0) { 
    countdownTimer = now;
    countdownStep = 1; 
  }
  
  uint32_t elapsed = now - countdownTimer; // Czas od startu przycisku
  int currentSecond = elapsed / 1000; // Licznik sekund (0, 1, 2, 3, 4, 5)
  
  if (currentSecond < 5) {
    // Odliczanie: 5 i 4 (s=0,1) diody ciemne, 3 (s=2) 1 para, 2 (s=3) 2 pary, 1 (s=4) 3 pary.
    int numPairs = 0;
    if (currentSecond == 2) numPairs = 1;
    else if (currentSecond == 3) numPairs = 2;
    else if (currentSecond == 4) numPairs = 3;
    
    for (int i = 0; i < numPairs; i++) {
        targetLeds[3 - i] = CRGB::Red;
        targetLeds[4 + i] = CRGB::Red;
    }
    
    // Sygnał dźwiękowy tylko dla 3... 2... 1...
    bool beep = (currentSecond >= 2 && elapsed % 1000 < 200);
    digitalWrite(BUZZER_PIN, (buzzerEnabled && beep) ? HIGH : LOW);
  } 
  else if (currentSecond < 6) {
    // Sekunda 5: Sygnał do startu (GO!) - Cały pasek zielony (8 diod) + długi sygnał
    for(int i = 0; i < NUM_LEDS; i++) targetLeds[i] = CRGB::Green;
    bool longBeep = (elapsed % 1000 < 800);
    digitalWrite(BUZZER_PIN, (buzzerEnabled && longBeep) ? HIGH : LOW);
  } 
  else {
    // Koniec sekwencji
    digitalWrite(BUZZER_PIN, LOW);
    currentMode = MODE_0_100_WAITING_FOR_LAUNCH; 
    countdownStep = 0; 
  }
}

// Logika wyświetlania obrotów (Shift Light)
void handleShiftLightLogic(uint32_t now) {

  // Pamięć dla efektu mrugania (stroboskopu)
  static bool flashState = false;
  static uint32_t lastFlash = 0;
  
  // Wygładzanie sygnału RPM (Exponential Moving Average)
  static float smoothedRPM = 0;
  smoothedRPM = (currentRPM * 0.7) + (smoothedRPM * 0.3);

  // Histereza temperatury (zapobiega migotaniu trybu zimnego przy równych 75st)
  static bool isColdState = true;
  if (currentTemp != 999) {
    if (isColdState && currentTemp >= 75) isColdState = false;      // Silnik się zagrzał
    else if (!isColdState && currentTemp < 72) isColdState = true;  // Silnik ostygł (np. długi postój)
  }
  bool isCold = (isColdState && currentTemp != 999); 
  int activeLimit = shiftLimit;
  
  if (ecoMode) activeLimit = shiftLimit * 0.4;     // Tryb ECO to 40% głównych obrotów (np. 1600 dla diesla, 2400 dla benzyny)
  else if (isCold) activeLimit = shiftLimit * 0.5; // Zimny Silnik to 50% głównych obrotów (Idealna dynamika między dieslem a benzyną)

  // Do sprawdzania limitu (stroboskopu) używamy surowych danych dla zerowego opóźnienia
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
      // Do dźwięku i paska LED używamy wygładzonych obrotów dla płynności
      float percent = smoothedRPM / (float)activeLimit;
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
    
    // Logika wyświetlania obrotów (Shift Light) na pasku LED
    if (smoothedRPM > 100) { // Próg wyświetlania, żeby pasek nie świecił na samym zapłonie
      int startRpm = 1000; // Zawsze zaczynamy od 1000 RPM, pomijamy jałowe obroty
      
      float range = (float)(activeLimit - startRpm);
      if (range <= 0) range = 1;

      float rpmOffset = smoothedRPM - (float)startRpm;
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
          targetLeds[i].nscale8( (uint8_t)(fraction * 255) ); // Wygasza diodę proporcjonalnie do ułamka obrotów
        } else {
          targetLeds[i] = CRGB::Black;          
        }
      }
    }
  }
}

// Obsługa animacji powitalnej po włączeniu zasilania (ceremonia otwarcia)
void handleWelcomeSequence(uint32_t now) {
  static uint32_t welcomeTimer = 0;
  if(welcomeTimer == 0) welcomeTimer = now;
  
  uint32_t elapsed = now - welcomeTimer;
  const uint32_t duration = 3000; // Długość animacji powitalnej

  if (elapsed < duration) {
    // Próg od 0 do 255 dla całości trwania animacji
    uint8_t pos = (elapsed * 255) / duration; 
    // Przesunięcie fali (mapujemy 0-255 na zakres od -2 do 10 dla płynnego wejścia/wyjścia)
    int16_t head = map(pos, 0, 255, -2, NUM_LEDS + 2); 
    
    for (int i = 0; i < NUM_LEDS; i++) {
        int16_t dist = abs(i - head);
        if (dist == 0) {
          targetLeds[i] = palette[i];
          targetLeds[i].maximizeBrightness();
        } else if (dist == 1) {
          targetLeds[i] = palette[i];
          targetLeds[i].nscale8(160); // 60% jasności dla krawędzi fali
        } else if (dist == 2) {
          targetLeds[i] = palette[i];
          targetLeds[i].nscale8(60);  // 25% jasności dla zanikania
        } else {
          targetLeds[i] = CRGB::Black;
        }
    }
  } else {
    // Koniec ceremonii - przechodzimy do głównego zadania
    currentMode = MODE_SHIFT_LIGHT;
    welcomeTimer = 0; 
  }
}

// Obsługa wizualnego potwierdzenia zapisu (wszystkie diody na zielono przez 1s)
void handleLEDTestSequence(uint32_t now) {
  static uint32_t testTimer = 0;
  if(testTimer == 0) testTimer = now;
  
  if (now - testTimer < 1000) {
    for(int i = 0; i < NUM_LEDS; i++) targetLeds[i] = CRGB::Green;
  } else {
    // Powrót do normalnego trybu
    currentMode = MODE_SHIFT_LIGHT;
    testTimer = 0; 
  }
}

// Wykonuje jeden cykl obliczeniowy i wyświetlanie diod LED (wywoływane cyklicznie przez Task1)
void updateLEDs() {
  uint32_t now = millis();
  // Jeśli są starsze niż 1000ms, pokazujemy animację błędu (pulsowanie na czerwono)
  bool isDataStale = (now - lastRPMTime > 500);
  bool isError = (now - lastRPMTime > 1000);

  // Czyścimy bufor docelowy przed nowymi obliczeniami
  for(int i = 0; i < NUM_LEDS; i++) targetLeds[i] = CRGB::Black;

  if (currentMode == MODE_WELCOME) {
    handleWelcomeSequence(now);
  } else if (isError && currentMode != MODE_0_100_COUNTDOWN) {
    uint8_t brt = max((int)brightness, 1);
    uint8_t safeMin = min(200, max(40, 1280 / brt));
    uint8_t breath = beatsin8(30, safeMin, 255);
    CRGB errorColor = CRGB(breath, 0, 0);
    for(int i=0; i<NUM_LEDS; i++) {
      targetLeds[i] = errorColor;
      leds[i] = errorColor; // Bezpośredni zapis — beatsin8 sam daje gładką sinusoidę, blending tylko dodaje szumy
    }
  } else if (isDataStale && currentMode != MODE_0_100_COUNTDOWN) {
    // Dane lekko spóźnione (500ms-1000ms) - po prostu wygaszamy
  } else if (currentMode == MODE_0_100_COUNTDOWN) {
    handleCountdownSequence(now);
  } else if (currentMode == MODE_LED_TEST) {
    handleLEDTestSequence(now);
  } else {
    // Normalna logika Shift Light
    handleShiftLightLogic(now);
  }

  // Płynne przechodzenie kolorów (efekt wygaszania dla 100 FPSów)
  for(int i=0; i<NUM_LEDS; i++) {
    // Jeśli różnica jest minimalna, przeskocz od razu do celu (eliminuje flickering/drgania przy niskiej jasności)
    if (abs((int)leds[i].r - (int)targetLeds[i].r) < 2 && 
        abs((int)leds[i].g - (int)targetLeds[i].g) < 2 && 
        abs((int)leds[i].b - (int)targetLeds[i].b) < 2) {
      leds[i] = targetLeds[i];
    } else {
      leds[i] = blend(leds[i], targetLeds[i], 60); 
    }
  }

  FastLED.show();
}
