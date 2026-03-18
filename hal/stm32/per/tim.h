// hal/stm32/per/tim.h

#ifndef TIM_H_
#define TIM_H_

#include "gpio.h"

//------------------------------------------------------------------------------------------------- Family-specific

#if defined(STM32G0)
  #include "tim_g0.h"
#elif defined(STM32WB)
  #include "tim_wb.h"
#elif defined(STM32G4)
  #include "tim_g4.h"
#endif

//------------------------------------------------------------------------------------------------- Types

typedef enum {
  TIM_CH1 = 0,
  TIM_CH2 = 1,
  TIM_CH3 = 2,
  TIM_CH4 = 3,
  TIM_CH1N = 4,
  TIM_CH2N = 5,
  TIM_CH3N = 6,
  TIM_CH4N = 7
} TIM_Channel_t;

typedef enum {
  TIM_Filter_NoFilter = 0x0,
  TIM_Filter_FCLK_N2 = 0x1,
  TIM_Filter_FCLK_N4 = 0x2,
  TIM_Filter_FCLK_N8 = 0x3,
  TIM_Filter_FDTS_2xN6 = 0x4,
  TIM_Filter_FDTS_2xN8 = 0x5,
  TIM_Filter_FDTS_4xN6 = 0x6,
  TIM_Filter_FDTS_4xN8 = 0x7,
  TIM_Filter_FDTS_8xN6 = 0x8,
  TIM_Filter_FDTS_8xN8 = 0x9,
  TIM_Filter_FDTS_16xN5 = 0xA,
  TIM_Filter_FDTS_16xN6 = 0xB,
  TIM_Filter_FDTS_16xN8 = 0xC,
  TIM_Filter_FDTS_32xN5 = 0xD,
  TIM_Filter_FDTS_32xN6 = 0xE,
  TIM_Filter_FDTS_32xN8 = 0xF
} TIM_Filter_t;

typedef enum {
  TIM_MasterMode_Reset = 0,
  TIM_MasterMode_Enable = 1,
  TIM_MasterMode_Update = 2,
  TIM_MasterMode_ComparePulse = 3,
  TIM_MasterMode_OC1 = 4,
  TIM_MasterMode_OC2 = 5,
  TIM_MasterMode_OC3 = 6,
  TIM_MasterMode_OC4 = 7,
} TIM_MasterMode_t;

typedef enum {
  TIM_BaseTime_1us = 1000000,
  TIM_BaseTime_10us = 100000,
  TIM_BaseTime_100us = 10000,
  TIM_BaseTime_1ms = 1000,
  TIM_BaseTime_10ms = 100
} TIM_BaseTime_t;

/**
 * @brief Timer configuration structure.
 * @param[in] reg Timer peripheral (TIM1, TIM2, etc.)
 * @param[in] irq_priority Interrupt priority
 * @param[in] one_pulse_mode Stop after single update event
 * @param[in] prescaler Clock prescaler (1 = no division)
 * @param[in] auto_reload Period value (0 = max for timer width)
 * @param[in] Callback Update event callback
 * @param[in] callback_arg Callback argument
 * @param[in] dma_trig Enable DMA trigger on update
 * @param[in] enable Start timer on init
 * @param[in] enable_interrupt Enable update interrupt on init
 */
typedef struct {
  TIM_TypeDef *reg;
  IRQ_Priority_t irq_priority;
  bool one_pulse_mode;
  uint32_t prescaler;
  uint32_t auto_reload;
  void (*Callback)(void *);
  void *callback_arg;
  bool dma_trig;
  bool enable;
  bool enable_interrupt;
  // internal
  volatile uint16_t _event_cnt;
  uint32_t _base_time;
} TIM_t;

//------------------------------------------------------------------------------------------------- Helpers

/**
 * @brief Check if timer is 32-bit.
 * @param[in] reg Timer peripheral
 * @return `true` if 32-bit (TIM2, TIM5)
 */
static inline bool TIM_Is32bit(TIM_TypeDef *reg)
{
  if(reg == TIM2) return true;
  #ifdef TIM5
    if(reg == TIM5) return true;
  #endif
  return false;
}

//------------------------------------------------------------------------------------------------- API

/**
 * @brief Initialize timer.
 * @param[in,out] tim Timer instance
 */
void TIM_Init(TIM_t *tim);

/**
 * @brief Start timer.
 * @param[in,out] tim Timer instance
 */
void TIM_Enable(TIM_t *tim);

/**
 * @brief Stop timer.
 * @param[in,out] tim Timer instance
 */
void TIM_Disable(TIM_t *tim);

/**
 * @brief Enable update interrupt.
 * @param[in,out] tim Timer instance
 */
void TIM_InterruptEnable(TIM_t *tim);

/**
 * @brief Disable update interrupt.
 * @param[in,out] tim Timer instance
 */
void TIM_InterruptDisable(TIM_t *tim);

/**
 * @brief Check if timer is running.
 * @param[in] tim Timer instance
 * @return `true` if enabled
 */
bool TIM_IsEnable(TIM_t *tim);

/**
 * @brief Check if timer is stopped.
 * @param[in] tim Timer instance
 * @return `true` if disabled
 */
bool TIM_IsDisable(TIM_t *tim);

/**
 * @brief Reset counter to zero.
 * @param[in,out] tim Timer instance
 */
void TIM_ResetValue(TIM_t *tim);

/**
 * @brief Get current counter value.
 * @param[in] tim Timer instance
 * @return Counter value
 */
uint32_t TIM_GetValue(TIM_t *tim);

/**
 * @brief Set prescaler value.
 * @param[in,out] tim Timer instance
 * @param[in] prescaler Prescaler (1 = no division)
 */
void TIM_SetPrescaler(TIM_t *tim, uint32_t prescaler);

/**
 * @brief Set auto-reload value.
 * @param[in,out] tim Timer instance
 * @param[in] auto_reload Period value
 */
void TIM_SetAutoreload(TIM_t *tim, uint32_t auto_reload);

/**
 * @brief Set auto-reload to maximum for timer width.
 * @param[in,out] tim Timer instance
 */
void TIM_MaxAutoreload(TIM_t *tim);

/**
 * @brief Check and clear update event.
 * @param[in,out] tim Timer instance
 * @return Event count since last check, `0` if none
 */
uint16_t TIM_Event(TIM_t *tim);

/**
 * @brief Configure timer as master (TRGO output).
 * @param[in,out] tim Timer instance
 * @param[in] mode Master mode selection
 */
void TIM_MasterMode(TIM_t *tim, TIM_MasterMode_t mode);

//------------------------------------------------------------------------------------------------- Delay

/**
 * @brief Initialize timer for blocking delays.
 * @param[in,out] tim Timer instance
 * @param[in] base_time Time base resolution
 */
void DELAY_Init(TIM_t *tim, TIM_BaseTime_t base_time);

/**
 * @brief Blocking delay.
 * @param[in,out] tim Timer instance
 * @param[in] value Delay in `base_time` units
 */
void DELAY_Wait(TIM_t *tim, uint32_t value);

//------------------------------------------------------------------------------------------------- Channel Map

extern const GPIO_Map_t TIM_CHx_MAP[];

//-------------------------------------------------------------------------------------------------

#endif