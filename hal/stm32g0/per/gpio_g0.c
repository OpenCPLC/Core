// hal/stm32g0/gpio_g0.c

#include "gpio.h"

//------------------------------------------------------------------------------------------------- Wakeup

#if(GPIO_INCLUDE_WAKEUP)
void GPIO_BackendWakeup(GPIO_t *gpio)
{
  if(!gpio->wakeup_pull) return;
  RCC->APBENR1 |= RCC_APBENR1_PWREN;
  PWR->CR3 |= PWR_CR3_APC;
  uint32_t mask = 1u << gpio->pin;
  if(gpio->wakeup_pull == GPIO_WakeupPull_Up) {
    switch((uint32_t)gpio->port) {
      case (uint32_t)GPIOA: PWR->PUCRA |= mask; break;
      case (uint32_t)GPIOB: PWR->PUCRB |= mask; break;
      case (uint32_t)GPIOC: PWR->PUCRC |= mask; break;
      case (uint32_t)GPIOD: PWR->PUCRD |= mask; break;
      case (uint32_t)GPIOF: PWR->PUCRF |= mask; break;
    }
  }
  else {
    switch((uint32_t)gpio->port) {
      case (uint32_t)GPIOA: PWR->PDCRA |= mask; break;
      case (uint32_t)GPIOB: PWR->PDCRB |= mask; break;
      case (uint32_t)GPIOC: PWR->PDCRC |= mask; break;
      case (uint32_t)GPIOD: PWR->PDCRD |= mask; break;
      case (uint32_t)GPIOF: PWR->PDCRF |= mask; break;
    }
  }
}
#endif

//------------------------------------------------------------------------------------------------- EXTI IRQ

static void EXTI_IRQHandler(EXTI_t *exti)
{
  uint32_t mask = 1u << exti->pin;
  // G0 has separate falling/rising pending registers
  if(EXTI->FPR1 & mask) {
    EXTI->FPR1 = mask; // Write 1 to clear
    exti->_fall_cnt++;
    exti->_state = false;
    if(exti->FallHandler) exti->FallHandler(exti->fall_arg);
    if(exti->oneshot) EXTI_Off(exti);
  }
  if(EXTI->RPR1 & mask) {
    EXTI->RPR1 = mask; // Write 1 to clear
    exti->_rise_cnt++;
    exti->_state = true;
    if(exti->RiseHandler) exti->RiseHandler(exti->rise_arg);
    if(exti->oneshot) EXTI_Off(exti);
  }
}

//------------------------------------------------------------------------------------------------- EXTI API

void EXTI_On(EXTI_t *exti)
{
  exti->irq_enable = true;
  exti->_rise_cnt = 0;
  exti->_fall_cnt = 0;
  EXTI->IMR1 |= (1u << exti->pin);
}

void EXTI_Off(EXTI_t *exti)
{
  exti->irq_enable = false;
  EXTI->IMR1 &= ~(1u << exti->pin);
}

void EXTI_Init(EXTI_t *exti)
{
  RCC->APBENR2 |= RCC_APBENR2_SYSCFGEN;
  RCC_EnableGPIO(exti->port);
  // GPIO mode and pull
  exti->port->MODER = (exti->port->MODER & ~(3u << (2u * exti->pin))) | (exti->mode << (2u * exti->pin));
  exti->port->PUPDR = (exti->port->PUPDR & ~(3u << (2u * exti->pin))) | (exti->pull << (2u * exti->pin));
  // EXTICR - G0 uses 8-bit fields in EXTI->EXTICR
  uint32_t reg = exti->pin / 4;
  uint32_t pos = 8 * (exti->pin % 4);
  uint32_t val = EXTI->EXTICR[reg] & ~(0xFFu << pos);
  switch((uint32_t)exti->port) {
    case (uint32_t)GPIOA: break;
    case (uint32_t)GPIOB: val |= (1u << pos); break;
    case (uint32_t)GPIOC: val |= (2u << pos); break;
    case (uint32_t)GPIOD: val |= (3u << pos); break;
    case (uint32_t)GPIOF: val |= (5u << pos); break;
  }
  EXTI->EXTICR[reg] = val;
  // Edge detection
  if(exti->fall_detect) EXTI->FTSR1 |= (1u << exti->pin);
  else EXTI->FTSR1 &= ~(1u << exti->pin);
  if(exti->rise_detect) EXTI->RTSR1 |= (1u << exti->pin);
  else EXTI->RTSR1 &= ~(1u << exti->pin);
  // Initial state
  exti->_state = (exti->port->IDR >> exti->pin) & 1;
  // Enable/disable
  if(exti->irq_enable) EXTI_On(exti);
  else EXTI_Off(exti);
  // IRQ - G0 has no subpriority
  IRQ_EnableEXTI(exti->pin, exti->irq_priority, (IRQ_Handler_t)EXTI_IRQHandler, exti);
}

uint16_t EXTI_Events(EXTI_t *exti)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint16_t cnt = exti->_rise_cnt + exti->_fall_cnt;
  exti->_rise_cnt = 0;
  exti->_fall_cnt = 0;
  __set_PRIMASK(primask);
  return cnt;
}

uint16_t EXTI_Rise(EXTI_t *exti)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint16_t cnt = exti->_rise_cnt;
  exti->_rise_cnt = 0;
  __set_PRIMASK(primask);
  return cnt;
}

uint16_t EXTI_Fall(EXTI_t *exti)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint16_t cnt = exti->_fall_cnt;
  exti->_fall_cnt = 0;
  __set_PRIMASK(primask);
  return cnt;
}

bool EXTI_In(EXTI_t *exti)
{
  return (exti->port->IDR >> exti->pin) & 1;
}

//-------------------------------------------------------------------------------------------------