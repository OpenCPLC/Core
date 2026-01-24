#include "rgb.h"
#include "bash.h"
#include "dbg.h"

const char *rgb_color_name[] = { "off", "red", "green", "blue", "yellow", "cyan", "magenta", "white" };
RGB_t *rgb_focus;

//-------------------------------------------------------------------------------------------------

static void RGB_SetRed(RGB_t *rgb) { if(rgb->red) GPIO_Set(rgb->red); }
static void RGB_RstRed(RGB_t *rgb) { if(rgb->red) GPIO_Rst(rgb->red); }
static void RGB_SetGreen(RGB_t *rgb) { if(rgb->green) GPIO_Set(rgb->green); }
static void RGB_RstGreen(RGB_t *rgb) { if(rgb->green) GPIO_Rst(rgb->green); }
static void RGB_SetBlue(RGB_t *rgb) { if(rgb->blue) GPIO_Set(rgb->blue); }
static void RGB_RstBlue(RGB_t *rgb) { if(rgb->blue) GPIO_Rst(rgb->blue); }

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
  RGB_Set(rgb, rgb->state);
  rgb_focus = rgb;
}

static void RGB_Preset(RGB_t *rgb, RGB_e color)
{
  switch(color) {
    case RGB_Off: RGB_RstRed(rgb); RGB_RstGreen(rgb); RGB_RstBlue(rgb); break;
    case RGB_Red: RGB_SetRed(rgb); RGB_RstGreen(rgb); RGB_RstBlue(rgb); break;
    case RGB_Green: RGB_RstRed(rgb); RGB_SetGreen(rgb); RGB_RstBlue(rgb); break;
    case RGB_Blue: RGB_RstRed(rgb); RGB_RstGreen(rgb); RGB_SetBlue(rgb); break;
    case RGB_Yellow: RGB_SetRed(rgb); RGB_SetGreen(rgb); RGB_RstBlue(rgb); break;
    case RGB_Cyan: RGB_RstRed(rgb); RGB_SetGreen(rgb); RGB_SetBlue(rgb); break;
    case RGB_Magenta: RGB_SetRed(rgb); RGB_RstGreen(rgb); RGB_SetBlue(rgb); break;
    case RGB_White: RGB_SetRed(rgb); RGB_SetGreen(rgb); RGB_SetBlue(rgb); break;
  }
}

void RGB_Set(RGB_t *rgb, RGB_e color)
{
  RGB_Preset(rgb, color);
  rgb->state = color;
}

void RGB_Rst(RGB_t *rgb)
{
  RGB_Set(rgb, RGB_Off);
}

void RGB_Tgl(RGB_t *rgb)
{
  if(rgb->state) RGB_Rst(rgb);
  else RGB_Set(rgb, rgb->state);
}

void RGB_Loop(RGB_t *rgb)
{
  if(rgb->blink_ms && rgb->state) {
    if(tick_over(&rgb->tick)) {
      rgb->blink_on = !rgb->blink_on;
      if(rgb->blink_on) RGB_Preset(rgb, rgb->state);
      else {
        RGB_Preset(rgb, RGB_Off);
        if(rgb->one_shot) {
          rgb_focus->blink_ms = 0;
          rgb->one_shot = false;
        }
      }
    };
    if(!rgb->tick) {
      rgb->tick = tick_keep(rgb->blink_ms);
    }
  }
}

//-------------------------------------------------------------------------------------------------

void LED_Focus(RGB_t *rgb)
{
  rgb_focus = rgb;
}

void LED_Set(RGB_e color)
{
  if(!rgb_focus) return;
  RGB_Set(rgb_focus, color);
}

void LED_Rst(void)
{
  LED_Set(RGB_Off);
}

void LED_Blink_ON(uint16_t ms)
{
  if(!rgb_focus) return;
  rgb_focus->blink_ms = ms;
}

void LED_Blink_OFF(void)
{
  if(!rgb_focus) return;
  rgb_focus->blink_ms = 0;
  LED_Set(rgb_focus->state);
}

void LED_OneShoot(RGB_e color, uint16_t ms)
{
  if(!rgb_focus) return;
  LED_Set(color);
  LED_Blink_ON(ms);
  rgb_focus->one_shot = true;
}

// TODO...
void LED_Bash(char **argv, uint16_t argc)
{
  RGB_Hash_e sw = hash_djb2(argv[0]);
  if(sw != RGB_Hash_Rgb && sw != RGB_Hash_Led) return;
  if(!rgb_focus) return;
  if(argc > 1) {
    switch(hash_djb2(argv[1])) {
      case RGB_Hash_Blink:
        if(argc < 3) return;
        sw = hash_djb2(argv[2]);
        if(sw == RGB_Hash_On && argc == 4) {
          if(!str_is_u16(argv[3])) {
            LOG_ErrorParse(argv[3], "uint16_t");
            BASH_ArgvExit(3);
          }
          uint16_t blink_ms = str_to_int(argv[3]);
          LED_Blink_ON(blink_ms);
        }
        else if(sw == RGB_Hash_Off && argc == 3) {
          LED_Blink_OFF();
        }
        break;
      case RGB_Hash_Shot:
        if(argc < 3) return;
        uint16_t shot_ms;
        if(argc >= 4) {
          if(!str_is_u16(argv[3])) {
            LOG_ErrorParse(argv[3], "uint16_t");
            BASH_ArgvExit(3);
          }
          shot_ms = str_to_int(argv[3]);
        }
        else shot_ms = 200;
        RGB_e color;
        switch(hash_djb2(argv[2])) {
          case RGB_Hash_Red: color = RGB_Red; break;
          case RGB_Hash_Green: color = RGB_Green; break;
          case RGB_Hash_Blue: color = RGB_Blue; break;
          case RGB_Hash_Yellow: color = RGB_Yellow; break;
          case RGB_Hash_Cyan: color = RGB_Cyan; break;
          case RGB_Hash_Magenta: color = RGB_Magenta; break;
          case RGB_Hash_White: color = RGB_White; break;
          default: return;
        }
        LED_OneShoot(color, shot_ms);
        break;
      case RGB_Hash_Off: LED_Set(RGB_Off); break;
      case RGB_Hash_Red: LED_Set(RGB_Red); break;
      case RGB_Hash_Green: LED_Set(RGB_Green); break;
      case RGB_Hash_Blue: LED_Set(RGB_Blue); break;
      case RGB_Hash_Yellow: LED_Set(RGB_Yellow); break;
      case RGB_Hash_Cyan: LED_Set(RGB_Cyan); break;
      case RGB_Hash_Magenta: LED_Set(RGB_Magenta); break;
      case RGB_Hash_White: LED_Set(RGB_White); break;
    }
  }
  DBG_String("RGB "); DBG_String((char *)rgb_color_name[rgb_focus->state]);
  if(rgb_focus->state && rgb_focus->blink_ms) {
    if(rgb_focus->one_shot) DBG_String(" shot:");
    else DBG_String(" blink:");
    DBG_uDec(rgb_focus->blink_ms); DBG_String("ms"); 
  }
  DBG_Enter();
}

//-------------------------------------------------------------------------------------------------
