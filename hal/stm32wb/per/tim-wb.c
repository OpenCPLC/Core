#include "tim.h"

//------------------------------------------------------------------------------------------------- Channel-Map

const GPIO_Map_t TIM_CHx_MAP[] = {
  [TIM1_CH1_PA8] = { .port = GPIOA, .pin = 8, .alternate = 1 },
  [TIM1_CH1N_PB13] = { .port = GPIOB, .pin = 13, .alternate = 1 },
  [TIM1_CH2_PA9] = { .port = GPIOA, .pin = 9, .alternate = 1 },
  [TIM1_CH2N_PB0] = { .port = GPIOB, .pin = 0, .alternate = 1 },
  [TIM1_CH3_PA10] = { .port = GPIOA, .pin = 10, .alternate = 1 },
  [TIM1_CH3N_PB1] = { .port = GPIOB, .pin = 1, .alternate = 1 },
  [TIM1_CH4_PA11] = { .port = GPIOA, .pin = 11, .alternate = 1 },
  [TIM2_CH1_PA0] = { .port = GPIOA, .pin = 0, .alternate = 1 },
  [TIM2_CH1_PA5] = { .port = GPIOA, .pin = 5, .alternate = 1 },
  [TIM2_CH1_PA15] = { .port = GPIOA, .pin = 15, .alternate = 1 },
  [TIM2_CH2_PA1] = { .port = GPIOA, .pin = 1, .alternate = 1 },
  [TIM2_CH2_PB3] = { .port = GPIOB, .pin = 3, .alternate = 1 },
  [TIM2_CH3_PA2] = { .port = GPIOA, .pin = 2, .alternate = 1 },
  [TIM2_CH4_PA3] = { .port = GPIOA, .pin = 3, .alternate = 1 },
  [TIM16_CH1_PA6] = { .port = GPIOA, .pin = 6, .alternate = 1 },
  [TIM16_CH1N_PB6] = { .port = GPIOB, .pin = 6, .alternate = 1 },
  [TIM16_CH1_PB8] = { .port = GPIOB, .pin = 8, .alternate = 1 },
  [TIM17_CH1_PA7] = { .port = GPIOA, .pin = 7, .alternate = 1 },
  [TIM17_CH1_PB9] = { .port = GPIOB, .pin = 9, .alternate = 1 },
  [TIM17_CH1N_PB7] = { .port = GPIOB, .pin = 7, .alternate = 1 }
};
