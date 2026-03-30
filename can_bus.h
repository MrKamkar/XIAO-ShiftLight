#pragma once
#include <Arduino.h>

void sendOBDRequest(uint8_t pid); // Wysyła zapytanie diagnostyczne OBD2 w standardzie ISO 15765-4
