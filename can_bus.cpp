#include <Arduino.h>
#include <driver/twai.h>
#include "config.h"
#include "can_bus.h"

/* Wysyła zapytanie diagnostyczne OBD2 w standardzie ISO 15765-4

   Identyfikator 0x7DF to bramka uniwersalna (Functional Request), nadsłuchiwana przez wszystkie systemy w aucie.
   Data[0]=0x02 oznacza rozmiar zapytania (2 bajty użytecznego ładunku).
   Data[1]=0x01 określa tak zwany "Tryb 1" (Parametry pracy samochodu na żywo).
   Data[2]=Podawany PID żądanej wartości (np. 0x0C dla obrotów RPM).
*/
void sendOBDRequest(uint8_t pid) {
  twai_message_t tx_msg;
  tx_msg.identifier = 0x7DF;
  tx_msg.extd = 0; // Zapewnia pełną kompatybilność z protokołem ISO 15765-4 i niższą latencję szyny
  tx_msg.data_length_code = 8; // Ramka CAN zgodnie ze standardem musi mieć 8 bajtów
  tx_msg.data[0] = 0x02;
  tx_msg.data[1] = 0x01;
  tx_msg.data[2] = pid;

  // Dopełnienie ramki do 8 bajtów
  tx_msg.data[3] = 0x00; tx_msg.data[4] = 0x00; tx_msg.data[5] = 0x00; tx_msg.data[6] = 0x00; tx_msg.data[7] = 0x00;
  
  twai_transmit(&tx_msg, pdMS_TO_TICKS(5));
}
