// lib/sys/jrn.c

#include "jrn.h"

#include "rtc.h"

//------------------------------------------------------------------------------------------ Config

// Earliest time a set clock shows, 2020-01-01: an RTC starts at 2000-01-01 [s from 1970 UTC]
#define JRN_CLOCK_MIN_ts 1577836800u

//---------------------------------------------------------------------------------------- Internal

static PDB_t *jrn;

static bool code_filter(const void *record, void *ctx)
{
  return ((const JRN_t *)record)->code == *(uint16_t *)ctx;
}

// RTC unix timestamp, `0` while the clock is not set: a tick restarts from zero at every boot
static uint32_t time_now(void)
{
  if(!RtcInit) return 0;
  uint64_t now = RTC_Timestamp();
  return now < JRN_CLOCK_MIN_ts ? 0 : (uint32_t)now;
}

//--------------------------------------------------------------------------------------------- API

status_t JRN_Init(PDB_t *pdb)
{
  if(!pdb) {
    JRN_LOG("Init fault: pdb pointer is NULL");
    return ERR;
  }
  jrn = pdb;
  jrn->record_size = sizeof(JRN_t);
  status_t st = PDB_Init(jrn);
  if(st) JRN_LOG("Init fault: underlying PDB failed");
  return st;
}

status_t JRN_Insert(uint16_t code, uint16_t data)
{
  JRN_t record = { .time = time_now(), .code = code, .data = data };
  JRN_LOG("Insert code:%u data:%u", code, data);
  return PDB_Insert(jrn, &record);
}

status_t JRN_Delete(void)
{
  JRN_LOG("Delete");
  return PDB_Delete(jrn);
}

uint32_t JRN_Select(const PDB_Query_t *query, MBB_t *mbb)
{
  mbb->size = 0;
  if(!mbb->limit) return 0;
  uint32_t max = mbb->limit / sizeof(JRN_t);
  uint32_t count = PDB_Select(jrn, query, mbb->buffer, max);
  mbb->size = count * sizeof(JRN_t);
  JRN_LOG("Select count:%u", count);
  return count;
}

uint32_t JRN_SelectCode(const PDB_Query_t *query, uint16_t code, MBB_t *mbb)
{
  mbb->size = 0;
  if(!mbb->limit) return 0;
  PDB_Query_t q = *query;
  uint16_t code_local = code;
  q.filter = &code_filter;
  q.filter_ctx = &code_local;
  uint32_t max = mbb->limit / sizeof(JRN_t);
  uint32_t count = PDB_Select(jrn, &q, mbb->buffer, max);
  mbb->size = count * sizeof(JRN_t);
  JRN_LOG("Select code:%u count:%u", code, count);
  return count;
}

uint32_t JRN_Count(const PDB_Query_t *query)
{
  return PDB_Count(jrn, query);
}

void JRN_GetTimestamp(MBB_t *mbb)
{
  JRN_t *record = (JRN_t *)mbb->buffer;
  uint32_t *ts = (uint32_t *)mbb->buffer;
  uint16_t count = mbb->size / sizeof(JRN_t);
  for(uint16_t i = 0; i < count; i++) {
    ts[i] = record[i].time;
  }
  mbb->size = count * sizeof(uint32_t);
}

//-------------------------------------------------------------------------------------------------
