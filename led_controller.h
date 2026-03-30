#pragma once

// Inicjalizuje kontroler LED
void setupLEDs();

// Główna pętla kontrolera LED wywoływana na rdzeniu 1, by płynnie wyświetlać animacje
void taskCore1(void *pvParameters);
