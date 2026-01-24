#ifndef SHTC3_H_
#define SHTC3_H_

#include <stdint.h>
#include "extmath.h"
#include "twi.h"

//-------------------------------------------------------------------------------------------------

#define SHTC3_ADRRESS 0x70

typedef enum {
	SHTC3_CMD_Sleep = 0xB098,
	SHTC3_CMD_WakuUp = 0x3517,
	SHTC3_Mode_NormalClockEnabled = 0x7CA2,
	SHTC3_Mode_NormalClockDisable = 0x7866,
	SHTC3_Mode_LowPowerClockEnabled = 0x6458,
	SHTC3_Mode_LowPowerClockDisable = 0x609C
} SHTC3_CMD_t;

typedef struct {
  int32_t humidity;     // [%RH]
  int32_t temperature;  // [°C]
} SHTC3_Raw_t;

typedef struct {
	uint16_t mode;          // Tryb pracy: Normal|Low-Power, Clock Enable|Disable
	uint16_t oversampling;  // Liczba próbek do nadpróbkowania software'owego
	SHTC3_Raw_t raw;        // Wartości pomiarowe przed konwersją
	float humidity;         // Wilgotność po konwersji
	float temperature;      // Temperatura po konwersji
	uint16_t expiry_ms;     // Czas, po którym brak odpowiedzi od czujnika skutkuje ustawieniem NaN
	uint16_t interval_ms;   // Interwał pomiędzy pomiarami (powinien być co najmniej o 200ms krótszy niż 'expiry_ms')
	uint8_t buff_tx[2];
	uint8_t buff_rx[6];
	uint64_t expiry_tick;
	uint64_t interval_tick;
} SHTC3_t;

status_t SHTC3_Loop(void);
float SHTC3_Temperature_C(void);
float SHTC3_Humidity_RH(void);

//-------------------------------------------------------------------------------------------------
#endif