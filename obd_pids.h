#pragma once

// Definicje parametrów OBD2 (PIDs) wg standardu SAE J1979

// Adresowanie CAN
#define CAN_ID_OBD_BROADCAST    0x7DF   // Adres ogólny do wszystkich sterowników (Functional Request)
#define CAN_ID_OBD_REPLY_MIN    0x7E8   // Minimalny ID odpowiedzi (zazwyczaj ECU silnika)
#define CAN_ID_OBD_REPLY_MAX    0x7EF   // Maksymalny ID odpowiedzi diagnostycznej

// Tryby pracy (SERVICES)
#define OBD_MODE_CURRENT_DATA   0x01    // Tryb 1: Dane bieżące z czujników
#define OBD_REPLY_OFFSET        0x40    // Offset dodawany przez ECU do numeru trybu w odpowiedzi

// Identyfikatory parametrów (PIDs)
#define PID_ENGINE_LOAD         0x04    // Obciążenie silnika (%)
#define PID_COOLANT_TEMP        0x05    // Temperatura płynu chłodniczego (°C)
#define PID_MAP_PRESSURE        0x0B    // Ciśnienie w dolocie (kPa)
#define PID_ENGINE_RPM          0x0C    // Obroty silnika (RPM)
#define PID_VEHICLE_SPEED       0x0D    // Prędkość pojazdu (km/h)
#define PID_IAT_TEMP            0x0F    // Temperatura powietrza dolotowego (°C)
#define PID_THROTTLE_POS        0x11    // Pozycja przepustnicy (%)
#define PID_BATTERY_VOLT        0x42    // Napięcie modułu sterującego (V)
#define PID_FUEL_LEVEL          0x2F    // Poziom paliwa w zbiorniku (%)

// Formatowanie ramki
#define OBD_PAYLOAD_LEN_2       0x02    // Rozmiar danych w zapytaniu (Tryb + PID)
#define OBD_UNUSED_PADDING      0x00    // Bajt wypełniający dla standardu 8-bajtowego
