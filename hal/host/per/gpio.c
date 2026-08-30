// hal/host/per/gpio.c

#include "gpio.h"
#include "xdef.h"

//-------------------------------------------------------------------------------------------------

/**
 * A pin is one bit in `ODR` of its port, written by `GPIO_Set` and read by `GPIO_In`.
 * That much is kept because a pin that forgets its level is not a pin. Everything else
 * silicon gives a pad is accepted and dropped: no slew rate, no alternate function, and
 * no vector for an edge to arrive on, so a handler is never called.
 */

GPIO_TypeDef HostGpio[HOST_GPIO_PORTS];

static uint32_t bit(uint8_t pin)
{
  return (pin < 16) ? (1u << pin) : 0u;
}

static void level(GPIO_TypeDef *port, uint8_t pin, bool high)
{
  if(!port) return;
  if(high) port->ODR |= bit(pin);
  else port->ODR &= ~bit(pin);
}

//--------------------------------------------------------------------------------------------- API

void GPIO_Init(GPIO_t *gpio)
{
  if(gpio->mode == GPIO_Mode_Output) {
    level(gpio->port, gpio->pin, gpio->set != gpio->reverse);
  }
  // A pull decides where an unconnected input rests
  else if(gpio->pull != GPIO_Pull_None) {
    level(gpio->port, gpio->pin, gpio->pull == GPIO_Pull_Up);
  }
}

void GPIO_InitList(GPIO_t *gpio, ...) { GPIO_Init(gpio); }
void GPIO_SupplyInit(GPIO_t *gpio) { GPIO_Init(gpio); }

void GPIO_InitAlternate(const GPIO_Map_t *map, bool open_drain)
{
  unused(map);
  unused(open_drain);
}

void GPIO_Mode(GPIO_t *gpio, GPIO_Mode_t mode) { gpio->mode = mode; }
void GPIO_ModeInput(GPIO_t *gpio) { gpio->mode = GPIO_Mode_Input; }
void GPIO_ModeOutput(GPIO_t *gpio) { gpio->mode = GPIO_Mode_Output; }

void GPIO_Set(GPIO_t *gpio) { level(gpio->port, gpio->pin, !gpio->reverse); }
void GPIO_Rst(GPIO_t *gpio) { level(gpio->port, gpio->pin, gpio->reverse); }

void GPIO_Tgl(GPIO_t *gpio)
{
  if(gpio->port) gpio->port->ODR ^= bit(gpio->pin);
}

bool GPIO_In(GPIO_t *gpio)
{
  if(!gpio->port) return false;
  bool high = (gpio->port->ODR & bit(gpio->pin)) != 0;
  return gpio->reverse ? !high : high;
}

bool GPIO_NotIn(GPIO_t *gpio) { return !GPIO_In(gpio); }

void EXTI_Init(EXTI_t *exti)
{
  if(exti->pull != GPIO_Pull_None) level(exti->port, exti->pin, exti->pull == GPIO_Pull_Up);
}

void EXTI_On(EXTI_t *exti) { exti->irq_enable = true; }
void EXTI_Off(EXTI_t *exti) { exti->irq_enable = false; }

uint16_t EXTI_Events(EXTI_t *exti) { unused(exti); return 0; }
uint16_t EXTI_Rise(EXTI_t *exti) { unused(exti); return 0; }
uint16_t EXTI_Fall(EXTI_t *exti) { unused(exti); return 0; }

bool EXTI_In(EXTI_t *exti)
{
  if(!exti->port) return false;
  return (exti->port->ODR & bit(exti->pin)) != 0;
}
