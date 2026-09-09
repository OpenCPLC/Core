// dvr/gnss.c

#include "gnss.h"

#include <string.h>

#include "vrts.h"
#include "xdef.h"
#include "xstring.h"

#define GNSS_BOOT_ms 200 // Module up after power on
#define GNSS_OFF_ms 500  // Module without power in a power cycle

//---------------------------------------------------------------------------------------- Internal

// Field `index` of a comma separated sentence copied into `out`, `false` when missing or too long
static bool field(const char *line, uint8_t index, char *out, uint8_t limit)
{
  for(; index; index--) {
    line = strchr(line, ',');
    if(!line) return false;
    line++;
  }
  const char *end = line;
  while(*end && *end != ',' && *end != '*') end++;
  if(end - line >= limit) return false;
  memcpy(out, line, end - line);
  out[end - line] = 0;
  return true;
}

// XOR of the characters between `$` and `*` against the two hex digits behind `*`
static bool checksum_ok(const char *line)
{
  if(*line != '$') return false;
  uint8_t sum = 0;
  const char *c = line + 1;
  while(*c && *c != '*') sum ^= (uint8_t)*c++;
  if(*c != '*' || !c[1] || !c[2]) return false;
  char hex[] = { '0', 'x', c[1], c[2], 0 };
  return str_is_u32(hex) && str_to_int(hex) == sum;
}

// `ddmm.mmmm` to decimal degrees, the sign from the hemisphere letter
static bool coordinate(const char *text, char hemisphere, char positive, char negative,
  float *out)
{
  if(!str_is_f32(text)) return false;
  float value = str_to_f32(text);
  float degrees = (float)(int32_t)(value / 100);
  float result = degrees + (value - 100 * degrees) / 60;
  if(hemisphere == negative) result = -result;
  else if(hemisphere != positive) return false;
  *out = result;
  return true;
}

static void power_on(GNSS_t *gnss)
{
  GPIO_Set(gnss->enable);
  delay(GNSS_BOOT_ms);
  if(gnss->reset_ms) gnss->_reset_tick = tick_keep(gnss->reset_ms);
}

//--------------------------------------------------------------------------------------------- API

bool GNSS_Parse(const char *line, float *latitude, float *longitude)
{
  char talker[8], lat[16], ns[4], lon[16], ew[4], status[4];
  if(!checksum_ok(line)) return false;
  if(!field(line, 0, talker, sizeof(talker)) || strlen(talker) != 6) return false;
  if(strcmp(talker + 3, "GLL")) return false;
  if(!field(line, 6, status, sizeof(status)) || status[0] != 'A') return false;
  if(!field(line, 1, lat, sizeof(lat)) || !field(line, 2, ns, sizeof(ns))) return false;
  if(!field(line, 3, lon, sizeof(lon)) || !field(line, 4, ew, sizeof(ew))) return false;
  float north, east;
  if(!coordinate(lat, ns[0], 'N', 'S', &north)) return false;
  if(!coordinate(lon, ew[0], 'E', 'W', &east)) return false;
  *latitude = north;
  *longitude = east;
  return true;
}

void GNSS_Init(GNSS_t *gnss)
{
  if(!gnss->expiry_ms) gnss->expiry_ms = GNSS_EXPIRY_ms;
  if(!gnss->reset_ms) gnss->reset_ms = GNSS_RESET_ms;
  gnss->latitude = NaN;
  gnss->longitude = NaN;
  UART_Init(gnss->uart);
  if(!gnss->enable) return;
  gnss->enable->mode = GPIO_Mode_Output;
  GPIO_Init(gnss->enable);
  power_on(gnss);
}

void GNSS_Reset(GNSS_t *gnss)
{
  if(!gnss->enable) return;
  GPIO_Rst(gnss->enable);
  delay(GNSS_OFF_ms);
  UART_Clear(gnss->uart);
  power_on(gnss);
}

bool GNSS_Loop(GNSS_t *gnss)
{
  if(tick_over(&gnss->_expiry_tick)) {
    gnss->latitude = NaN;
    gnss->longitude = NaN;
  }
  if(tick_over(&gnss->_reset_tick)) GNSS_Reset(gnss);
  bool fix = false;
  char *frame;
  while((frame = UART_ReadString(gnss->uart))) {
    // A frame closes on a gap of the line, so a burst of sentences comes in one.
    for(char *sentence = strchr(frame, '$'); sentence; sentence = strchr(sentence + 1, '$')) {
      if(GNSS_Parse(sentence, &gnss->latitude, &gnss->longitude)) fix = true;
    }
  }
  if(!fix) return false;
  gnss->_expiry_tick = tick_keep(gnss->expiry_ms);
  if(gnss->enable) gnss->_reset_tick = tick_keep(gnss->reset_ms);
  return true;
}

//-------------------------------------------------------------------------------------------------
