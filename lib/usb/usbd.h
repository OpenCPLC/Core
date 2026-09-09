// lib/usb/usbd.h

#ifndef USBD_H_
#define USBD_H_

#include <stdbool.h>
#include <stdint.h>

#include "usb.h"
#include "buff.h"
#include "xdef.h"
#include "main.h"

#if(USB_PRESENT)

//------------------------------------------------------------------------------------------ Config

#ifndef USBD_PACKET_SIZE
  // Bulk endpoint packet size [B], full-speed maximum
  #define USBD_PACKET_SIZE 64
#endif

#ifndef USBD_EP_BULK
  // Hardware endpoint carrying the bulk pair, IN and OUT share the number
  #define USBD_EP_BULK 1
#endif

#ifndef USBD_EP_NOTIFY
  // Hardware endpoint of the CDC notification IN, required by the class, never written
  #define USBD_EP_NOTIFY 2
#endif

#ifndef USBD_DESC_SIZE
  // Scratch for descriptors built on request, sized for the MS OS 2.0 set with one GUID
  #define USBD_DESC_SIZE 192
#endif

#ifndef USBD_CTRL_SIZE
  // Control OUT data stage buffer [B], one packet is enough for every class request
  #define USBD_CTRL_SIZE 64
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  USBD_Mode_Vendor = 0, // one vendor interface with a bulk pair, WinUSB and WebUSB through BOS
  USBD_Mode_CDC = 1     // CDC ACM: virtual COM port, class driver on every desktop OS
} USBD_Mode_t;

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief USB device: byte link to the host over a bulk pair, vendor interface or CDC ACM.
 * In vendor mode WinUSB binds through the MS OS 2.0 descriptor set, browsers through WebUSB,
 * both announced in the BOS descriptor when their strings are set.
 * In CDC mode the host sees a serial port, `guid` and `url` are ignored.
 * @param[in] irq_priority Interrupt priority
 * @param[in] mode Interface presented to the host
 * @param[in] vid Vendor ID
 * @param[in] pid Product ID
 * @param[in] version `bcdDevice`, `0x0100` = 1.0
 * @param[in] manufacturer ASCII string, `NULL` = none
 * @param[in] product ASCII string, `NULL` = none
 * @param[in] serial ASCII string, `NULL` = MCU unique ID as 24 hex digits
 * @param[in] guid WinUSB `DeviceInterfaceGUID` as `{...}` with dashes, `NULL` = no MS OS 2.0
 * @param[in] url WebUSB landing page without scheme, `NULL` = no WebUSB
 * @param[in] url_https Landing page scheme, `true` = https, `false` = http
 * @param[in] max_power_mA Bus current declared to the host, `0..500`
 * @param[in] self_powered Device runs without bus power
 * @param[in] remote_wakeup Device may wake the host
 * @param[in] buff OUT stream, host to device
 * Internal:
 * @param _usb Controller
 * @param _ctrl Control OUT data stage stream
 * @param _ctrl_memory Backing store of `_ctrl`
 * @param _desc Descriptor under transfer
 * @param _line_coding CDC line coding: baud, stop bits, parity, data bits
 * @param _out_request Class request waiting for its data stage, `0` = none
 * @param _out_length Bytes the pending request carries
 * @param _config Active configuration, `0` = addressed only
 * @param _suspended Bus suspended
 * @param _wakeup Remote wakeup armed by the host
 * @param _dtr CDC terminal open (`DTR`)
 * @param _init Initialization completed flag
 */
typedef struct {
  IRQ_Priority_t irq_priority;
  USBD_Mode_t mode;
  uint16_t vid;
  uint16_t pid;
  uint16_t version;
  const char *manufacturer;
  const char *product;
  const char *serial;
  const char *guid;
  const char *url;
  bool url_https;
  uint16_t max_power_mA;
  bool self_powered;
  bool remote_wakeup;
  BUFF_t *buff;
  // internal
  USB_t _usb;
  BUFF_t _ctrl;
  uint8_t _ctrl_memory[USBD_CTRL_SIZE];
  uint8_t _desc[USBD_DESC_SIZE];
  uint8_t _line_coding[7];
  uint8_t _out_request;
  uint16_t _out_length;
  uint8_t _config;
  bool _suspended;
  bool _wakeup;
  bool _dtr;
  bool _init;
} USBD_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Start device and attach to the bus.
 * @param[in,out] usbd Pointer to device structure
 * @return `OK` on success, `ERR` without a `buff` or when the controller refuses the endpoints
 */
status_t USBD_Init(USBD_t *usbd);

/**
 * @brief Service one pending bus event: reset, suspend, resume or a control request.
 * @param[in,out] usbd Pointer to device structure
 * @return `true` if an event was handled, `false` if nothing was pending
 */
bool USBD_Step(USBD_t *usbd);

// Service thread for the device passed to `USBD_Init`, never returns
void USBD_Loop(void);

/**
 * @brief Check if host configured the device and the bus is awake.
 * @param[in] usbd Pointer to device structure
 * @return `true` if the bulk link can carry data
 */
bool USBD_IsReady(USBD_t *usbd);

/**
 * @brief Check if a CDC host opened the port. Always `false` in vendor mode.
 * @param[in] usbd Pointer to device structure
 * @return `true` while the host asserts `DTR`
 */
bool USBD_DTR(USBD_t *usbd);

/**
 * @brief Get CDC line coding requested by the host, `115200` 8N1 until it sets one.
 * @param[in] usbd Pointer to device structure
 * @return Baud rate [bit/s]
 */
uint32_t USBD_Baud(USBD_t *usbd);

/**
 * @brief Start bulk IN transfer. `data` must stay valid until `USBD_IsFree`.
 *   A transfer ending on a packet boundary closes with a zero-length packet.
 * @param[in,out] usbd Pointer to device structure
 * @param[in] data Pointer to data
 * @param[in] len Number of bytes
 * @return `OK` if started, `ERR` if link is not ready, `BUSY` if a transfer is in flight
 */
status_t USBD_Send(USBD_t *usbd, const uint8_t *data, uint16_t len);

/**
 * @brief Check if bulk IN transfer is in flight.
 * @param[in] usbd Pointer to device structure
 * @return `true` if busy
 */
bool USBD_IsBusy(USBD_t *usbd);

/**
 * @brief Check if bulk IN direction is free for `USBD_Send`.
 * @param[in] usbd Pointer to device structure
 * @return `true` if free
 */
bool USBD_IsFree(USBD_t *usbd);

/**
 * @brief Get number of bytes in current RX transfer.
 * @param[in] usbd Pointer to device structure
 * @return Number of bytes in current transfer
 */
uint16_t USBD_Size(USBD_t *usbd);

/**
 * @brief Get number of pending transfers in RX queue.
 * @param[in] usbd Pointer to device structure
 * @return Number of transfers waiting to be read
 */
uint16_t USBD_MessageCount(USBD_t *usbd);

/**
 * @brief Read current transfer from RX buffer.
 * @param[in,out] usbd Pointer to device structure
 * @param[out] data Pointer to destination buffer
 * @return Number of bytes read
 */
uint16_t USBD_Read(USBD_t *usbd, uint8_t *data);

/**
 * @brief Read current transfer as null-terminated string.
 * @param[in,out] usbd Pointer to device structure
 * @return Pointer to string (valid until next read)
 */
char *USBD_ReadString(USBD_t *usbd);

/**
 * @brief Skip current transfer in RX buffer.
 * @param[in,out] usbd Pointer to device structure
 * @return `true` if a transfer was skipped
 */
bool USBD_Skip(USBD_t *usbd);

/**
 * @brief Clear RX buffer.
 * @param[in,out] usbd Pointer to device structure
 */
void USBD_Clear(USBD_t *usbd);

//-------------------------------------------------------------------------------------------------
#endif
#endif
