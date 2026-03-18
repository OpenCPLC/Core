// hal/host/rng.h

#ifndef RNG_H_
#define RNG_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

//------------------------------------------------------------------------------------------------- Compatibility (stubs)

typedef enum {
  RNG_Source_Void = 0,
  RNG_Source_HSI16 = 1,
  RNG_Source_Sysclk = 2,
  RNG_Source_PLLQ = 3
} RNG_Source_t;

typedef enum {
  RNG_Divider_1 = 0,
  RNG_Divider_2 = 1,
  RNG_Divider_4 = 2,
  RNG_Divider_8 = 3
} RNG_Divider_t;

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Get random 32-bit value.
 * @return Random number
 */
uint32_t RNG_Run(void);

/**
 * @brief Initialize RNG (seeds from system entropy).
 * @param[in] source Ignored on desktop
 * @param[in] div Ignored on desktop
 */
void RNG_Init(RNG_Source_t source, RNG_Divider_t div);

/**
 * @brief Get random value in range [min, max).
 * @param[in] min Minimum value (inclusive)
 * @param[in] max Maximum value (exclusive)
 * @return Random number in range
 */
int32_t rng(int32_t min, int32_t max);

/**
 * @brief Fill buffer with random bytes.
 * @param[out] buf Buffer to fill
 * @param[in] len Buffer length
 */
void RNG_Fill(uint8_t *buf, uint16_t len);

//-------------------------------------------------------------------------------------------------

#endif