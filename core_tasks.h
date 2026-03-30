#pragma once

// Rdzeń 0 (Protocol CPU): WiFi, Serwer WWW, CAN, Logowanie
void taskCore0(void *pvParameters);

// Rdzeń 1 (Application CPU): Wyświetlanie diod LED i animacje
void taskCore1(void *pvParameters);
