#include "gpio.h"

#if(GPIO_INCLUDE_WAKEUP)
void GPIO_BackendWakeup(GPIO_t *gpio)
{
  if(!gpio->wakeup_pull) return;
  RCC->APBENR1 |= RCC_APBENR1_PWREN;
  PWR->CR3 |= PWR_CR3_APC;
  if(gpio->wakeup_pull == GPIO_WakeupPull_Up) {
    switch((uint32_t)gpio->port) {
      case (uint32_t)GPIOA: PWR->PUCRA |= (1U << gpio->pin); break;
      case (uint32_t)GPIOB: PWR->PUCRB |= (1U << gpio->pin); break;
      case (uint32_t)GPIOC: PWR->PUCRC |= (1U << gpio->pin); break;
      case (uint32_t)GPIOD: PWR->PUCRD |= (1U << gpio->pin); break;
      case (uint32_t)GPIOF: PWR->PUCRF |= (1U << gpio->pin); break;
      default: break;
    }
  }
  else {
    switch((uint32_t)gpio->port) {
      case (uint32_t)GPIOA: PWR->PDCRA |= (1U << gpio->pin); break;
      case (uint32_t)GPIOB: PWR->PDCRB |= (1U << gpio->pin); break;
      case (uint32_t)GPIOC: PWR->PDCRC |= (1U << gpio->pin); break;
      case (uint32_t)GPIOD: PWR->PDCRD |= (1U << gpio->pin); break;
      case (uint32_t)GPIOF: PWR->PDCRF |= (1U << gpio->pin); break;
      default: break;
    }
  }
}
#endif

//------------------------------------------------------------------------------------------------- EXTI

static void EXTI_Interrupt(EXTI_t *exti)
{
  if(EXTI->FPR1 & (1U << exti->pin)) {
    EXTI->FPR1 = (1U << exti->pin); // Write 1 to clear (don't OR)
    exti->fall_events++;
    exti->int_cnt++;
    if(exti->FallHandler) exti->FallHandler(exti->fall_arg);
    if(exti->oneshot) EXTI_Off(exti);
  }
  if(EXTI->RPR1 & (1U << exti->pin)) {
    EXTI->RPR1 = (1U << exti->pin); // Write 1 to clear (don't OR)
    exti->rise_events++;
    exti->int_cnt++;
    if(exti->RiseHandler) exti->RiseHandler(exti->rise_arg);
    if(exti->oneshot) EXTI_Off(exti);
  }
}

void EXTI_On(EXTI_t *exti)
{
  exti->irq_enable = true;
  exti->fall_events = 0;
  exti->rise_events = 0;
  exti->int_cnt = 0;
  EXTI->IMR1 |= (1U << exti->pin);
}

void EXTI_Off(EXTI_t *exti)
{
  exti->irq_enable = false;
  EXTI->IMR1 &= ~(1U << exti->pin);
}

void EXTI_Init(EXTI_t *exti)
{
  RCC_EnableGPIO(exti->port);
  exti->port->MODER = (exti->port->MODER & ~(3U << (2U * exti->pin))) | (exti->mode << (2U * exti->pin));
  exti->port->PUPDR = (exti->port->PUPDR & ~(3U << (2U * exti->pin))) | (exti->pull << (2U * exti->pin));
  uint32_t exticr_reg = exti->pin / 4;
  uint32_t exticr_move = 8 * (exti->pin % 4);
  uint32_t exticr = EXTI->EXTICR[exticr_reg];
  exticr &= ~(0xFU << exticr_move);
  switch((uint32_t)exti->port) {
    case (uint32_t)GPIOA: break;
    case (uint32_t)GPIOB: exticr |= (1U << exticr_move); break;
    case (uint32_t)GPIOC: exticr |= (2U << exticr_move); break;
    case (uint32_t)GPIOD: exticr |= (3U << exticr_move); break;
    case (uint32_t)GPIOF: exticr |= (5U << exticr_move); break;
    default: break;
  }
  EXTI->EXTICR[exticr_reg] = exticr;
  if(exti->fall_detect) EXTI->FTSR1 |= (1U << exti->pin);
  else EXTI->FTSR1 &= ~(1U << exti->pin);
  if(exti->rise_detect) EXTI->RTSR1 |= (1U << exti->pin);
  else EXTI->RTSR1 &= ~(1U << exti->pin);
  exti->state = ((exti->port->IDR >> exti->pin) & 0x01U) != 0;
  if(exti->irq_enable) EXTI_On(exti);
  else EXTI_Off(exti);
  IRQ_EnableEXTI(exti->pin, exti->irq_priority, (void (*)(void *))&EXTI_Interrupt, exti);
}

uint16_t EXTI_Events(EXTI_t *exti)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint16_t response = exti->rise_events + exti->fall_events;
  exti->rise_events = 0;
  exti->fall_events = 0;
  exti->int_cnt = 0;
  __set_PRIMASK(primask);
  return response;
}

uint16_t EXTI_Rise(EXTI_t *exti)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint16_t response = exti->rise_events;
  exti->rise_events = 0;
  __set_PRIMASK(primask);
  return response;
}

uint16_t EXTI_Fall(EXTI_t *exti)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint16_t response = exti->fall_events;
  exti->fall_events = 0;
  __set_PRIMASK(primask);
  return response;
}

bool EXTI_In(EXTI_t *exti)
{
  return (exti->port->IDR >> exti->pin) & 0x0001;
}
