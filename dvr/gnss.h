// dvr/gnss.h

#ifndef GNSS_H_
#define GNSS_H_

#include <stdbool.h>
#include <stdint.h>

#include "gpio.h"
#include "uart.h"

//------------------------------------------------------------------------------------------ Config

#ifndef GNSS_EXPIRY_ms
  // Time without a fix after which the position turns `NaN`, the default of `expiry_ms`
  #define GNSS_EXPIRY_ms 5000
#endif

#ifndef GNSS_RESET_ms
  // Time without a fix after which the module is power cycled, the default of `reset_ms`
  #define GNSS_RESET_ms 30000
#endif

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief GNSS receiver on a UART talking NMEA 0183. The position comes from the `GLL`
 *   sentence, sent by every GPS, GLONASS and Galileo talker.
 *   A frame of the port may hold a burst of sentences, each of them is parsed.
 * @param[in] uart Port of the receiver, `GNSS_Init` initializes it
 * @param[in] enable Supply or enable pin of the module, `NULL` = always on
 * @param[in] expiry_ms Time without a fix after which the position turns `NaN`, `0` = default
 * @param[in] reset_ms Time without a fix after which the module is power cycled, `0` = default
 * @param[out] latitude Decimal degrees, north positive, `NaN` without a fix
 * @param[out] longitude Decimal degrees, east positive, `NaN` without a fix
 * Internal:
 * @param _expiry_tick Deadline of the position
 * @param _reset_tick Deadline of the power cycle
 */
typedef struct {
  UART_t *uart;
  GPIO_t *enable;
  uint32_t expiry_ms;
  uint32_t reset_ms;
  float latitude;
  float longitude;
  // internal
  uint64_t _expiry_tick;
  uint64_t _reset_tick;
} GNSS_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize the pin and the port, power the module on and give it time to boot.
 * @param[in,out] gnss Receiver
 */
void GNSS_Init(GNSS_t *gnss);

/**
 * @brief Power cycle the module over `enable`, its stale sentences dropped.
 * @param[in,out] gnss Receiver
 */
void GNSS_Reset(GNSS_t *gnss);

/**
 * @brief Take every frame waiting on the port and keep the position current.
 *   Runs in the thread that owns the port, the frames live on its heap.
 * @param[in,out] gnss Receiver
 * @return `true` when a fix came in with this call
 */
bool GNSS_Loop(GNSS_t *gnss);

/**
 * @brief Position out of one NMEA sentence.
 * @param[in] line Sentence from `$` through its checksum, whatever follows is ignored
 * @param[out] latitude Decimal degrees, north positive
 * @param[out] longitude Decimal degrees, east positive
 * @return `true` for a `GLL` sentence with a valid fix and a good checksum
 */
bool GNSS_Parse(const char *line, float *latitude, float *longitude);

//-------------------------------------------------------------------------------------------------
#endif
