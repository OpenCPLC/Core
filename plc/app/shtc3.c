#include "shtc3.h"
#include "crc.h"

//-------------------------------------------------------------------------------------------------

const CRC_t shtc3_crc = {
  .width = 8,
  .polynomial = 0x31,
  .initial = 0xFF,
  .reflect_data_in = false,
  .reflect_data_out = false,
  .final_xor = 0x00,
  .invert_out = false
};

//-------------------------------------------------------------------------------------------------

static bool SHTC3_SendCommand(SHTC3_t *shtc3, SHTC3_CMD_t cmd)
{
	shtc3->buff_tx[0] = (uint8_t)(cmd >> 8);
	shtc3->buff_tx[1] = (uint8_t)(cmd);
	return TWI_Write(SHTC3_ADRRESS, shtc3->buff_tx, 2);
}

static bool SHTC3_WakeUp(SHTC3_t *shtc3)
{
  bool ok = SHTC3_SendCommand(shtc3, SHTC3_CMD_WakuUp);
  if(!ok) return false;
  delay(10);
  return true;
}

static bool SHTC3_SetMode(SHTC3_t *shtc3)
{
  return SHTC3_SendCommand(shtc3, shtc3->mode);
}

static bool SHTC3_Read(SHTC3_t *shtc3)
{
  return TWI_Read(SHTC3_ADRRESS, shtc3->buff_rx, 6);
}

static bool SHTC3_Sleep(SHTC3_t *shtc3)
{
  bool ok = SHTC3_SendCommand(shtc3, SHTC3_CMD_Sleep);
  if(!ok) return false;
  delay(10);
  return true;
}

static bool SHTC3_CheckAppend(SHTC3_t *shtc3)
{
  if((uint8_t)CRC_Run(&shtc3_crc, &shtc3->buff_rx[0], 2) != shtc3->buff_rx[2]) return false;
	if((uint8_t)CRC_Run(&shtc3_crc, &shtc3->buff_rx[3], 2) != shtc3->buff_rx[5]) return false;
	shtc3->raw.humidity += ((int32_t) shtc3->buff_rx[3] << 8 | shtc3->buff_rx[4]);
	shtc3->raw.temperature += ((int32_t) shtc3->buff_rx[0] << 8 | shtc3->buff_rx[1]);
	return true;
}

static void SHTC3_Calculate(SHTC3_t *shtc3)
{
	shtc3->humidity = 100.0 * (float)shtc3->raw.humidity / shtc3->oversampling / 65536;
	shtc3->temperature = (175.0 * (float)shtc3->raw.temperature / shtc3->oversampling / 65536) - 45.0;
}

SHTC3_t SHTC3 = {
  .mode = SHTC3_Mode_NormalClockEnabled,
  .oversampling = 50,
  .expiry_ms = 1000,
  .interval_ms = 500,
  .humidity = NaN,
  .temperature = NaN
};

status_t SHTC3_Loop(void)
{
  if(tick_away(&SHTC3.interval_tick)) return OK;
  if(tick_over(&SHTC3.expiry_tick)) {
    SHTC3.humidity = NaN;
    SHTC3.temperature = NaN;
  }
  if(TWI_IsBusy()) return BUSY;
  SHTC3.raw.humidity = 0;
  SHTC3.raw.temperature = 0;
  if(!SHTC3_WakeUp(&SHTC3)) return ERR;
  if(!SHTC3_SetMode(&SHTC3)) return ERR;
  if(!SHTC3.oversampling) SHTC3.oversampling = 1;
  for(uint16_t i = 0; i < SHTC3.oversampling; i++) {
    if(!SHTC3_Read(&SHTC3)) return ERR;
     if(!SHTC3_CheckAppend(&SHTC3)) return ERR;
  }
  if(!SHTC3_Sleep(&SHTC3)) return ERR;
  SHTC3_Calculate(&SHTC3);
  SHTC3.expiry_tick = tick_keep(SHTC3.expiry_ms);
  SHTC3.interval_tick = tick_keep(SHTC3.interval_ms);
  return OK;
}

float SHTC3_Temperature_C(void)
{
  return SHTC3.temperature;
}

float SHTC3_Humidity_RH(void)
{
  return SHTC3.humidity;
}

//-------------------------------------------------------------------------------------------------
