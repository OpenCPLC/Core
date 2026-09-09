// hal/stm32/per/gpio.h

#ifndef GPIO_H_
#define GPIO_H_

#include <stdbool.h>
#include <stdint.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif
#include "irq.h"
#include "pwr.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef GPIO_INCLUDE_WAKEUP
  // Standby pull configuration in `GPIO_t`, applied by `GPIO_Init`
  #define GPIO_INCLUDE_WAKEUP 0
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  GPIO_Mode_Input = 0,
  GPIO_Mode_Output = 1,
  GPIO_Mode_Alternate = 2,
  GPIO_Mode_Analog = 3
} GPIO_Mode_t;

typedef enum {
  GPIO_Pull_None = 0,
  GPIO_Pull_Up = 1,
  GPIO_Pull_Down = 2
} GPIO_Pull_t;

typedef enum {
  GPIO_OutType_PushPull = 0,
  GPIO_OutType_OpenDrain = 1
} GPIO_OutType_t;

typedef enum {
  GPIO_Speed_VeryLow = 0,
  GPIO_Speed_Low = 1,
  GPIO_Speed_High = 2,
  GPIO_Speed_VeryHigh = 3
} GPIO_Speed_t;

#if(GPIO_INCLUDE_WAKEUP)
typedef enum {
  GPIO_WakeupPull_None = 0,
  GPIO_WakeupPull_Up = 1,
  GPIO_WakeupPull_Down = 2
} GPIO_WakeupPull_t;
#endif

//----------------------------------------------------------------------------------------- Presets

// Initializers: an input at rest, and an alternate function pin at full speed
#define GPIO_DEFAULT { .mode = GPIO_Mode_Input, .speed = GPIO_Speed_VeryLow }
#define GPIO_ALTERNATE { .mode = GPIO_Mode_Alternate, .speed = GPIO_Speed_VeryHigh }

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief One pin, its configuration and its level.
 * @param[in] port Port registers, `GPIOA`, `GPIOB`, ...
 * @param[in] pin Pin number, `0..15`
 * @param[in] reverse Invert the logic level
 * @param[in] mode Pin mode
 * @param[in] pull Pull configuration
 * @param[in] out_type Push-pull or open-drain
 * @param[in] speed Output speed
 * @param[in] wakeup_pull Standby pull, with `GPIO_INCLUDE_WAKEUP`
 * @param[in] alternate Alternate function number, `0..15`
 * @param[in,out] set Level, applied by `GPIO_Init`, tracked by the setters
 */
typedef struct {
  GPIO_TypeDef *port;
  uint8_t pin;
  bool reverse;
  GPIO_Mode_t mode;
  GPIO_Pull_t pull;
  GPIO_OutType_t out_type;
  GPIO_Speed_t speed;
  #if(GPIO_INCLUDE_WAKEUP)
  GPIO_WakeupPull_t wakeup_pull;
  #endif
  uint8_t alternate;
  bool set;
} GPIO_t;

/**
 * @brief Alternate function of a pin, the entry of a pin map.
 * @param[in] port Port registers
 * @param[in] pin Pin number
 * @param[in] alternate Alternate function number
 */
#pragma pack(1)
typedef struct {
  GPIO_TypeDef *port;
  uint8_t pin;
  uint8_t alternate;
} GPIO_Map_t;
#pragma pack()

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Configure a pin and apply its level.
 * @param[in,out] gpio Pointer to GPIO structure
 */
void GPIO_Init(GPIO_t *gpio);

/**
 * @brief Configure pins from a `NULL`-terminated list.
 * @param[in,out] gpio First pin, the rest follow as arguments, `NULL` last
 */
void GPIO_InitList(GPIO_t *gpio, ...);

/**
 * @brief Configure a pin in alternate mode from a pin map entry.
 * @param[in] map Map entry
 * @param[in] open_drain `true` = open-drain with pull-up (I2C), `false` = push-pull, full speed
 */
void GPIO_InitAlternate(const GPIO_Map_t *map, bool open_drain);

/**
 * @brief Configure a pin as a full-speed output, for a supply switch.
 * @param[in,out] gpio Pointer to GPIO structure
 */
void GPIO_SupplyInit(GPIO_t *gpio);

/**
 * @brief Change the mode of a configured pin.
 * @param[in,out] gpio Pointer to GPIO structure
 * @param[in] mode New mode
 */
void GPIO_Mode(GPIO_t *gpio, GPIO_Mode_t mode);
void GPIO_ModeInput(GPIO_t *gpio);
void GPIO_ModeOutput(GPIO_t *gpio);

// Drive the pin high, low or the other way; `reverse` flips the electrical level
void GPIO_Set(GPIO_t *gpio);
void GPIO_Rst(GPIO_t *gpio);
void GPIO_Tgl(GPIO_t *gpio);

// Input level, `reverse` flips it; `NotIn` is the negation
bool GPIO_In(GPIO_t *gpio);
bool GPIO_NotIn(GPIO_t *gpio);

//-------------------------------------------------------------------------------------------- EXTI

typedef void (*EXTI_Handler_t)(void *arg);

/**
 * @brief External interrupt on a pin.
 * @param[in] port Port registers
 * @param[in] pin Pin number, `0..15`
 * @param[in] mode Pin mode, `GPIO_Mode_Input` as a rule
 * @param[in] pull Pull configuration
 * @param[in] rise_detect Interrupt on the rising edge
 * @param[in] fall_detect Interrupt on the falling edge
 * @param[in] irq_enable Enabled by `EXTI_Init`
 * @param[in] irq_priority Interrupt priority
 * @param[in] oneshot Disable after the first edge
 * @param[in] RiseHandler Called on a rising edge, `NULL` = none
 * @param[in] rise_arg Argument of `RiseHandler`
 * @param[in] FallHandler Called on a falling edge, `NULL` = none
 * @param[in] fall_arg Argument of `FallHandler`
 * Internal:
 * @param _rise_cnt Rising edges since the last read
 * @param _fall_cnt Falling edges since the last read
 * @param _state Level at the last edge
 */
typedef struct {
  GPIO_TypeDef *port;
  uint8_t pin;
  GPIO_Mode_t mode;
  GPIO_Pull_t pull;
  bool rise_detect;
  bool fall_detect;
  bool irq_enable;
  IRQ_Priority_t irq_priority;
  bool oneshot;
  EXTI_Handler_t RiseHandler;
  void *rise_arg;
  EXTI_Handler_t FallHandler;
  void *fall_arg;
  // internal
  volatile uint16_t _rise_cnt;
  volatile uint16_t _fall_cnt;
  bool _state;
} EXTI_t;

/**
 * @brief Configure the pin, route it to its EXTI line and arm the interrupt.
 * @param[in,out] exti Pointer to EXTI structure
 */
void EXTI_Init(EXTI_t *exti);

// Unmask or mask the line, an edge latched while masked is dropped on unmask
void EXTI_On(EXTI_t *exti);
void EXTI_Off(EXTI_t *exti);

// Edges since the last read, counters cleared: both, rising, falling
uint16_t EXTI_Events(EXTI_t *exti);
uint16_t EXTI_Rise(EXTI_t *exti);
uint16_t EXTI_Fall(EXTI_t *exti);

// Pin level
bool EXTI_In(EXTI_t *exti);

//---------------------------------------------------------------------------------------- Internal

#if(GPIO_INCLUDE_WAKEUP)
// Family glue: standby pull of the pin from `wakeup_pull`
void GPIO_BackendWakeup(GPIO_t *gpio);
#endif

//-------------------------------------------------------------------------------------------------
#endif
