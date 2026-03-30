#include <Arduino.h>
#include <driver/twai.h>
#include "config.h"
#include "can_bus.h"
#include "obd_pids.h"

/* Wysyła zapytanie diagnostyczne OBD2 w standardzie ISO 15765-4

   Identyfikator 0x7DF to bramka uniwersalna (Functional Request), nadsłuchiwana przez wszystkie systemy w aucie.
   Data[0]=0x02 oznacza rozmiar zapytania (2 bajty użytecznego ładunku).
   Data[1]=0x01 określa tak zwany "Tryb 1" (Parametry pracy samochodu na żywo).
   Data[2]=Podawany PID żądanej wartości (np. 0x0C dla obrotów RPM).
*/
void sendOBDRequest(uint8_t pid) {
  twai_message_t tx_msg;
  memset(&tx_msg, 0, sizeof(tx_msg)); // Zerowanie całej struktury (w tym padding data[3..7])
  
  tx_msg.identifier = CAN_ID_OBD_BROADCAST;
  tx_msg.data_length_code = 8; // Ramka CAN zgodnie ze standardem musi mieć 8 bajtów
  tx_msg.data[0] = OBD_PAYLOAD_LEN_2;
  tx_msg.data[1] = OBD_MODE_CURRENT_DATA;
  tx_msg.data[2] = pid;
  
  twai_transmit(&tx_msg, pdMS_TO_TICKS(5));
}
