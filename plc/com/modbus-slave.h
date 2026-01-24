#ifndef MODBUS_SLAVE_H_
#define MODBUS_SLAVE_H_

#include "uart.h"
#include "modbus.h"
#include "crc.h"

//-------------------------------------------------------------------------------------------------

typedef enum {
  MODBUS_Status_None = 0,        // No new frame received
  MODBUS_Status_Handled = 1,     // Valid frame received and processed (address matched)
  MODBUS_Status_Ignored = 2,     // Valid frame received but addressed to another device
  MODBUS_Status_UartBusy = 3,    // UART is currently busy (TX or RX in progress)
  MODBUS_Status_TooShort = 4,    // Frame is too short to be valid
  MODBUS_Status_InvalidSize = 5, // Data length does not match function specification
  MODBUS_Status_InvalidCRC = 6,  // CRC check failed
  MODBUS_Status_SendError = 7,   // Failed to send response frame
} MODBUS_Status_e;

#define MODBUS_IsError(status) (status >= MODBUS_Status_TooShort)

typedef struct {
  UART_t *uart;
  uint8_t address;
  uint16_t *reg_read;
  uint16_t *reg_write;
  uint16_t reg_count;
  const bool *write_mask; // Ustaw 1 jeżeli pozwala na wpisywanie do danego rejestru
  bool *update_flag; // Ustawia 1, gdy wartość została odświerzona
  bool update_any;
  uint8_t *buffer_tx;
  uint8_t *buffer_rx;
} MODBUS_Slave_t;

MODBUS_Status_e MODBUS_Loop(MODBUS_Slave_t *modbus);
bool MODBUS_HasUpdate(MODBUS_Slave_t *modbus);

//-------------------------------------------------------------------------------------------------
#endif