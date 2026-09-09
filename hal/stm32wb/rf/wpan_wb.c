// hal/stm32wb/rf/wpan_wb.c

#include "wpan_wb.h"

#include <string.h>

#include "stm32wbxx.h"
#include "vrts.h"
#include "pwr.h"
#include "sys.h"
#include "rtc.h"
#include "wpan_abi.h"

#define shared(section_name) __attribute__((section(section_name), aligned(4)))

// Kept a macro: it sizes static arrays, so only constant arguments ever reach it
#define div_ceil(x, y) (((x) + (y) - 1) / (y))

//------------------------------------------------------------------------------------------- Lists

// Queues shared with the CPU2: circular, doubly linked, the head is a sentinel.
// Node layout is fixed by `WPAN_Header_t`, ownership by the IPCC channel flags;
// the interrupt mask only fences CPU1 against its own contexts.
typedef struct node {
  struct node *next;
  struct node *prev;
} node_t;

static void list_init(node_t *head)
{
  head->next = head;
  head->prev = head;
}

static bool list_empty(const node_t *head) { return head->next == head; }

static void list_push(node_t *head, node_t *node)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  node->next = head;
  node->prev = head->prev;
  head->prev->next = node;
  head->prev = node;
  __set_PRIMASK(primask);
}

static node_t *list_pop(node_t *head)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  node_t *node = head->next;
  head->next = node->next;
  node->next->prev = head;
  __set_PRIMASK(primask);
  return node;
}

//----------------------------------------------------------------------------------- Shared memory

// The reference table has to land where the `IPCCDBA` option byte points,
// which the linker guarantees by putting `MAPPING_TABLE` first in `RAM_SHARED`
shared("MAPPING_TABLE") static volatile WPAN_RefTable_t ref_table;

shared("MB_MEM1") static WPAN_DeviceInfoTable_t device_info_table;
shared("MB_MEM1") static WPAN_BleTable_t ble_table;
shared("MB_MEM1") static WPAN_ThreadTable_t thread_table;
shared("MB_MEM1") static WPAN_LldTestsTable_t lld_tests_table;
shared("MB_MEM1") static WPAN_BleLldTable_t ble_lld_table;
shared("MB_MEM1") static WPAN_SysTable_t sys_table;
shared("MB_MEM1") static WPAN_MemManagerTable_t mm_table;
shared("MB_MEM1") static WPAN_TracesTable_t traces_table;
shared("MB_MEM1") static WPAN_MacTable_t mac_table;
shared("MB_MEM1") static WPAN_ZigbeeTable_t zigbee_table;

shared("MB_MEM1") static node_t free_buf_queue;
shared("MB_MEM1") static node_t ble_evt_queue;
shared("MB_MEM1") static node_t sys_evt_queue;
shared("MB_MEM1") static node_t traces_queue;
shared("MB_MEM1") static WPAN_EvtPacket_t sys_spare_buffer;

#define WPAN_POOL_SIZE (WPAN_EVENT_QUEUE * 4u * div_ceil(sizeof(WPAN_EvtPacket_t), 4u))

shared("MB_MEM2") static WPAN_CmdPacket_t sys_cmd_buffer;
shared("MB_MEM2") static WPAN_CmdPacket_t ble_cmd_buffer;
shared("MB_MEM2") static WPAN_AclPacket_t ble_acl_buffer;
// Command status events land here, command completes reuse `ble_cmd_buffer`
shared("MB_MEM2") static uint8_t ble_cs_buffer[sizeof(WPAN_Header_t) +
  WPAN_EVT_SERIAL_SIZE + sizeof(WPAN_CsEvt_t)];
shared("MB_MEM2") static WPAN_EvtPacket_t ble_spare_buffer;
shared("MB_MEM2") static uint8_t evt_pool[WPAN_POOL_SIZE];

//------------------------------------------------------------------------------------------- State

static struct {
  volatile WPAN_Fw_t fw;         // reported by the ready event
  volatile bool ready;
  volatile bool sys_done;        // system command response landed in `sys_cmd_buffer`
  volatile bool ble_done;        // BLE command response parsed below
  volatile uint8_t ble_status;
  volatile uint8_t ble_rsp_len;
  volatile uint16_t ble_opcode;  // command awaiting its response
  volatile bool ble_lost;        // that command timed out, the buffer is not ours
  uint8_t ble_rsp[WPAN_RSP_SIZE];
  WPAN_EvtPacket_t * volatile event[WPAN_EVENT_LIMIT];
  volatile uint8_t event_head;
  volatile uint8_t event_tail;
  volatile uint16_t errors;
  bool init;
} wpan;

// Buffers on the way back to the CPU2 pool, flushed when the release channel frees up
static node_t local_free_queue;

//---------------------------------------------------------------------------------- Buffer release

// The release channel flag can only be set while it is free, so a busy channel
// defers the flush to its transmit-free interrupt.
static void mm_flush(void)
{
  while(!list_empty(&local_free_queue)) {
    list_push((node_t *)mm_table.pevt_free_buffer_queue, list_pop(&local_free_queue));
  }
}

// The mask register is shared with the IPCC interrupts and the interrupted
// read-modify-write of a thread would restore their stale bits
static void c1mr_clear(uint32_t mask)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  IPCC->C1MR &= ~mask;
  __set_PRIMASK(primask);
}

static void mm_release(WPAN_EvtPacket_t *packet)
{
  list_push(&local_free_queue, (node_t *)packet);
  if(IPCC->C1TOC2SR & WPAN_CH_MM) {
    c1mr_clear(WPAN_CH_MM << 16); // flush on transmit-free
  }
  else {
    mm_flush();
    IPCC->C1SCR = WPAN_CH_MM << 16;
  }
}

//--------------------------------------------------------------------------------------- Interrupt

static void sys_event(WPAN_EvtPacket_t *packet)
{
  WPAN_AsynchEvt_t *event = (WPAN_AsynchEvt_t *)packet->evt.payload;
  switch(event->subevtcode) {
    case WPAN_EVT_READY:
      wpan.fw = (event->payload[0] == WPAN_FUS_RUNNING) ? WPAN_Fw_Fus : WPAN_Fw_Wireless;
      wpan.ready = true;
      // FUS has no memory manager, its ready packet must not go back to the pool
      if(wpan.fw == WPAN_Fw_Fus) return;
      break;
    case WPAN_EVT_ERROR:
      wpan.errors++;
      break;
    default:
      break;
  }
  mm_release(packet);
}

// BLE command responses arrive in static buffers reused by the next command:
// the interrupt copies what the waiting thread needs and releases nothing.
static void ble_response(WPAN_EvtPacket_t *packet)
{
  WPAN_Evt_t *evt = &packet->evt;
  if(evt->plen < sizeof(WPAN_CsEvt_t)) return; // too short for either response shape
  if(evt->evtcode == WPAN_EVT_STATUS) {
    WPAN_CsEvt_t *status = (WPAN_CsEvt_t *)evt->payload;
    if(status->cmdcode != wpan.ble_opcode) return; // late answer of an abandoned command
    wpan.ble_status = status->status;
    wpan.ble_rsp_len = 0;
  }
  else {
    WPAN_CcEvt_t *complete = (WPAN_CcEvt_t *)evt->payload;
    if(complete->cmdcode != wpan.ble_opcode) return; // late answer of an abandoned command
    uint8_t len = evt->plen - WPAN_CC_SIZE; // return parameters, status byte first
    wpan.ble_status = len ? complete->payload[0] : 0;
    if(len) len--;
    if(len > WPAN_RSP_SIZE) len = WPAN_RSP_SIZE;
    memcpy(wpan.ble_rsp, &complete->payload[1], len);
    wpan.ble_rsp_len = len;
  }
  wpan.ble_done = true;
}

static void ble_event(WPAN_EvtPacket_t *packet)
{
  uint8_t code = packet->evt.evtcode;
  if(code == WPAN_EVT_STATUS || code == WPAN_EVT_COMPLETE) {
    ble_response(packet);
    return;
  }
  uint8_t next = (uint8_t)((wpan.event_head + 1) % WPAN_EVENT_LIMIT);
  if(next == wpan.event_tail) {
    // A dropped event still goes back to the pool, a leaked one would starve the CPU2.
    wpan.errors++;
    mm_release(packet);
    return;
  }
  wpan.event[wpan.event_head] = packet;
  wpan.event_head = next;
}

static void rx_handler(void *arg)
{
  unused(arg);
  uint32_t unmasked = ~IPCC->C1MR;
  if((IPCC->C2TOC1SR & WPAN_CH_SYS) && (unmasked & WPAN_CH_SYS)) {
    while(!list_empty(&sys_evt_queue)) {
      sys_event((WPAN_EvtPacket_t *)list_pop(&sys_evt_queue));
    }
    IPCC->C1SCR = WPAN_CH_SYS;
  }
  if((IPCC->C2TOC1SR & WPAN_CH_BLE) && (unmasked & WPAN_CH_BLE)) {
    while(!list_empty(&ble_evt_queue)) {
      ble_event((WPAN_EvtPacket_t *)list_pop(&ble_evt_queue));
    }
    IPCC->C1SCR = WPAN_CH_BLE;
  }
}

static void tx_handler(void *arg)
{
  unused(arg);
  uint32_t unmasked = (~IPCC->C1MR) >> 16;
  uint32_t idle = ~IPCC->C1TOC2SR;
  if((idle & WPAN_CH_SYS) && (unmasked & WPAN_CH_SYS)) {
    // The response overwrote the command in `sys_cmd_buffer`, no list header this time.
    IPCC->C1MR |= WPAN_CH_SYS << 16;
    wpan.sys_done = true;
  }
  if((idle & WPAN_CH_MM) && (unmasked & WPAN_CH_MM)) {
    IPCC->C1MR |= WPAN_CH_MM << 16;
    mm_flush();
    IPCC->C1SCR = WPAN_CH_MM << 16;
  }
}

//-------------------------------------------------------------------------------------------- Wait

// Cooperative wait shared by every command: the response comes from interrupt context.
static bool wait_flag(volatile bool *flag, uint32_t timeout_ms)
{
  uint64_t deadline = tick_keep(timeout_ms);
  while(!*flag) {
    if(tick_over(&deadline)) return false;
    let();
  }
  return true;
}

//------------------------------------------------------------------------------------------ Clocks

static status_t clock_wait(volatile const uint32_t *reg, uint32_t mask)
{
  uint64_t deadline = tick_keep(WPAN_CLOCK_TIMEOUT_ms);
  while(!(*reg & mask)) {
    if(tick_over(&deadline)) return ERR;
    let();
  }
  return OK;
}

// The radio needs HSE, its wakeup timer needs LSE. CPU2 drives them once booted.
static status_t radio_clocks(void)
{
  RCC->CR |= RCC_CR_HSEON;
  if(clock_wait(&RCC->CR, RCC_CR_HSERDY)) return ERR;
  PWR->CR1 |= PWR_CR1_DBP;
  if(clock_wait(&PWR->CR1, PWR_CR1_DBP)) return ERR;
  if(RTC_StartLSE()) return ERR; // running already in a build with the calendar
  RCC->CSR = (RCC->CSR & ~RCC_CSR_RFWKPSEL) | RCC_CSR_RFWKPSEL_0; // RF wakeup from LSE
  return OK;
}

//-------------------------------------------------------------------------------------------- Init

status_t WPAN_Start(IRQ_Priority_t priority)
{
  // CPU2 finds the reference table through the `IPCCDBA` option byte.
  uint32_t ipccdba = FLASH->IPCCBR & FLASH_IPCCBR_IPCCDBA;
  if((uint32_t)&ref_table != SRAM2A_BASE + (ipccdba << 2)) return ERR;
  if(radio_clocks()) return ERR;
  RCC->AHB3ENR |= RCC_AHB3ENR_IPCCEN;
  (void)RCC->AHB3ENR;
  // Tables the CPU2 walks on boot
  ref_table.p_device_info_table = &device_info_table;
  ref_table.p_ble_table = &ble_table;
  ref_table.p_thread_table = &thread_table;
  ref_table.p_lld_tests_table = &lld_tests_table;
  ref_table.p_ble_lld_table = &ble_lld_table;
  ref_table.p_sys_table = &sys_table;
  ref_table.p_mem_manager_table = &mm_table;
  ref_table.p_traces_table = &traces_table;
  ref_table.p_mac_802_15_4_table = &mac_table;
  ref_table.p_zigbee_table = &zigbee_table;
  list_init(&sys_evt_queue);
  list_init(&ble_evt_queue);
  list_init(&traces_queue);
  list_init(&free_buf_queue);
  list_init(&local_free_queue);
  sys_table.pcmd_buffer = (uint8_t *)&sys_cmd_buffer;
  sys_table.sys_queue = (uint8_t *)&sys_evt_queue;
  ble_table.pcmd_buffer = (uint8_t *)&ble_cmd_buffer;
  ble_table.pcs_buffer = ble_cs_buffer;
  ble_table.pevt_queue = (uint8_t *)&ble_evt_queue;
  ble_table.phci_acl_data_buffer = (uint8_t *)&ble_acl_buffer;
  mm_table.blepool = evt_pool;
  mm_table.blepoolsize = WPAN_POOL_SIZE;
  mm_table.pevt_free_buffer_queue = (uint8_t *)&free_buf_queue;
  mm_table.spare_ble_buffer = (uint8_t *)&ble_spare_buffer;
  mm_table.spare_sys_buffer = (uint8_t *)&sys_spare_buffer;
  mm_table.traces_evt_pool = NULL;
  mm_table.tracespoolsize = 0;
  traces_table.traces_queue = (uint8_t *)&traces_queue;
  // IPCC: event channels unmasked for receive, everything else stays masked.
  IPCC->C1MR = 0xFFFFFFFFu;
  IPCC->C1SCR = 0x0000003Fu;
  IPCC->C1CR = IPCC_C1CR_RXOIE | IPCC_C1CR_TXFIE;
  IPCC->C1MR &= ~(WPAN_CH_SYS | WPAN_CH_BLE);
  IRQ_EnableIPCC(priority, rx_handler, tx_handler, NULL);
  // Release CPU2: IPCC clock on its bus, wakeup path out of standby, then boot
  RCC->C2AHB3ENR |= RCC_C2AHB3ENR_IPCCEN;
  EXTI->RTSR2 |= EXTI_RTSR2_RT41;
  EXTI->C2EMR2 |= EXTI_C2EMR2_EM41; // event wakeup for the CPU2
  __SEV();
  __WFE();
  PWR->CR4 |= PWR_CR4_C2BOOT;
  wpan.init = true;
  if(!wait_flag(&wpan.ready, WPAN_READY_TIMEOUT_ms)) return ERR;
  if(wpan.fw == WPAN_Fw_Fus) {
    // Any programmer contact leaves FUS engaged across resets; only the start command
    // brings the stack back. No response comes: CPU2 restarts and reports ready again.
    wpan.ready = false;
    sys_cmd_buffer.type = WPAN_TYPE_SYS_CMD;
    sys_cmd_buffer.cmd.cmdcode = WPAN_FUS_START_WS;
    sys_cmd_buffer.cmd.plen = 0;
    IPCC->C1SCR = WPAN_CH_SYS << 16;
    if(!wait_flag(&wpan.ready, WPAN_READY_TIMEOUT_ms)) return ERR;
  }
  return wpan.fw == WPAN_Fw_Wireless ? OK : ERR;
}

WPAN_Fw_t WPAN_Firmware(void)
{
  return wpan.fw;
}

uint32_t WPAN_StackVersion(void)
{
  return device_info_table.WirelessFwInfoTable.Version;
}

uint16_t WPAN_Errors(void)
{
  return wpan.errors;
}

//---------------------------------------------------------------------------------- System channel

static uint8_t sys_cmd(uint16_t opcode, const void *param, uint8_t len)
{
  if(!wpan.init) return 0xFF;
  sys_cmd_buffer.type = WPAN_TYPE_SYS_CMD;
  sys_cmd_buffer.cmd.cmdcode = opcode;
  sys_cmd_buffer.cmd.plen = len;
  if(len) memcpy(sys_cmd_buffer.cmd.payload, param, len);
  wpan.sys_done = false;
  IPCC->C1SCR = WPAN_CH_SYS << 16;
  c1mr_clear(WPAN_CH_SYS << 16);
  if(!wait_flag(&wpan.sys_done, WPAN_CMD_TIMEOUT_ms)) return 0xFF;
  // The response starts at the packet type, the list header is not rewritten
  WPAN_Evt_t *evt = (WPAN_Evt_t *)(&sys_cmd_buffer.type + 1);
  return ((WPAN_CcEvt_t *)evt->payload)->payload[0];
}

uint8_t WPAN_BleStackInit(uint16_t attributes, uint16_t services, uint16_t values)
{
  WPAN_BleInit_t param;
  memset(&param, 0, sizeof(param)); // zeroed fields keep the CPU2 defaults
  param.NumAttrRecord = attributes;
  param.NumAttrServ = services;
  param.AttrValueArrSize = values;
  param.NumOfLinks = WPAN_BLE_LINKS;
  param.ExtendedPacketLengthEnable = 1;
  // Prepare-write and memory block counts from the `ble_bufsize.h` formulas
  param.PrWriteListSize = (uint8_t)(div_ceil(WPAN_BLE_ATT_MTU, 23 - 5) * 2);
  param.MblockCount = (uint8_t)(param.PrWriteListSize +
    div_ceil(WPAN_BLE_ATT_MTU + 4u, 32) + 1 + WPAN_BLE_LINKS +
    (div_ceil(WPAN_BLE_ATT_MTU + 4u, 32) + 2) * WPAN_BLE_LINKS + 1);
  param.AttMtu = WPAN_BLE_ATT_MTU;
  param.PeripheralSca = 500;
  param.CentralSca = 0;
  param.LsSource = 0; // no calibration, SoC device, LSE
  param.MaxConnEventLength = 0xFFFFFFFFu;
  param.HsStartupTime = 0x148; // ~800us, the Nucleo crystal
  param.ViterbiEnable = 1;
  param.Options = 0; // LL with host, service-change descriptor, name RW, no extended adv
  param.min_tx_power = -40;
  param.max_tx_power = 6;
  param.ble_core_version = WPAN_BLE_CORE_5_4;
  return sys_cmd(WPAN_SHCI_BLE_INIT, &param, sizeof(param));
}

//------------------------------------------------------------------------------------- BLE channel

void WPAN_FlashEraseActivity(bool active)
{
  if(!wpan.init) return; // no radio to warn
  uint8_t on = active ? 1 : 0;
  (void)sys_cmd(WPAN_FLASH_ERASE_ACTIVITY, &on, 1);
}

uint8_t WPAN_BleCmd(uint16_t opcode, const uint8_t *param, uint8_t len,
  uint8_t *rsp, uint8_t limit, uint8_t *rsp_len)
{
  if(rsp_len) *rsp_len = 0;
  if(!wpan.init) return 0xFF;
  if(wpan.ble_lost) {
    // The buffer belongs to the CPU2 until the late answer lands, refuse until then
    if(!wpan.ble_done) return 0xFF;
    wpan.ble_lost = false;
  }
  ble_cmd_buffer.type = WPAN_TYPE_BLE_CMD;
  ble_cmd_buffer.cmd.cmdcode = opcode;
  ble_cmd_buffer.cmd.plen = len;
  if(len) memcpy(ble_cmd_buffer.cmd.payload, param, len);
  wpan.ble_opcode = opcode;
  wpan.ble_done = false;
  IPCC->C1SCR = WPAN_CH_BLE << 16;
  if(!wait_flag(&wpan.ble_done, WPAN_CMD_TIMEOUT_ms)) {
    wpan.ble_lost = true;
    return 0xFF;
  }
  uint8_t copy = wpan.ble_rsp_len < limit ? wpan.ble_rsp_len : limit;
  if(rsp && copy) memcpy(rsp, wpan.ble_rsp, copy);
  if(rsp_len) *rsp_len = copy;
  return wpan.ble_status;
}

uint8_t *WPAN_BleEvent(void)
{
  if(wpan.event_head == wpan.event_tail) return NULL;
  WPAN_EvtPacket_t *packet = wpan.event[wpan.event_tail];
  wpan.event_tail = (uint8_t)((wpan.event_tail + 1) % WPAN_EVENT_LIMIT);
  return &packet->type;
}

void WPAN_BleEventDone(uint8_t *event)
{
  if(event) mm_release((WPAN_EvtPacket_t *)(event - sizeof(WPAN_Header_t)));
}

//-------------------------------------------------------------------------------------------------
