// dvr/rgb.c

#include "rgb.h"
#include "vrts.h"
#if(RGB_BASH)
#include "cmd.h"
#include "dbg.h"
#include "xstring.h"
#endif

//-------------------------------------------------------------------------------------------------

#if(RGB_BASH)
static RGB_t *rgb_focus; // Target of `RGB_Bash`, last `RGB_Init` wins
#endif

static void set_red(RGB_t *rgb) { if(rgb->red) GPIO_Set(rgb->red); }
static void rst_red(RGB_t *rgb) { if(rgb->red) GPIO_Rst(rgb->red); }
static void set_green(RGB_t *rgb) { if(rgb->green) GPIO_Set(rgb->green); }
static void rst_green(RGB_t *rgb) { if(rgb->green) GPIO_Rst(rgb->green); }
static void set_blue(RGB_t *rgb) { if(rgb->blue) GPIO_Set(rgb->blue); }
static void rst_blue(RGB_t *rgb) { if(rgb->blue) GPIO_Rst(rgb->blue); }

// Drive the pins alone, `state` stays untouched: the blink engine uses it as the target
static void apply(RGB_t *rgb, RGB_Color_t color)
{
  switch(color) {
    case RGB_Off: rst_red(rgb); rst_green(rgb); rst_blue(rgb); break;
    case RGB_Red: set_red(rgb); rst_green(rgb); rst_blue(rgb); break;
    case RGB_Green: rst_red(rgb); set_green(rgb); rst_blue(rgb); break;
    case RGB_Blue: rst_red(rgb); rst_green(rgb); set_blue(rgb); break;
    case RGB_Yellow: set_red(rgb); set_green(rgb); rst_blue(rgb); break;
    case RGB_Cyan: rst_red(rgb); set_green(rgb); set_blue(rgb); break;
    case RGB_Magenta: set_red(rgb); rst_green(rgb); set_blue(rgb); break;
    case RGB_White: set_red(rgb); set_green(rgb); set_blue(rgb); break;
  }
}

void RGB_Init(RGB_t *rgb)
{
  if(rgb->red) {
    rgb->red->mode = GPIO_Mode_Output;
    GPIO_Init(rgb->red);
  }
  if(rgb->green) {
    rgb->green->mode = GPIO_Mode_Output;
    GPIO_Init(rgb->green);
  }
  if(rgb->blue) {
    rgb->blue->mode = GPIO_Mode_Output;
    GPIO_Init(rgb->blue);
  }
  // Unconditional: reading `_color` first would trust whatever the object was built on
  rgb->_color = rgb->state ? rgb->state : RGB_White;
  RGB_Set(rgb, rgb->state);
  #if(RGB_BASH)
  rgb_focus = rgb;
  #endif
}

void RGB_Set(RGB_t *rgb, RGB_Color_t color)
{
  apply(rgb, color);
  rgb->state = color;
  // Remembered across an off state, so a toggle knows what to light again
  if(color) rgb->_color = color;
}

void RGB_Rst(RGB_t *rgb)
{
  RGB_Set(rgb, RGB_Off);
}

void RGB_Tgl(RGB_t *rgb)
{
  if(rgb->state) RGB_Rst(rgb);
  else if(rgb->_color) RGB_Set(rgb, rgb->_color);
}

void RGB_Loop(RGB_t *rgb)
{
  if(!rgb->blink_ms || !rgb->state) return;
  if(tick_over(&rgb->_tick)) {
    rgb->_blink_on = !rgb->_blink_on;
    if(rgb->_blink_on) apply(rgb, rgb->state);
    else {
      apply(rgb, RGB_Off);
      if(rgb->_one_shot) { // one flash done, the blink disarms itself
        rgb->blink_ms = 0;
        rgb->_one_shot = false;
      }
    }
  }
  if(!rgb->_tick) rgb->_tick = tick_keep(rgb->blink_ms);
}

void RGB_Blink(RGB_t *rgb, uint16_t ms)
{
  rgb->blink_ms = ms;
}

void RGB_BlinkOff(RGB_t *rgb)
{
  rgb->blink_ms = 0;
  RGB_Set(rgb, rgb->state);
}

void RGB_Shot(RGB_t *rgb, RGB_Color_t color, uint16_t ms)
{
  RGB_Set(rgb, color);
  RGB_Blink(rgb, ms);
  rgb->_one_shot = true;
}

//------------------------------------------------------------------------------------------- Shell
#if(RGB_BASH)

static const char *color_name[] = {
  "off", "red", "green", "blue", "yellow", "cyan", "magenta", "white"
};

static RGB_Color_t color_from_hash(RGB_Hash_t hash)
{
  switch(hash) {
    case RGB_Hash_Red: return RGB_Red;
    case RGB_Hash_Green: return RGB_Green;
    case RGB_Hash_Blue: return RGB_Blue;
    case RGB_Hash_Yellow: return RGB_Yellow;
    case RGB_Hash_Cyan: return RGB_Cyan;
    case RGB_Hash_Magenta: return RGB_Magenta;
    case RGB_Hash_White: return RGB_White;
    default: return RGB_Off;
  }
}

// Parse `argv[idx]` as a millisecond count; `false` after logging the parse error
static bool bash_ms(char **argv, uint16_t idx, uint16_t *ms)
{
  if(!str_is_u16(argv[idx])) {
    LOG_ErrorParse(argv[idx], "uint16_t");
    return false;
  }
  *ms = str_to_int(argv[idx]);
  return true;
}

void RGB_Focus(RGB_t *rgb)
{
  rgb_focus = rgb;
}

void RGB_Bash(char **argv, uint16_t argc)
{
  RGB_Hash_t sw = hash_djb2(argv[0]);
  if(sw != RGB_Hash_Rgb && sw != RGB_Hash_Led) return;
  if(!rgb_focus) return;
  if(argc > 1) {
    switch(hash_djb2(argv[1])) {
      case RGB_Hash_Blink:
        if(argc < 3) return;
        sw = hash_djb2(argv[2]);
        if(sw == RGB_Hash_On && argc == 4) {
          uint16_t ms;
          if(!bash_ms(argv, 3, &ms)) CMD_ArgvExit(3);
          RGB_Blink(rgb_focus, ms);
        }
        else if(sw == RGB_Hash_Off && argc == 3) RGB_BlinkOff(rgb_focus);
        break;
      case RGB_Hash_Shot: {
        if(argc < 3) return;
        RGB_Color_t color = color_from_hash(hash_djb2(argv[2]));
        if(!color) return;
        uint16_t ms = 200;
        if(argc >= 4 && !bash_ms(argv, 3, &ms)) CMD_ArgvExit(3);
        RGB_Shot(rgb_focus, color, ms);
        break;
      }
      case RGB_Hash_Off: RGB_Rst(rgb_focus); break;
      default: {
        RGB_Color_t color = color_from_hash(hash_djb2(argv[1]));
        if(color) RGB_Set(rgb_focus, color);
        break;
      }
    }
  }
  DBG_String("RGB "); DBG_String((char *)color_name[rgb_focus->state]);
  if(rgb_focus->state && rgb_focus->blink_ms) {
    DBG_String(rgb_focus->_one_shot ? " shot:" : " blink:");
    DBG_uDec(rgb_focus->blink_ms); DBG_String("ms");
  }
  DBG_Enter();
}

#endif
//-------------------------------------------------------------------------------------------------
