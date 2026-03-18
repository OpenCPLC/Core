// hal/stm32/per/gpio.c

#include "gpio.h"

#if(GPIO_INCLUDE_WAKEUP)
extern void GPIO_BackendWakeup(GPIO_t *gpio);
#endif

//------------------------------------------------------------------------------------------------- GPIO

void GPIO_Init(GPIO_t *gpio)
{
  RCC_EnableGPIO(gpio->port);
  gpio->port->PUPDR = (gpio->port->PUPDR & ~(3u << (2u * gpio->pin))) | (gpio->pull << (2u * gpio->pin));
  gpio->port->OSPEEDR = (gpio->port->OSPEEDR & ~(3u << (2u * gpio->pin))) | (gpio->speed << (2u * gpio->pin));
  if(gpio->out_type) gpio->port->OTYPER |= (1u << gpio->pin);
  else gpio->port->OTYPER &= ~(1u << gpio->pin);
  if(gpio->mode == GPIO_Mode_Alternate) {
    uint32_t afr_idx = gpio->pin >> 3;
    uint32_t afr_pos = (gpio->pin & 7u) << 2;
    gpio->port->AFR[afr_idx] = (gpio->port->AFR[afr_idx] & ~(0x0Fu << afr_pos)) | (gpio->alternate << afr_pos);
  }
  gpio->port->MODER = (gpio->port->MODER & ~(3u << (2u * gpio->pin))) | (gpio->mode << (2u * gpio->pin));
  if(gpio->set) GPIO_Set(gpio);
  else GPIO_Rst(gpio);
  #if(GPIO_INCLUDE_WAKEUP)
    GPIO_BackendWakeup(gpio);
  #endif
}

void GPIO_InitList(GPIO_t *gpio, ...)
{
  va_list args;
  va_start(args, gpio);
  while(gpio) {
    GPIO_Init(gpio);
    gpio = va_arg(args, GPIO_t *);
  }
  va_end(args);
}

void GPIO_InitAlternate(const GPIO_Map_t *map, bool open_drain)
{
  GPIO_t gpio = GPIO_ALTERNATE;
  gpio.port = map->port;
  gpio.pin = map->pin;
  gpio.alternate = map->alternate;
  if(open_drain) {
    gpio.speed = GPIO_Speed_VeryLow;
    gpio.out_type = GPIO_OutType_OpenDrain;
    gpio.pull = GPIO_Pull_Up;
  }
  GPIO_Init(&gpio);
}

void GPIO_SupplyInit(GPIO_t *gpio)
{
  gpio->mode = GPIO_Mode_Output;
  gpio->speed = GPIO_Speed_VeryHigh;
  GPIO_Init(gpio);
}

void GPIO_Mode(GPIO_t *gpio, GPIO_Mode_t mode)
{
  gpio->mode = mode;
  gpio->port->MODER = (gpio->port->MODER & ~(3u << (2u * gpio->pin))) | (mode << (2u * gpio->pin));
}

void GPIO_ModeInput(GPIO_t *gpio) { GPIO_Mode(gpio, GPIO_Mode_Input); }
void GPIO_ModeOutput(GPIO_t *gpio) { GPIO_Mode(gpio, GPIO_Mode_Output); }

void GPIO_Set(GPIO_t *gpio)
{
  gpio->set = true;
  if(gpio->reverse) gpio->port->BRR = (1u << gpio->pin);
  else gpio->port->BSRR = (1u << gpio->pin);
}

void GPIO_Rst(GPIO_t *gpio)
{
  gpio->set = false;
  if(gpio->reverse) gpio->port->BSRR = (1u << gpio->pin);
  else gpio->port->BRR = (1u << gpio->pin);
}

void GPIO_Tgl(GPIO_t *gpio)
{
  if(gpio->set) GPIO_Rst(gpio);
  else GPIO_Set(gpio);
}

bool GPIO_In(GPIO_t *gpio)
{
  bool raw = (gpio->port->IDR & (1u << gpio->pin)) != 0;
  return gpio->reverse ? !raw : raw;
}

bool GPIO_NotIn(GPIO_t *gpio) { return !GPIO_In(gpio); }

//------------------------------------------------------------------------------------------------- GPIF

void GPIF_Init(GPIF_t *gpif)
{
  gpif->gpio.mode = GPIO_Mode_Input;
  if(!gpif->ton_ms) gpif->ton_ms = GPIF_DEFAULT_TON_ms;
  if(!gpif->toff_ms) gpif->toff_ms = GPIF_DEFAULT_TOFF_ms;
  if(!gpif->ton_long_ms) gpif->ton_long_ms = GPIF_DEFAULT_TON_LONG_ms;
  if(!gpif->toff_long_ms) gpif->toff_long_ms = GPIF_DEFAULT_TOFF_LONG_ms;
  if(!gpif->toggle_ms) gpif->toggle_ms = GPIF_DEFAULT_TOGGLE_ms;
  GPIO_Init(&gpif->gpio);
  gpif->_input = GPIO_In(&gpif->gpio);
  gpif->_toggle = false;
  gpif->_toggle_changed = false;
  gpif->_rise = false;
  gpif->_fall = false;
  gpif->_rise_long = false;
  gpif->_fall_long = false;
  gpif->_tick_debounce = gpif->_input ? tick_keep(gpif->toff_ms) : tick_keep(gpif->ton_ms);
  gpif->_tick_long = 0;
  gpif->_tick_toggle = 0;
  gpif->_tick_reset = 0;
}

void GPIF_Loop(GPIF_t *gpif)
{
  bool raw = GPIO_In(&gpif->gpio);
  // Debounce
  if(raw == gpif->_input) {
    gpif->_tick_debounce = raw ? tick_keep(gpif->toff_ms) : tick_keep(gpif->ton_ms);
  }
  else if(tick_over(&gpif->_tick_debounce)) {
    gpif->_input = raw;
    // Toggle logic
    if(!gpif->_tick_reset) gpif->_tick_toggle = tick_keep(gpif->toggle_ms);
    if(!gpif->_toggle) gpif->_tick_reset = tick_keep(gpif->toggle_ms / 2);
    // Edge detection
    if(raw) {
      gpif->_rise = true;
      gpif->_tick_long = tick_keep(gpif->ton_long_ms);
    }
    else {
      gpif->_fall = true;
      gpif->_tick_long = tick_keep(gpif->toff_long_ms);
    }
  }
  // Long press/release
  if(tick_over(&gpif->_tick_long)) {
    if(gpif->_input) gpif->_rise_long = true;
    else gpif->_fall_long = true;
  }
  // Toggle reset window
  if(tick_over(&gpif->_tick_reset)) gpif->_tick_toggle = 0;
  // Toggle trigger
  if(tick_over(&gpif->_tick_toggle)) {
    gpif->_toggle = !gpif->_toggle;
    gpif->_toggle_changed = true;
  }
}

bool GPIF_Input(GPIF_t *gpif) { return gpif->_input; }
bool GPIF_Toggle(GPIF_t *gpif) { return gpif->_toggle; }

bool GPIF_Rise(GPIF_t *gpif)
{
  if(gpif->_rise) { gpif->_rise = false; return true; }
  return false;
}

bool GPIF_Fall(GPIF_t *gpif)
{
  if(gpif->_fall) { gpif->_fall = false; return true; }
  return false;
}

bool GPIF_Edge(GPIF_t *gpif)
{
  return GPIF_Rise(gpif) || GPIF_Fall(gpif);
}

bool GPIF_RiseLong(GPIF_t *gpif)
{
  if(gpif->_rise_long) { gpif->_rise_long = false; return true; }
  return false;
}

bool GPIF_FallLong(GPIF_t *gpif)
{
  if(gpif->_fall_long) { gpif->_fall_long = false; return true; }
  return false;
}

bool GPIF_EdgeLong(GPIF_t *gpif)
{
  return GPIF_RiseLong(gpif) || GPIF_FallLong(gpif);
}

//-------------------------------------------------------------------------------------------------