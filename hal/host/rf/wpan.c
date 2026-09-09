// hal/host/rf/wpan.c

// Host model of the CPU2 mailbox: the `wpan_wb.h` API backed by queues a test controls.
// Commands are captured, responses come scripted, events arrive on injection.

#include "../../stm32wb/rf/wpan_wb.h"

#include <string.h>

#include "wpan_host.h"

//------------------------------------------------------------------------------------------- Model

#define HOST_QUEUE 64
#define HOST_EVENT_SIZE 260

uint8_t HOST_UID64[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

typedef struct {
  uint16_t opcode;
  uint8_t len;
  uint8_t param[255];
} host_cmd_t;

typedef struct {
  uint8_t status;
  uint8_t len;
  uint8_t rsp[WPAN_RSP_SIZE];
} host_rsp_t;

typedef struct {
  uint8_t data[HOST_EVENT_SIZE];
} host_evt_t;

static struct {
  host_cmd_t cmd[HOST_QUEUE];
  uint8_t cmd_head, cmd_tail;
  host_rsp_t rsp[HOST_QUEUE];
  uint8_t rsp_head, rsp_tail;
  host_evt_t evt[HOST_QUEUE];
  uint8_t evt_head, evt_tail;
  uint16_t attributes;
  uint16_t values;
} host;

//------------------------------------------------------------------------------------------- Hooks

void WPAN_HostReset(void)
{
  memset(&host, 0, sizeof(host));
}

void WPAN_HostRespond(uint8_t status, const uint8_t *rsp, uint8_t len)
{
  host_rsp_t *slot = &host.rsp[host.rsp_head];
  host.rsp_head = (uint8_t)((host.rsp_head + 1) % HOST_QUEUE);
  slot->status = status;
  slot->len = len > WPAN_RSP_SIZE ? WPAN_RSP_SIZE : len;
  if(slot->len) memcpy(slot->rsp, rsp, slot->len);
}

uint16_t WPAN_HostCommand(uint8_t *param, uint8_t *len)
{
  if(host.cmd_head == host.cmd_tail) return 0;
  host_cmd_t *slot = &host.cmd[host.cmd_tail];
  host.cmd_tail = (uint8_t)((host.cmd_tail + 1) % HOST_QUEUE);
  if(param) memcpy(param, slot->param, slot->len);
  if(len) *len = slot->len;
  return slot->opcode;
}

void WPAN_HostEvent(const uint8_t *event, uint8_t len)
{
  host_evt_t *slot = &host.evt[host.evt_head];
  host.evt_head = (uint8_t)((host.evt_head + 1) % HOST_QUEUE);
  memcpy(slot->data, event, len);
}

uint16_t WPAN_HostAttributes(void)
{
  return host.attributes;
}

uint16_t WPAN_HostValues(void)
{
  return host.values;
}

//------------------------------------------------------------------------------------ Modelled API

// A command finding no scripted response succeeds with no return parameters
static host_rsp_t *pop_response(void)
{
  static host_rsp_t clean;
  if(host.rsp_head == host.rsp_tail) return &clean;
  host_rsp_t *slot = &host.rsp[host.rsp_tail];
  host.rsp_tail = (uint8_t)((host.rsp_tail + 1) % HOST_QUEUE);
  return slot;
}

status_t WPAN_Start(IRQ_Priority_t priority)
{
  unused(priority);
  return OK;
}

WPAN_Fw_t WPAN_Firmware(void)
{
  return WPAN_Fw_Wireless;
}

uint32_t WPAN_StackVersion(void)
{
  return 0x01180003; // 1.24.0.3
}

uint16_t WPAN_Errors(void)
{
  return 0;
}

uint8_t WPAN_BleStackInit(uint16_t attributes, uint16_t services, uint16_t values)
{
  unused(services);
  host.attributes = attributes;
  host.values = values;
  return pop_response()->status;
}

uint8_t WPAN_BleCmd(uint16_t opcode, const uint8_t *param, uint8_t len,
  uint8_t *rsp, uint8_t limit, uint8_t *rsp_len)
{
  host_cmd_t *slot = &host.cmd[host.cmd_head];
  host.cmd_head = (uint8_t)((host.cmd_head + 1) % HOST_QUEUE);
  slot->opcode = opcode;
  slot->len = len;
  if(len) memcpy(slot->param, param, len);
  host_rsp_t *answer = pop_response();
  uint8_t copy = answer->len < limit ? answer->len : limit;
  if(rsp && copy) memcpy(rsp, answer->rsp, copy);
  if(rsp_len) *rsp_len = copy;
  return answer->status;
}

uint8_t *WPAN_BleEvent(void)
{
  if(host.evt_head == host.evt_tail) return NULL;
  return host.evt[host.evt_tail].data;
}

void WPAN_BleEventDone(uint8_t *event)
{
  if(event) host.evt_tail = (uint8_t)((host.evt_tail + 1) % HOST_QUEUE);
}

//-------------------------------------------------------------------------------------------------
