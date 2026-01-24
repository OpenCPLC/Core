#include "max31865.h"

//-------------------------------------------------------------------------------------------------

/**
 * @brief Inicjalizuje moduł MAX31865 do pracy z czujnikiem RTD.
 * @param rtd Wskaźnik do struktury reprezentującej układ MAX31865.
 */
void MAX31865_Init(MAX31865_t *rtd)
{
  if(rtd->cs) {
    rtd->cs->mode = GPIO_Mode_Output;
    GPIO_Init(rtd->cs);
  }
  rtd->ready->mode = GPIO_Mode_Input;
  GPIO_Init(rtd->ready);
  if(!rtd->nominal_ohms) rtd->nominal_ohms = RTD_Type_PT100;
  if(!rtd->reference_ohms) rtd->reference_ohms = 4 * rtd->nominal_ohms;
  rtd->raw_float = NaN;
}

static status_t MAX31865_GetData(MAX31865_t *rtd)
{
  if(timeout(100, WAIT_&GPIO_In, rtd->ready)) return ERR;
  SPI_Master_Read(rtd->spi, MAX31865_Reg_Read_RTD_MSB, rtd->buff, 3);
  if(timeout(50, WAIT_&SPI_Master_IsFree, rtd->spi)) return ERR;
  rtd->raw = ((uint16_t)rtd->buff[1] << 7) | (rtd->buff[2] >> 1);
  return OK;
}

static status_t MAX31865_SetConfig(MAX31865_t *rtd, uint8_t config)
{
  rtd->buff[0] = MAX31865_Reg_Write_Configuration;
  rtd->buff[1] = config;
  SPI_Master_Write(rtd->spi, rtd->buff, 2);
  if(timeout(50, WAIT_&SPI_Master_IsFree, rtd->spi)) return ERR;
  return OK;
}

/**
 * @brief Pętla obsługująca pomiar temperatury za pomocą czujnika RTD.
 * Wykonuje konfigurację, dokonuje pomiarów oraz oblicza temperaturę.
 * @param rtd Wskaźnik do struktury reprezentującej układ MAX31865.
 * @return Status wykonanej operacji {OK/ERR/BUSY}
*/
status_t MAX31865_Loop(MAX31865_t *rtd)
{
  if(tick_away(&rtd->interval_tick)) return OK;
  if(tick_over(&rtd->expiry_tick)) {
    rtd->raw_float = NaN;
  }
  if(rtd->cs) rtd->spi->cs_gpio = rtd->cs;
  uint8_t cfg = (rtd->wire << 4) | rtd->reject;
  if(SPI_Master_IsBusy(rtd->spi)) return BUSY;
  if(MAX31865_SetConfig(rtd, MAX31865_CFG_BIAS | cfg)) return ERR;
  delay(10);
  if(rtd->oversampling) {
    if(MAX31865_SetConfig(rtd, MAX31865_CFG_BIAS | MAX31865_CFG_AUTO | cfg)) return ERR;
    float value = 0;
    for(uint16_t n = 0; n < rtd->oversampling; n++) {
      if(MAX31865_GetData(rtd)) return ERR;
      value += (float)rtd->raw;
    }
    rtd->raw_float = value / rtd->oversampling;
  }
  else {
    if(MAX31865_SetConfig(rtd, MAX31865_CFG_BIAS | MAX31865_CFG_SHOT | cfg)) return ERR;
    if(MAX31865_GetData(rtd)) return ERR;
    rtd->raw_float = (float)rtd->raw;
  }
  // Read Errors
  if(MAX31865_SetConfig(rtd, MAX31865_CFG_FSCLR | cfg)) return ERR;
  LOG_Debug("MAX31865 %s raw value: %.2f", rtd->name, rtd->raw_float);
  rtd->expiry_tick = tick_keep(rtd->expiry_ms);
  rtd->interval_tick = tick_keep(rtd->interval_ms);
  return OK;
}

//-------------------------------------------------------------------------------------------------

#define MAX31865_A 3.9083e-3
#define MAX31865_B -5.775e-7
#define MAX31865_Z1 (-MAX31865_A)
#define MAX31865_Z2 (MAX31865_A * MAX31865_A - (4 * MAX31865_B))

/**
 * @brief Oblicza rezystancję [Ω] na podstawie surowej wartości pomiarowej.
 * @param rtd Wskaźnik do struktury MAX31865_t zawierającej konfigurację czujnika RTD
 * @return Wartość rezystancji [Ω] czujnika RTD.
 */
float RTD_Resistance_Ohm(MAX31865_t *rtd)
{
  if(isNaN(rtd->raw_float)) return NaN;
  return rtd->raw_float * rtd->reference_ohms / 32768;
}

/**
 * @brief Oblicza temperaturę w stopniach Celsjusza [°C] na podstawie
 * rezystancji termometru oporowego RTD za pomocą obliczeń rezystancji.
 * @param rtd Wskaźnik do struktury MAX31865_t zawierającej konfigurację czujnika RTD
 * @return Wartość temperatury [°C]
 */
float RTD_Temperature_C(MAX31865_t *rtd)
{
  if(isNaN(rtd->raw_float)) return NaN;
  float ohms = RTD_Resistance_Ohm(rtd);
  float z3 = (4 * MAX31865_B) / rtd->nominal_ohms;
  float z4 = 2 * MAX31865_B;
  float temp = MAX31865_Z2 + (z3 * ohms);
  temp = (sqrt(temp) + MAX31865_Z1) / z4;
  if(temp >= 0) return temp;
  ohms /= rtd->nominal_ohms;
  ohms *= 100;
  float x = ohms;
  temp = -242.02;
  temp += 2.2228 * x;
  x *= ohms;
  temp += 2.5859e-3 * x;
  x *= ohms;
  temp -= 4.8260e-6 * x;
  x *= ohms;
  temp -= 2.8183e-8 * x;
  x *= ohms;
  temp += 1.5243e-10 * x;
  return temp;
}

//-------------------------------------------------------------------------------------------------
