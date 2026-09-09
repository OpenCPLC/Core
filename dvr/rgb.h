// dvr/rgb.h

#ifndef RGB_H_
#define RGB_H_

#include "gpio.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef RGB_BASH
  // Shell command `led`/`rgb` with the color-name table; costs flash, off unless asked for
  #define RGB_BASH OFF
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  RGB_Off = 0,
  RGB_Red = 1,
  RGB_Green = 2,
  RGB_Blue = 3,
  RGB_Yellow = 4,
  RGB_Cyan = 5,
  RGB_Magenta = 6,
  RGB_White = 7
} RGB_Color_t;

#define RGB_END_COLOR RGB_White

/**
 * @brief RGB LED on three GPIO lines, any of them optional.
 * @param[in] red Red pin (`NULL` = absent)
 * @param[in] green Green pin (`NULL` = absent)
 * @param[in] blue Blue pin (`NULL` = absent)
 * @param[in] state Color applied by `RGB_Init`, tracks the LED afterwards
 * @param[in] blink_ms Blink half-period for `RGB_Loop`, `0` = steady
 * Internal:
 * @param _tick Blink deadline
 * @param _blink_on Lit half of the blink period
 * @param _one_shot Blink disarms itself after one dark edge
 * @param _color Last non-off color, what `RGB_Tgl` lights again
 */
typedef struct {
  GPIO_t *red;
  GPIO_t *green;
  GPIO_t *blue;
  RGB_Color_t state;
  uint16_t blink_ms;
  // internal
  uint64_t _tick;
  bool _blink_on;
  bool _one_shot;
  RGB_Color_t _color;
} RGB_t;

//--------------------------------------------------------------------------------------------- API

// Init pins, apply `state` and take the `RGB_Bash` focus
void RGB_Init(RGB_t *rgb);

/**
 * @brief Set color.
 * @param[in,out] rgb LED instance
 * @param[in] color Color to apply, `RGB_Off` turns the LED off
 */
void RGB_Set(RGB_t *rgb, RGB_Color_t color);

// Turn the LED off
void RGB_Rst(RGB_t *rgb);

// Toggle between off and the last non-off color
void RGB_Tgl(RGB_t *rgb);

// Step the blink engine, call periodically while `blink_ms` is in use
void RGB_Loop(RGB_t *rgb);

/**
 * @brief Start blinking the current color.
 * @param[in,out] rgb LED instance
 * @param[in] ms Blink half-period
 */
void RGB_Blink(RGB_t *rgb, uint16_t ms);

// Stop blinking, the LED stays in its steady color
void RGB_BlinkOff(RGB_t *rgb);

/**
 * @brief Light a color for one blink period, then back off.
 * @param[in,out] rgb LED instance
 * @param[in] color Color to flash
 * @param[in] ms Flash duration
 */
void RGB_Shot(RGB_t *rgb, RGB_Color_t color, uint16_t ms);

//------------------------------------------------------------------------------------------- Shell
#if(RGB_BASH)

typedef enum {
  RGB_Hash_Led = 193498042,
  RGB_Hash_Rgb = 193504640,
  RGB_Hash_Blink = 254371765,
  RGB_Hash_Shot = 2090723395,
  RGB_Hash_On = 5863682,
  RGB_Hash_Off = 193501344,
  RGB_Hash_Red = 193504576,
  RGB_Hash_Green = 260512342,
  RGB_Hash_Blue = 2090117005,
  RGB_Hash_Yellow = 696252129,
  RGB_Hash_Cyan = 2090166448,
  RGB_Hash_Magenta = 3021013506,
  RGB_Hash_White = 279132550
} RGB_Hash_t;

// Pick the LED `RGB_Bash` drives; `RGB_Init` focuses its instance already
void RGB_Focus(RGB_t *rgb);

/**
 * @brief Shell handler for the focused LED, register with `CMD_AddCommand`.
 *   Usage: `led <color>`, `led blink on <ms>`, `led blink off`, `led shot <color> [ms]`;
 *   always prints the resulting state.
 * @param[in] argv Tokenized command, `argv[0]` is `led` or `rgb`
 * @param[in] argc Token count
 */
void RGB_Bash(char **argv, uint16_t argc);

#endif
//-------------------------------------------------------------------------------------------------
#endif
