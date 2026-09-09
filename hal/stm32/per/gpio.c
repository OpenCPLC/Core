// hal/stm32/per/gpio.c

#include "gpio.h"

#include <stdarg.h>

//-------------------------------------------------------------------------------------------- GPIO

// Two-bit field of `pin` in a port register
static inline void set_field(volatile uint32_t *reg, uint8_t pin, uint32_t value)
{
  *reg = (*reg & ~(3u << (2u * pin))) | (value << (2u * pin));
}

void GPIO_Init(GPIO_t *gpio)
{
  RCC_EnableGPIO(gpio->port);
  set_field(&gpio->port->PUPDR, gpio->pin, gpio->pull);
  set_field(&gpio->port->OSPEEDR, gpio->pin, gpio->speed);
  if(gpio->out_type) gpio->port->OTYPER |= (1u << gpio->pin);
  else gpio->port->OTYPER &= ~(1u << gpio->pin);
  if(gpio->mode == GPIO_Mode_Alternate) {
    uint32_t afr_idx = gpio->pin >> 3;
    uint32_t afr_pos = (gpio->pin & 7u) << 2;
    gpio->port->AFR[afr_idx] =
      (gpio->port->AFR[afr_idx] & ~(0x0Fu << afr_pos)) | (gpio->alternate << afr_pos);
  }
  set_field(&gpio->port->MODER, gpio->pin, gpio->mode);
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
  set_field(&gpio->port->MODER, gpio->pin, mode);
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

//-------------------------------------------------------------------------------------------------
