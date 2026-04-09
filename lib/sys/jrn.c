// lib/sys/jrn.c

#include "jrn.h"
#include "rtc.h"
#include "vrts.h"

//------------------------------------------------------------------------------------ Internal

static PDB_t *jrn;

static bool JRN_CodeFilter(const void *record, void *ctx)
{
  return ((const JRN_t *)record)->code == *(uint16_t *)ctx;
}

// Current time as journal sort key: RTC unix timestamp, fallback to boot tick
static uint32_t JRN_GetTime(void)
{
  if(RtcInit) {
    RTC_Datetime_t dt = RTC_Datetime();
    return (uint32_t)RTC_DatetimeToUnix(&dt);
  }
  return (uint32_t)tick_keep(0);
}

//----------------------------------------------------------------------------------------- API

status_t JRN_Init(PDB_t *pdb)
{
  if(!pdb) {
    JRN_LOG("Init fault: pdb pointer is NULL");
    return ERR;
  }
  jrn = pdb;
  jrn->payload_size = sizeof(JRN_t);
  status_t st = PDB_Init(jrn);
  if(st) JRN_LOG("Init fault: underlying PDB failed");
  return st;
}

status_t JRN_Insert(uint16_t code)
{
  JRN_t record = { .time = JRN_GetTime(), .code = code };
  JRN_LOG("Insert code:%u", code);
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
  q.filter = &JRN_CodeFilter;
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
