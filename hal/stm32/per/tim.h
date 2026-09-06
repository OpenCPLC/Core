// hal/stm32/per/tim.h

#ifndef TIM_H_
#define TIM_H_

#include "gpio.h"

//---------------------------------------------------------------------------------- Family include

#if defined(STM32G0)
  #include "tim_g0.h"
#elif defined(STM32WB)
  #include "tim_wb.h"
#endif

//------------------------------------------------------------------------------------------- Types

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

// Input capture filter: sampling clock and the samples that must agree
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

// `TRGO` source, the `MMS` field
typedef enum {
  TIM_MasterMode_Reset = 0,
  TIM_MasterMode_Enable = 1,
  TIM_MasterMode_Update = 2,
  TIM_MasterMode_ComparePulse = 3,
  TIM_MasterMode_OC1 = 4,
  TIM_MasterMode_OC2 = 5,
  TIM_MasterMode_OC3 = 6,
  TIM_MasterMode_OC4 = 7
} TIM_MasterMode_t;

// Delay timer tick rate [Hz]
typedef enum {
  TIM_BaseTime_1us = 1000000,
  TIM_BaseTime_10us = 100000,
  TIM_BaseTime_100us = 10000,
  TIM_BaseTime_1ms = 1000,
  TIM_BaseTime_10ms = 100
} TIM_BaseTime_t;

//---------------------------------------------------------------------------------------- Pin maps

extern const GPIO_Map_t TIM_CHx_MAP[];

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief General timer: a counter with an update callback.
 * @param[in] reg Timer peripheral, `TIM1`, `TIM2`, ...
 * @param[in] irq_priority Interrupt priority
 * @param[in] one_pulse_mode Stop after one update event
 * @param[in] prescaler Clock divider, `1` = undivided
 * @param[in] auto_reload Period, `0` = the widest the counter holds
 * @param[in] Callback Called on every update event, `NULL` = none
 * @param[in] callback_arg Argument of `Callback`
 * @param[in] dma_trig Raise a DMA request on update
 * @param[in] enable Running after `TIM_Init`
 * @param[in] enable_interrupt Update interrupt armed by `TIM_Init`
 * Internal:
 * @param _event_cnt Update events since `TIM_Event`
 * @param _base_time Tick rate of a delay timer [Hz]
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

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Clock the timer and apply the configuration.
 * @param[in,out] tim Timer instance
 */
void TIM_Init(TIM_t *tim);

/**
 * @brief Set the clock divider.
 * @param[in,out] tim Timer instance
 * @param[in] prescaler Divider, `1` = undivided
 */
void TIM_SetPrescaler(TIM_t *tim, uint32_t prescaler);

/**
 * @brief Set the period.
 * @param[in,out] tim Timer instance
 * @param[in] auto_reload Period, `0` is ignored
 */
void TIM_SetAutoreload(TIM_t *tim, uint32_t auto_reload);

// Period to the widest the counter holds
void TIM_MaxAutoreload(TIM_t *tim);

/**
 * @brief Update events since the last call, cleared on read.
 * @param[in,out] tim Timer instance
 * @return Event count, `0` when none
 */
uint16_t TIM_Event(TIM_t *tim);

/**
 * @brief Route an event to `TRGO`.
 * @param[in,out] tim Timer instance
 * @param[in] mode Event routed
 */
void TIM_MasterMode(TIM_t *tim, TIM_MasterMode_t mode);

//------------------------------------------------------------------------------------------- Delay

/**
 * @brief Run a timer as the tick source of `DELAY_Wait`.
 * @param[in,out] tim Timer instance
 * @param[in] base_time Tick rate
 */
void DELAY_Init(TIM_t *tim, TIM_BaseTime_t base_time);

/**
 * @brief Busy-wait on a delay timer.
 * @param[in,out] tim Timer instance
 * @param[in] value Wait [ticks of `base_time`]
 */
void DELAY_Wait(TIM_t *tim, uint32_t value);

//------------------------------------------------------------------------------------------ Inline

// `true` for the 32-bit counters, `TIM2` and `TIM5`
static inline bool TIM_Is32bit(TIM_TypeDef *reg)
{
  if(reg == TIM2) return true;
  #ifdef TIM5
  if(reg == TIM5) return true;
  #endif
  return false;
}

static inline void TIM_Enable(TIM_t *tim)
{
  tim->reg->CR1 |= TIM_CR1_CEN;
  tim->reg->SR &= ~TIM_SR_UIF;
  tim->enable = true;
}

static inline void TIM_Disable(TIM_t *tim)
{
  tim->reg->CR1 &= ~TIM_CR1_CEN;
  tim->_event_cnt = 0;
  tim->enable = false;
}

static inline void TIM_InterruptEnable(TIM_t *tim)
{
  tim->reg->DIER |= TIM_DIER_UIE;
  tim->enable_interrupt = true;
}

static inline void TIM_InterruptDisable(TIM_t *tim)
{
  tim->reg->DIER &= ~TIM_DIER_UIE;
  tim->_event_cnt = 0;
  tim->enable_interrupt = false;
}

static inline bool TIM_IsEnable(TIM_t *tim) { return tim->reg->CR1 & TIM_CR1_CEN_Msk; }
static inline bool TIM_IsDisable(TIM_t *tim) { return !(tim->reg->CR1 & TIM_CR1_CEN_Msk); }

// Counter back to zero, flags cleared
static inline void TIM_ResetValue(TIM_t *tim)
{
  tim->reg->EGR = TIM_EGR_UG;
  tim->reg->SR = 0;
  tim->_event_cnt = 0;
}

static inline uint32_t TIM_GetValue(TIM_t *tim)
{
  return TIM_Is32bit(tim->reg) ? tim->reg->CNT : (tim->reg->CNT & 0xFFFFu);
}

//-------------------------------------------------------------------------------------------------
#endif
