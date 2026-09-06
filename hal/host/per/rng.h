// hal/host/per/rng.h

#ifndef RNG_H_
#define RNG_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

//------------------------------------------------------------------------------------------- Types

// The target clock routes, recorded and ignored

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

//--------------------------------------------------------------------------------------------- API

// Open the OS entropy source; `RNG_Run` and `RNG_Fill` open it on their own when needed
void RNG_Init(RNG_Source_t source, RNG_Divider_t div);

// One random word, or a buffer of random bytes
uint32_t RNG_Run(void);
void RNG_Fill(uint8_t *buf, uint16_t len);

/**
 * @brief Random value in `[min, max)`.
 * @param[in] min Lowest value, inclusive
 * @param[in] max Bound, exclusive
 * @return Random value, `min` when the range is empty
 */
int32_t rng(int32_t min, int32_t max);

//-------------------------------------------------------------------------------------------------
#endif
