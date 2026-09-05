// hal/stm32wb/rf/wpan_abi.h

// Shared-memory contract with the CPU2 wireless firmware, transcribed from
// `stm32-mw-wpan` commit `1a66948e` (STM32CubeWB 1.24.0): `mbox_def.h`, `tl.h`, `shci.h`.
// Every layout here must match the CPU2 binary bit for bit.
// Field names follow the ST headers, like register names follow the reference manual,
// so both sides of the contract can be compared directly.

#ifndef WPAN_ABI_H_
#define WPAN_ABI_H_

#include <stdint.h>

#define WPAN_PACKED __attribute__((packed))

//----------------------------------------------------------------------------------- IPCC channels

// CPU1 to CPU2: command carries the flag, CPU2 clears it when done
#define WPAN_CH_BLE  0x01u // BLE commands, events come back on the same number
#define WPAN_CH_SYS  0x02u // system commands and events
#define WPAN_CH_MM   0x08u // event buffers returned to the memory manager
#define WPAN_CH_ACL  0x20u // ACL data, unused with GATT on the CPU2

//----------------------------------------------------------------------------------------- Packets

// Every packet starts with the list node the queues link through
typedef struct WPAN_PACKED {
  uint32_t *next;
  uint32_t *prev;
} WPAN_Header_t;

#define WPAN_TYPE_BLE_CMD 0x01
#define WPAN_TYPE_ACL     0x02
#define WPAN_TYPE_SYS_CMD 0x10

typedef struct WPAN_PACKED {
  uint16_t cmdcode;
  uint8_t plen;
  uint8_t payload[255];
} WPAN_Cmd_t;

typedef struct WPAN_PACKED {
  WPAN_Header_t header;
  uint8_t type;
  WPAN_Cmd_t cmd;
} WPAN_CmdPacket_t;

// Event codes: standard HCI events keep their own, these three shape the payload below
#define WPAN_EVT_COMPLETE 0x0E // command complete, `WPAN_CcEvt_t`
#define WPAN_EVT_STATUS   0x0F // command status, `WPAN_CsEvt_t`
#define WPAN_EVT_VENDOR   0xFF // asynchronous ACI and system events, `WPAN_AsynchEvt_t`

typedef struct WPAN_PACKED {
  uint8_t evtcode;
  uint8_t plen;
  uint8_t payload[255];
} WPAN_Evt_t;

typedef struct WPAN_PACKED {
  WPAN_Header_t header;
  uint8_t type;
  WPAN_Evt_t evt;
} WPAN_EvtPacket_t;

typedef struct WPAN_PACKED {
  uint8_t status;
  uint8_t numcmd;
  uint16_t cmdcode;
} WPAN_CsEvt_t;

typedef struct WPAN_PACKED {
  uint8_t numcmd;
  uint16_t cmdcode;
  uint8_t payload[255]; // return parameters, status byte first
} WPAN_CcEvt_t;

typedef struct WPAN_PACKED {
  uint16_t subevtcode;
  uint8_t payload[255];
} WPAN_AsynchEvt_t;

typedef struct WPAN_PACKED {
  WPAN_Header_t header;
  uint8_t type;
  uint16_t handle;
  uint16_t length;
  uint8_t data[251];
} WPAN_AclPacket_t;

// Prefix sizes inside an event: serialized front and the command-complete front
#define WPAN_EVT_SERIAL_SIZE 3 // type, evtcode, plen
#define WPAN_CC_SIZE 3         // numcmd, cmdcode

//------------------------------------------------------------------------------------------ Tables

// The reference table sits where the `IPCCDBA` option byte points (`SRAM2A` base
// by default), the CPU2 reaches everything else through it

typedef struct WPAN_PACKED { uint32_t Version; } WPAN_SafeBootInfo_t;

typedef struct WPAN_PACKED {
  uint32_t Version;
  uint32_t MemorySize;
  uint32_t FusInfo;
} WPAN_FusInfo_t;

typedef struct WPAN_PACKED {
  uint32_t Version; // major.minor.sub in the top three bytes
  uint32_t MemorySize;
  uint32_t InfoStack;
  uint32_t Reserved;
} WPAN_WirelessFwInfo_t;

typedef struct {
  WPAN_SafeBootInfo_t SafeBootInfoTable;
  WPAN_FusInfo_t FusInfoTable;
  WPAN_WirelessFwInfo_t WirelessFwInfoTable;
} WPAN_DeviceInfoTable_t;

typedef struct {
  uint8_t *pcmd_buffer;
  uint8_t *pcs_buffer;
  uint8_t *pevt_queue;
  uint8_t *phci_acl_data_buffer;
} WPAN_BleTable_t;

typedef struct {
  uint8_t *notack_buffer;
  uint8_t *clicmdrsp_buffer;
  uint8_t *otcmdrsp_buffer;
  uint8_t *clinot_buffer;
} WPAN_ThreadTable_t;

typedef struct {
  uint8_t *clicmdrsp_buffer;
  uint8_t *m0cmd_buffer;
} WPAN_LldTestsTable_t;

typedef struct {
  uint8_t *cmdrsp_buffer;
  uint8_t *m0cmd_buffer;
} WPAN_BleLldTable_t;

typedef struct {
  uint8_t *notifM0toM4_buffer;
  uint8_t *appliCmdM4toM0_buffer;
  uint8_t *requestM0toM4_buffer;
} WPAN_ZigbeeTable_t;

typedef struct {
  uint8_t *pcmd_buffer;
  uint8_t *sys_queue;
} WPAN_SysTable_t;

typedef struct {
  uint8_t *spare_ble_buffer;
  uint8_t *spare_sys_buffer;
  uint8_t *blepool;
  uint32_t blepoolsize;
  uint8_t *pevt_free_buffer_queue;
  uint8_t *traces_evt_pool;
  uint32_t tracespoolsize;
} WPAN_MemManagerTable_t;

typedef struct { uint8_t *traces_queue; } WPAN_TracesTable_t;

typedef struct {
  uint8_t *p_cmdrsp_buffer;
  uint8_t *p_notack_buffer;
  uint8_t *evt_queue;
} WPAN_MacTable_t;

typedef struct {
  WPAN_DeviceInfoTable_t *p_device_info_table;
  WPAN_BleTable_t *p_ble_table;
  WPAN_ThreadTable_t *p_thread_table;
  WPAN_SysTable_t *p_sys_table;
  WPAN_MemManagerTable_t *p_mem_manager_table;
  WPAN_TracesTable_t *p_traces_table;
  WPAN_MacTable_t *p_mac_802_15_4_table;
  WPAN_ZigbeeTable_t *p_zigbee_table;
  WPAN_LldTestsTable_t *p_lld_tests_table;
  WPAN_BleLldTable_t *p_ble_lld_table;
} WPAN_RefTable_t;

//-------------------------------------------------------------------------------------------- SHCI

#define WPAN_SHCI_BLE_INIT 0xFC66 // OGF 0x3F << 10, OCF 0x66
#define WPAN_FUS_START_WS  0xFC5A // FUS boots the installed stack, then CPU2 restarts
#define WPAN_FLASH_ERASE_ACTIVITY 0xFC69 // CPU1 erases ahead, radio timing stretches

// Vendor system events, `subevtcode` values
#define WPAN_EVT_READY 0x9200
#define WPAN_EVT_ERROR 0x9201

// Ready event payload when firmware upgrade services answer instead of the stack
#define WPAN_FUS_RUNNING 0x01

#define WPAN_BLE_CORE_5_4 13

// `SHCI_C2_BLE_INIT` parameters; the comments in the ST `shci.h` describe each field
typedef struct WPAN_PACKED {
  uint8_t *pBleBufferAddress; // unused, zero
  uint32_t BleBufferSize;     // unused, zero
  uint16_t NumAttrRecord;
  uint16_t NumAttrServ;
  uint16_t AttrValueArrSize;
  uint8_t NumOfLinks;
  uint8_t ExtendedPacketLengthEnable;
  uint8_t PrWriteListSize;
  uint8_t MblockCount;
  uint16_t AttMtu;
  uint16_t PeripheralSca;
  uint8_t CentralSca;
  uint8_t LsSource;
  uint32_t MaxConnEventLength;
  uint16_t HsStartupTime;
  uint8_t ViterbiEnable;
  uint8_t Options;
  uint8_t HwVersion;
  uint8_t max_coc_initiator_nbr;
  int8_t min_tx_power;
  int8_t max_tx_power;
  uint8_t rx_model_config;
  uint8_t max_adv_set_nbr;
  uint16_t max_adv_data_len;
  int16_t tx_path_compens;
  int16_t rx_path_compens;
  uint8_t ble_core_version;
  uint8_t Options_extension;
  uint8_t MaxAddEattBearers;
  uint8_t *extra_data_buffer;
  uint32_t extra_data_buffer_size;
} WPAN_BleInit_t;

//------------------------------------------------------------------------------------------ Guards

// A transcription slip in a packed layout would corrupt the mailbox silently
_Static_assert(sizeof(WPAN_Header_t) == 8, "WPAN_Header_t layout");
_Static_assert(sizeof(WPAN_CmdPacket_t) == 267, "WPAN_CmdPacket_t layout");
_Static_assert(sizeof(WPAN_EvtPacket_t) == 266, "WPAN_EvtPacket_t layout");
_Static_assert(sizeof(WPAN_CsEvt_t) == 4, "WPAN_CsEvt_t layout");
_Static_assert(sizeof(WPAN_CcEvt_t) == 258, "WPAN_CcEvt_t layout");
_Static_assert(sizeof(WPAN_AclPacket_t) == 264, "WPAN_AclPacket_t layout");
_Static_assert(sizeof(WPAN_BleInit_t) == 55, "WPAN_BleInit_t layout");
_Static_assert(sizeof(WPAN_RefTable_t) == 40, "WPAN_RefTable_t layout");
_Static_assert(sizeof(WPAN_DeviceInfoTable_t) == 32, "WPAN_DeviceInfoTable_t layout");

//-------------------------------------------------------------------------------------------------
#endif
