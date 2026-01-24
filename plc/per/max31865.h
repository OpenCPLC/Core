#ifndef MAX31865_H_
#define MAX31865_H_

//-------------------------------------------------------------------------------------------------

#include "spi-master.h"
#include "log.h"
#include "extmath.h"

#define MAX31865_CFG_BIAS  (1 << 7)
#define MAX31865_CFG_AUTO  (1 << 6)
#define MAX31865_CFG_SHOT  (1 << 5)
#define MAX31865_CFG_FSCLR (1 << 1)

typedef enum {
  MAX31865_Reg_Read_Configuration = 0x00,
  MAX31865_Reg_Read_RTD_MSB = 0x01,
  MAX31865_Reg_Read_RTD_LSB = 0x02,
  MAX31865_Reg_Read_HighFaultThreshold_MSB = 0x03,
  MAX31865_Reg_Read_HighFaultThreshold_LSB = 0x04,
  MAX31865_Reg_Read_LowFaultThreshold_MSB = 0x05,
  MAX31865_Reg_Read_LowFaultThreshold_LSB = 0x06,
  MAX31865_Reg_Read_FaultStatus = 0x07,
  MAX31865_Reg_Write_Configuration = 0x80,
  MAX31865_Reg_Write_HighFaultThreshold_MSB = 0x83,
  MAX31865_Reg_Write_HighFaultThreshold_LSB = 0x84,
  MAX31865_Reg_Write_LowFaultThreshold_MSB = 0x85,
  MAX31865_Reg_Write_LowFaultThreshold_LSB = 0x86,
} MAX31865_Reg_e;

typedef enum {
  RTD_Type_None = 0,
  RTD_Type_PT100 = 100,
  RTD_Type_PT1000 = 1000
} RTD_Type_e;

typedef enum {
  RTD_Wire_2 = 0,
  RTD_Wire_3 = 1,
  RTD_Wire_4 = 0
} RTD_Wire_e;

typedef enum {
  RTD_Reject_60Hz = 0,
  RTD_Reject_50Hz = 1
} RTD_Reject_e;

typedef struct {
  const char *name;
  SPI_Master_t *spi; // Interfejs SPI dla komunikacji
  GPIO_t *cs; // Pin CS (chip select)
  GPIO_t *ready; // Pin DATA-READY sygnalizujący zakończenie konwersji
  float nominal_ohms; // Nominalna rezystancja czujnika [Ω] lib RTD_Type_e
  float reference_ohms; // Rezystancja referencyjna
  RTD_Wire_e wire; // Typ połączenia (2, 3 lub 4-przewodowe)
  RTD_Reject_e reject; // Filtracja zakłóceń (sieć 50Hz lub 60Hz)
  uint16_t oversampling; // Liczba próbek dla nadpróbkowania software'owego
	uint16_t expiry_ms;     // Czas, po którym brak odpowiedzi od czujnika skutkuje ustawieniem NaN
	uint16_t interval_ms;   // Interwał pomiędzy pomiarami (powinien być co najmniej o 200ms krótszy niż 'expiry_ms')
  uint16_t raw;
  float raw_float;
  volatile float temperature;
  uint8_t buff[3];
  uint64_t expiry_tick;
  uint64_t interval_tick;
} MAX31865_t;

void MAX31865_Init(MAX31865_t *rtd); // Inicjalizacja modułu MAX31865
status_t MAX31865_Loop(MAX31865_t *rtd); // Główna pętla obsługi pomiarów
float RTD_Resistance_Ohm(MAX31865_t *rtd); // Oblicza rezystancję [Ω] czujnika RTD
float RTD_Temperature_C(MAX31865_t *rtd); // Oblicza temperaturę [°C] PT100/PT1000

//-------------------------------------------------------------------------------------------------
#endif