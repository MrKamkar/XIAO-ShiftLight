#pragma once

// Rdzeń 0 (Protocol CPU): Komunikacja BLE, CAN, Logowanie
void taskCore0(void *pvParameters);

// Rdzeń 1 (Application CPU): Wyświetlanie diod LED i animacje
void taskCore1(void *pvParameters);
