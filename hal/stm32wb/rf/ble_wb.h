// hal/stm32wb/rf/ble_wb.h

#ifndef BLE_WB_H_
#define BLE_WB_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"
#include "buff.h"
#include "irq.h"
#include "wpan_wb.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef BLE_CHUNK_SIZE
  // Upper bound of one notification or indication [B], the ATT MTU caps it further
  #define BLE_CHUNK_SIZE 244
#endif

// Longest device name used by GAP and the scan response [B], longer names are cut.
// A scan response carries 31B, so the name with its length and type bytes has 29 of them.
#define BLE_NAME_SIZE 23

// What the 31B advertisement leaves for the name,
// beside the flags entry and the 128-bit service UUID: 31 - 3 - 18 - 2.
// Past it the advertised name is cut and marked shortened.
#define BLE_ADV_NAME_SIZE 8

#ifndef BLE_ADV_INTERVAL_MIN
  // Advertising interval [0.625ms], `0x80` = 80ms
  #define BLE_ADV_INTERVAL_MIN 0x80
#endif

#ifndef BLE_ADV_INTERVAL_MAX
  #define BLE_ADV_INTERVAL_MAX 0xA0
#endif

#ifndef BLE_TX_POWER
  // PA level `0..31`, `0x18` = 0dBm
  #define BLE_TX_POWER 0x18
#endif

#ifndef BLE_PIN_ENABLE
  // Pairing with a passkey and an encrypted link after it, `OFF` leaves the link open
  #define BLE_PIN_ENABLE OFF
#endif

#ifndef BLE_PAIR_DELAY_ms
  // Pause after the first failed pairing, each next failure doubles it, at most ×512
  #define BLE_PAIR_DELAY_ms 1000
#endif

#ifndef BLE_BOND_LIMIT
  // Bonded clients kept, under the CPU2 table capacity, whose overflow wipes the table
  #define BLE_BOND_LIMIT 4
#endif

//------------------------------------------------------------------------------------------- Types

// Step of `BLE_Init` that refused, the stack status byte lands in `fault_status`
typedef enum {
  BLE_Fault_None = 0,
  BLE_Fault_Config = 1,      // structure rejected before touching the CPU2
  BLE_Fault_Start = 2,       // `WPAN_Start`: clocks, mailbox, CPU2 boot
  BLE_Fault_StackInit = 3,   // `SHCI_C2_BLE_INIT`
  BLE_Fault_Reset = 4,       // `HCI_RESET`
  BLE_Fault_Address = 5,     // public address write
  BLE_Fault_Power = 6,       // TX power level
  BLE_Fault_GattInit = 7,    // `ACI_GATT_INIT`
  BLE_Fault_GapInit = 8,     // `ACI_GAP_INIT`
  BLE_Fault_Name = 9,        // GAP device name update
  BLE_Fault_Service = 10,    // `ACI_GATT_ADD_SERVICE`
  BLE_Fault_Char = 11,       // `ACI_GATT_ADD_CHAR`
  BLE_Fault_Advertise = 12,  // `ACI_GAP_SET_DISCOVERABLE`
  BLE_Fault_Security = 13    // pairing set-up of `BLE_PIN_ENABLE`
} BLE_Fault_t;

// Stack replies from the latest advertising attempt: 0 = accepted,
// 0xFF = no response or step not reached. Valid after advertising was attempted.
typedef struct {
  uint8_t start;         // `ACI_GAP_SET_DISCOVERABLE`
  uint8_t tx_power;      // `ACI_GAP_DELETE_AD_TYPE`
  uint8_t uuid;          // `ACI_GAP_UPDATE_ADV_DATA`
  uint8_t scan_response; // `HCI_LE_SET_SCAN_RESPONSE_DATA`
} BLE_AdvStatus_t;

// Characteristic properties, values as in the Bluetooth declaration bitfield
typedef enum {
  BLE_Prop_Read = 0x02,
  BLE_Prop_WriteNoResp = 0x04,
  BLE_Prop_Write = 0x08,
  BLE_Prop_Notify = 0x10,
  BLE_Prop_Indicate = 0x20
} BLE_Prop_t;

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief One characteristic of the service.
 * A readable one serves its stored value from the GATT database, so `BLE_Send`
 * on it only updates the value. Client writes land in `buff` and `OnWrite`,
 * `Notify` and `Indicate` push data out once the client subscribes.
 * @param[in] number Fourth UUID group seen by the client, `1..0xFFFF`
 * @param[in] properties `BLE_Prop_t` flags combined
 * @param[in] size Stored value size [B], up to `BLE_CHUNK_SIZE`
 * @param[in] buff Stream for client writes, `NULL` = value only in the database
 * @param[in] OnWrite Called from `BLE_Loop` on a client write (`NULL` = disabled)
 * Internal:
 * @param _handle Characteristic handle, value at `+1`, client configuration at `+2`
 * @param _subscribed CCCD bits, notifications `0x01` and indications `0x02`
 */
typedef struct {
  uint16_t number;
  uint8_t properties;
  uint16_t size;
  BUFF_t *buff;
  void (*OnWrite)(const uint8_t *data, uint16_t len);
  // internal
  uint16_t _handle;
  volatile uint8_t _subscribed;
} BLE_Char_t;

/**
 * @brief Bluetooth LE peripheral: one service with the characteristics the application
 * declares. Indications wait for the client confirmation, which is the flow control
 * of an indicated stream; notifications go out as fast as the stack takes them.
 * @param[in] name GAP device name, also advertised, `BLE_NAME_SIZE` at most
 * @param[in] uuid Service UUID in textual byte order; the fourth group is replaced
 *   by `0x0000` for the service and by `number` of each characteristic
 * @param[in] chars Characteristics of the service
 * @param[in] count Number of characteristics
 * @param[in] irq_priority IPCC interrupt priority
 * @param[in] pin Passkey `0..999999` the client types in, `BLE_PIN_ENABLE` only
 * @param[out] overflow RX bytes dropped on a full characteristic `buff`
 * @param[out] errors Refused commands and transfers cut by a disconnect
 * @param[out] resets Disconnects seen
 * @param[out] fault Step of `BLE_Init` that refused
 * @param[out] fault_status Stack status byte of that step, `0xFF` = no response
 * @param[out] adv_status Replies from the latest advertising attempt, including restarts;
 *   accepted commands do not guarantee reception by a scanner
 * @param[out] pairings Successful pairings, `BLE_PIN_ENABLE` only
 * @param[out] pair_fails Failed pairings after the passkey was given, `BLE_PIN_ENABLE` only
 * @param[out] bonds Clients in the CPU2 bond table, `BLE_PIN_ENABLE` only
 * Internal:
 * @param _service Service handle
 * @param _conn Connection handle
 * @param _mtu Negotiated ATT MTU [B]
 * @param _connected Client connected
 * @param _advertise Advertising restart pending
 * @param _adv_retry Next attempt after the stack refused the restart, `0` = none pending
 * @param _tx Characteristic with a transfer in flight
 * @param _tx_data Transfer in flight
 * @param _tx_len Transfer length [B]
 * @param _tx_pos Bytes already pushed
 * @param _tx_wait Indication awaiting its confirmation
 * @param _tx_busy Transfer in flight flag
 * @param _secured Link encrypted at security level 4, `BLE_PIN_ENABLE` only
 * @param _pin_given Passkey handed to the stack in the running pairing
 * @param _rebond Running pairing replaces the bond of the same client
 * @param _pair_streak Failed pairings since the last successful one, sets the delay
 * @param _pair_end Pairing window deadline, `0` = closed
 * @param _pair_hold No passkey before this deadline
 * @param _forget Bond removal: `1` pending, `2` link dropped and awaited
 * @param _init Initialization completed flag
 */
typedef struct {
  const char *name;
  uint8_t uuid[16];
  BLE_Char_t *chars;
  uint8_t count;
  IRQ_Priority_t irq_priority;
  #if(BLE_PIN_ENABLE)
  uint32_t pin;
  #endif
  volatile uint16_t overflow;
  volatile uint16_t errors;
  volatile uint16_t resets;
  BLE_Fault_t fault;
  uint8_t fault_status;
  BLE_AdvStatus_t adv_status;
  #if(BLE_PIN_ENABLE)
  uint16_t pairings;
  uint16_t pair_fails;
  uint8_t bonds;
  #endif
  // internal
  uint16_t _service;
  uint16_t _conn;
  uint16_t _mtu;
  bool _connected;
  bool _advertise;
  uint64_t _adv_retry;
  BLE_Char_t *_tx;
  const uint8_t *_tx_data;
  volatile uint16_t _tx_len;
  volatile uint16_t _tx_pos;
  bool _tx_wait;
  volatile bool _tx_busy;
  #if(BLE_PIN_ENABLE)
  bool _secured;
  bool _pin_given;
  bool _rebond;
  uint16_t _pair_streak;
  uint64_t _pair_end;
  uint64_t _pair_hold;
  uint8_t _forget;
  #endif
  bool _init;
} BLE_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Bring up CPU2, the BLE stack, GAP, GATT, the service and advertising.
 *   Blocks cooperatively on the CPU2, so call it from the thread that will run `BLE_Loop`.
 * @param[in,out] ble BLE instance
 * @return `OK` on success, `ERR` when the CPU2 or the stack refuses a step
 */
status_t BLE_Init(BLE_t *ble);

// Service thread for the instance passed to `BLE_Init`, never returns
void BLE_Loop(void);

/**
 * @brief One pass of the service work: pending events, advertising restart, transfers.
 *   `BLE_Loop` is this in a loop, a test or a custom scheduler calls it directly.
 * @return `true` when an event or the advertising restart was handled
 */
bool BLE_Step(void);

/**
 * @brief Check if a client is connected.
 * @param[in] ble BLE instance
 * @return `true` if connected, with `BLE_PIN_ENABLE` once the link is paired and encrypted
 */
bool BLE_IsConnected(BLE_t *ble);

// Park the radio before a system low-power entry:
// one reset drops the connection and the advertising both,
// so the CPU2 idles with no scheduled events.
// Only a system reset brings BLE back, the API refuses until then.
void BLE_Sleep(BLE_t *ble);

#if(BLE_PIN_ENABLE)
/**
 * @brief Open pairing for a new client, bonded clients connect regardless.
 *   Closes on the first successful pairing or when the window runs out.
 * @param[in,out] ble BLE instance
 * @param[in] window_ms Window length, `0` closes it
 */
void BLE_Pairing(BLE_t *ble, uint32_t window_ms);

// Check if the pairing window is open
bool BLE_IsPairing(BLE_t *ble);

// Remove every bond: the client drops first, then the CPU2 clears its bond table
void BLE_Forget(BLE_t *ble);
#endif

/**
 * @brief Check if the client subscribed to a characteristic.
 * @param[in] chr Characteristic
 * @return `true` when its notifications or indications are enabled
 */
bool BLE_Subscribed(BLE_Char_t *chr);

/**
 * @brief Get negotiated ATT MTU.
 * @param[in] ble BLE instance
 * @return MTU [B], `23` until the client asks for more
 */
uint16_t BLE_Mtu(BLE_t *ble);

/**
 * @brief Update a characteristic and push it to a subscribed client.
 *   Every send is executed by `BLE_Loop`, the only owner of the command channel,
 *   so `data` must stay valid until `BLE_IsFree`.
 *   Data longer than one chunk streams out piece by piece.
 *   A characteristic without `Notify` or `Indicate` only stores the value for reads,
 *   one chunk at most.
 * @param[in,out] ble BLE instance
 * @param[in,out] chr Target characteristic
 * @param[in] data New value
 * @param[in] len Number of bytes
 * @return `OK` if taken, `ERR` if refused or disconnected, `BUSY` if a transfer is in flight
 */
status_t BLE_Send(BLE_t *ble, BLE_Char_t *chr, const uint8_t *data, uint16_t len);

/**
 * @brief Check if a transfer to the client is in flight.
 * @param[in] ble BLE instance
 * @return `true` if busy
 */
bool BLE_IsBusy(BLE_t *ble);

/**
 * @brief Check if the link is free for `BLE_Send`.
 * @param[in] ble BLE instance
 * @return `true` if free
 */
bool BLE_IsFree(BLE_t *ble);

/**
 * @brief Get number of bytes in current message of a written characteristic.
 * @param[in] chr Characteristic
 * @return Number of bytes, `0` without a `buff`
 */
uint16_t BLE_Size(BLE_Char_t *chr);

/**
 * @brief Get number of pending messages of a written characteristic.
 * @param[in] chr Characteristic
 * @return Number of messages waiting to be read
 */
uint16_t BLE_MessageCount(BLE_Char_t *chr);

/**
 * @brief Read current message of a written characteristic.
 * @param[in,out] chr Characteristic
 * @param[out] data Destination buffer
 * @return Number of bytes read
 */
uint16_t BLE_Read(BLE_Char_t *chr, uint8_t *data);

/**
 * @brief Read current message as null-terminated string.
 * @param[in,out] chr Characteristic
 * @return String, valid until next read
 */
char *BLE_ReadString(BLE_Char_t *chr);

/**
 * @brief Skip current message of a written characteristic.
 * @param[in,out] chr Characteristic
 * @return `true` if a message was skipped
 */
bool BLE_Skip(BLE_Char_t *chr);

/**
 * @brief Clear RX buffer of a written characteristic.
 * @param[in,out] chr Characteristic
 */
void BLE_Clear(BLE_Char_t *chr);

//-------------------------------------------------------------------------------------------------
#endif
