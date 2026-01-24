#ifndef ONE_WIRE_H_
#define ONE_WIRE_H_

#include "gpio.h"
#include "sys.h"

#define ONEWIRE_CMD_SKIP_ROM 0xCC
#define ONEWIRE_CMD_READ_ROM 0x33
#define ONEWIRE_CMD_SEARCH_ROM 0xF0
#define ONEWIRE_CMD_MATCH_ROM 0x55

typedef struct {
  GPIO_t *gpio;
  uint8_t _rom[8];
  uint8_t _last;
  uint8_t _last_family;
  bool _last_dev_flag;
} WIRE_t;

bool WIRE_Init(WIRE_t *onewire);
bool WIRE_Reset(WIRE_t *onewire);
void WIRE_Write(WIRE_t *onewire, uint8_t value);
void WIRE_WriteParasitePower(WIRE_t *onewire, uint8_t value);
uint8_t WIRE_Read(WIRE_t *onewire);
void WIRE_Select(WIRE_t *onewire, uint8_t *addr);
void WIRE_Skip(WIRE_t *onewire);
bool WIRE_Search(WIRE_t *onewire, uint8_t *addr);

#endif
