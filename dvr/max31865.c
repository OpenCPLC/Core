// dvr/max31865.c

#include "max31865.h"

#include <math.h>
#include "log.h"
#include "vrts.h"

//---------------------------------------------------------------------------------------- Internal

static status_t get_data(MAX31865_t *rtd)
{
  if(timeout(100, WAIT_&GPIO_In, rtd->ready)) return ERR;
  SPI_Master_Read(rtd->spi, MAX31865_Reg_Read_RTD_MSB, rtd->_buff, 3);
  if(timeout(50, WAIT_&SPI_Master_IsFree, rtd->spi)) return ERR;
  rtd->_raw = ((uint16_t)rtd->_buff[1] << 7) | (rtd->_buff[2] >> 1);
  return OK;
}

static status_t set_config(MAX31865_t *rtd, uint8_t config)
{
  rtd->_buff[0] = MAX31865_Reg_Write_Configuration;
  rtd->_buff[1] = config;
  SPI_Master_Write(rtd->spi, rtd->_buff, 2);
  if(timeout(50, WAIT_&SPI_Master_IsFree, rtd->spi)) return ERR;
  return OK;
}

//--------------------------------------------------------------------------------------------- API

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
  rtd->_raw_float = NaN;
}

status_t MAX31865_Loop(MAX31865_t *rtd)
{
  if(tick_away(&rtd->_interval_tick)) return OK;
  if(tick_over(&rtd->_expiry_tick)) rtd->_raw_float = NaN;
  if(rtd->cs) rtd->spi->cs = rtd->cs;
  uint8_t cfg = (rtd->wire << 4) | rtd->reject;
  if(SPI_Master_IsBusy(rtd->spi)) return BUSY;
  if(set_config(rtd, MAX31865_CFG_BIAS | cfg)) return ERR;
  delay(10);
  if(rtd->oversampling) {
    if(set_config(rtd, MAX31865_CFG_BIAS | MAX31865_CFG_AUTO | cfg)) return ERR;
    float value = 0;
    for(uint16_t n = 0; n < rtd->oversampling; n++) {
      if(get_data(rtd)) return ERR;
      value += (float)rtd->_raw;
    }
    rtd->_raw_float = value / rtd->oversampling;
  }
  else {
    if(set_config(rtd, MAX31865_CFG_BIAS | MAX31865_CFG_SHOT | cfg)) return ERR;
    if(get_data(rtd)) return ERR;
    rtd->_raw_float = (float)rtd->_raw;
  }
  // Fault status cleared for the next round
  if(set_config(rtd, MAX31865_CFG_FSCLR | cfg)) return ERR;
  LOG_Debug("MAX31865 %s raw value: %.2f", rtd->name, rtd->_raw_float);
  rtd->_expiry_tick = tick_keep(rtd->expiry_ms);
  rtd->_interval_tick = tick_keep(rtd->interval_ms);
  return OK;
}

//----------------------------------------------------------------------------------------- Convert

// Callendar-Van Dusen coefficients of platinum
#define MAX31865_A  3.9083e-3f
#define MAX31865_B  -5.775e-7f
#define MAX31865_Z1 (-MAX31865_A)
#define MAX31865_Z2 (MAX31865_A * MAX31865_A - (4 * MAX31865_B))

float RTD_Resistance_Ohm(const MAX31865_t *rtd)
{
  if(isNaN(rtd->_raw_float)) return NaN;
  return rtd->_raw_float * rtd->reference_ohms / 32768;
}

// The quadratic above 0 deg C, a fifth-order polynomial fit below
float RTD_Temperature_C(const MAX31865_t *rtd)
{
  if(isNaN(rtd->_raw_float)) return NaN;
  float ohms = RTD_Resistance_Ohm(rtd);
  float z3 = (4 * MAX31865_B) / rtd->nominal_ohms;
  float z4 = 2 * MAX31865_B;
  float temp = MAX31865_Z2 + (z3 * ohms);
  temp = (sqrtf(temp) + MAX31865_Z1) / z4;
  if(temp >= 0) return temp;
  ohms = ohms / rtd->nominal_ohms * 100;
  float x = ohms;
  temp = -242.02f;
  temp += 2.2228f * x;
  x *= ohms;
  temp += 2.5859e-3f * x;
  x *= ohms;
  temp -= 4.8260e-6f * x;
  x *= ohms;
  temp -= 2.8183e-8f * x;
  x *= ohms;
  temp += 1.5243e-10f * x;
  return temp;
}

//-------------------------------------------------------------------------------------------------
