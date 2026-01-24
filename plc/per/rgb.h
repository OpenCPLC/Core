#ifndef RGB_H_
#define RGB_H_

#include "gpio.h"
#include "extstr.h"

//-------------------------------------------------------------------------------------------------

typedef enum {
  RGB_Off = 0,
  RGB_Red = 1,
  RGB_Green = 2,
  RGB_Blue = 3,
  RGB_Yellow = 4,
  RGB_Cyan = 5,
  RGB_Magenta = 6,
  RGB_White = 7
} RGB_e;

#define RGB_END_COLOR RGB_White

typedef struct {
  GPIO_t *red;
  GPIO_t *green;
  GPIO_t *blue;
  RGB_e state;
  uint16_t blink_ms;
  uint64_t tick;
  bool blink_on;
  bool one_shot;
} RGB_t;

void RGB_Init(RGB_t *rgb);
void RGB_Set(RGB_t *rgb, RGB_e color);
void RGB_Rst(RGB_t *rgb);
void RGB_Tgl(RGB_t *rgb);
void RGB_Loop(RGB_t *rgb);

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
} RGB_Hash_e;

void LED_Focus(RGB_t *rgb);
void LED_Set(RGB_e color);
void LED_Rst(void);
void LED_Blink_ON(uint16_t ms);
void LED_Blink_OFF(void);
void LED_OneShoot(RGB_e color, uint16_t ms);
void LED_Bash(char **argv, uint16_t argc);

//-------------------------------------------------------------------------------------------------
#endif