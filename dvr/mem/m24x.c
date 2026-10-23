// dvr/mem/m24x.c

#include "m24x.h"

#include <string.h>

#include "vrts.h"

//------------------------------------------------------------------------------------------ Config

// Longest wait for the bus and for one transfer on it: 255B at 100kHz take 23ms
#define M24X_BUS_ms 25

// Data of one write transfer at most [B], its frame on the stack: a longer page goes in parts
#define M24X_BURST 64u

//---------------------------------------------------------------------------------------- Internal

// Address in one byte up to 2kB, in two past it
static uint8_t address_bytes(const M24X_t *m24x) { return m24x->size > 2048 ? 2 : 1; }

// Address bits above the address bytes pick the block, each under its own device address
static uint8_t device_of(const M24X_t *m24x, uint32_t addr)
{
  uint8_t address = m24x->address ? m24x->address : M24X_ADDRESS;
  return address | (uint8_t)(addr >> (8 * address_bytes(m24x)));
}

// Address bytes of `addr` into `frame`, high first; returns their count
static uint8_t address_put(const M24X_t *m24x, uint32_t addr, uint8_t *frame)
{
  uint8_t count = address_bytes(m24x);
  if(count == 2) *frame++ = (uint8_t)(addr >> 8);
  *frame = (uint8_t)addr;
  return count;
}

// Run of `len` bytes from `addr` inside the memory, of a known page
static bool run_valid(const M24X_t *m24x, uint32_t addr, const void *data, uint16_t len)
{
  if(!m24x->i2c || !data || !m24x->page_size) return false;
  return addr <= m24x->size && len <= m24x->size - addr;
}

// Bus shared with other devices: wait for it to free up, `false` when it stays taken
static bool bus_wait(I2C_Master_t *i2c) {
  return !timeout(M24X_BUS_ms, WAIT_&I2C_Master_IsFree, i2c);
}

// Transfer waited out to its end, `true` when the memory acknowledged every byte
static bool transfer_acked(I2C_Master_t *i2c) { return bus_wait(i2c) && !I2C_Master_Nack(i2c); }

// Part of a page in one frame: its address bytes, then the data
static bool part_send(M24X_t *m24x, uint32_t addr, const uint8_t *data, uint16_t len)
{
  uint8_t frame[2 + M24X_BURST];
  uint8_t count = address_put(m24x, addr, frame);
  memcpy(&frame[count], data, len);
  if(!bus_wait(m24x->i2c)) return false;
  if(I2C_Master_Write(m24x->i2c, device_of(m24x, addr), frame, count + len)) return false;
  return transfer_acked(m24x->i2c);
}

// The memory programs a part after its stop and refuses its address until done.
// One still busy past twice the longest write cycle does not answer.
static bool part_programmed(M24X_t *m24x)
{
  uint64_t start = tick_now();
  uint8_t byte;
  do {
    delay(1);
    if(!bus_wait(m24x->i2c)) return false;
    if(I2C_Master_Read(m24x->i2c, device_of(m24x, 0), &byte, 1)) return false;
    if(transfer_acked(m24x->i2c)) return true;
  } while(tick_diff(start) < 2 * M24X_WRITE_ms);
  return false;
}

// Bytes part by part, each programmed before the next goes out
static status_t write_parts(M24X_t *m24x, uint32_t addr, const uint8_t *data, uint16_t len)
{
  while(len) {
    // A write wraps around inside its page: the bytes are cut at every page end,
    // and at the burst one frame holds.
    uint16_t part = minv(len, m24x->page_size - addr % m24x->page_size, M24X_BURST);
    if(!part_send(m24x, addr, data, part)) return ERR;
    if(!part_programmed(m24x)) return ERR;
    addr += part;
    data += part;
    len -= part;
  }
  return OK;
}

//--------------------------------------------------------------------------------------------- API

void M24X_Init(M24X_t *m24x)
{
  if(!m24x->wc) return;
  m24x->wc->mode = GPIO_Mode_Output;
  GPIO_Init(m24x->wc);
}

status_t M24X_Read(M24X_t *m24x, uint32_t addr, void *data, uint16_t len)
{
  if(!run_valid(m24x, addr, data, len)) return ERR;
  I2C_Master_t *i2c = m24x->i2c;
  uint8_t *bytes = data;
  while(len) {
    // A read runs on across pages and blocks, so only the transfer limit splits it
    uint16_t part = minv(len, I2C_TRANSFER_MAX);
    uint8_t frame[2];
    uint8_t count = address_put(m24x, addr, frame);
    if(!bus_wait(i2c)) return ERR;
    if(I2C_Master_WriteRead(i2c, device_of(m24x, addr), frame, count, bytes, part)) return ERR;
    if(!transfer_acked(i2c)) return ERR;
    addr += part;
    bytes += part;
    len -= part;
  }
  return OK;
}

status_t M24X_Write(M24X_t *m24x, uint32_t addr, const void *data, uint16_t len)
{
  if(!run_valid(m24x, addr, data, len)) return ERR;
  if(m24x->wc) GPIO_Set(m24x->wc);
  status_t err = write_parts(m24x, addr, data, len);
  if(m24x->wc) GPIO_Rst(m24x->wc);
  return err;
}

//-------------------------------------------------------------------------------------------------
