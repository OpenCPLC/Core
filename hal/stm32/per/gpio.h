// hal/stm32/per/gpio.h

#ifndef GPIO_H_
#define GPIO_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
#endif
#include "irq.h"
#include "pwr.h"
#include "vrts.h"
#include "main.h"

#ifndef GPIO_INCLUDE_WAKEUP
  #define GPIO_INCLUDE_WAKEUP 0
#endif

//------------------------------------------------------------------------------------------------- GPIO Types

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

//------------------------------------------------------------------------------------------------- GPIO Presets

#if(GPIO_INCLUDE_WAKEUP)
  #define GPIO_DEFAULT { NULL, 0, false, GPIO_Mode_Input, GPIO_Pull_None, GPIO_OutType_PushPull, GPIO_Speed_VeryLow, GPIO_WakeupPull_None, 0, false }
  #define GPIO_ALTERNATE { NULL, 0, false, GPIO_Mode_Alternate, GPIO_Pull_None, GPIO_OutType_PushPull, GPIO_Speed_VeryHigh, GPIO_WakeupPull_None, 0, false }
#else
  #define GPIO_DEFAULT { NULL, 0, false, GPIO_Mode_Input, GPIO_Pull_None, GPIO_OutType_PushPull, GPIO_Speed_VeryLow, 0, false }
  #define GPIO_ALTERNATE { NULL, 0, false, GPIO_Mode_Alternate, GPIO_Pull_None, GPIO_OutType_PushPull, GPIO_Speed_VeryHigh, 0, false }
#endif

//------------------------------------------------------------------------------------------------- GPIO Structure

/**
 * @brief GPIO pin configuration and state.
 * @param[in] port GPIO port pointer (`GPIOA`, `GPIOB`, etc.)
 * @param[in] pin Pin number (0-15)
 * @param[in] reverse Invert logic level
 * @param[in] mode Pin mode (`GPIO_Mode_Input`, `GPIO_Mode_Output`, etc.)
 * @param[in] pull Pull configuration
 * @param[in] out_type Output type (push-pull or open-drain)
 * @param[in] speed Output speed
 * @param[in] wakeup_pull Standby wakeup pull (if `GPIO_INCLUDE_WAKEUP`)
 * @param[in] alternate Alternate function number (0-15)
 * @param[in] set Initial output state
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
 * @brief GPIO alternate function mapping (for pin tables).
 * @param[in] port GPIO port pointer
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

//------------------------------------------------------------------------------------------------- GPIO API

/**
 * @brief Initialize GPIO pin.
 * @param[in,out] gpio Pointer to GPIO structure
 */
void GPIO_Init(GPIO_t *gpio);

/**
 * @brief Initialize multiple GPIO pins (NULL-terminated list).
 * @param[in,out] gpio First GPIO pointer, followed by more, terminated with `NULL`
 */
void GPIO_InitList(GPIO_t *gpio, ...);

/**
 * @brief Initialize GPIO in alternate mode from pin map.
 * @param[in] map Pointer to `GPIO_Map_t`
 * @param[in] open_drain `true` = open-drain with pull-up (I2C), `false` = push-pull high-speed
 */
void GPIO_InitAlternate(const GPIO_Map_t *map, bool open_drain);

/**
 * @brief Initialize GPIO as high-speed output (power supply control).
 * @param[in,out] gpio Pointer to GPIO structure
 */
void GPIO_SupplyInit(GPIO_t *gpio);

/**
 * @brief Set GPIO pin mode.
 * @param[in,out] gpio Pointer to GPIO structure
 * @param[in] mode New mode
 */
void GPIO_Mode(GPIO_t *gpio, GPIO_Mode_t mode);

/**
 * @brief Set GPIO to input mode.
 * @param[in,out] gpio Pointer to GPIO structure
 */
void GPIO_ModeInput(GPIO_t *gpio);

/**
 * @brief Set GPIO to output mode.
 * @param[in,out] gpio Pointer to GPIO structure
 */
void GPIO_ModeOutput(GPIO_t *gpio);

/**
 * @brief Set GPIO pin high (respects `reverse` flag).
 * @param[in,out] gpio Pointer to GPIO structure
 */
void GPIO_Set(GPIO_t *gpio);

/**
 * @brief Set GPIO pin low (respects `reverse` flag).
 * @param[in,out] gpio Pointer to GPIO structure
 */
void GPIO_Rst(GPIO_t *gpio);

/**
 * @brief Toggle GPIO pin.
 * @param[in,out] gpio Pointer to GPIO structure
 */
void GPIO_Tgl(GPIO_t *gpio);

/**
 * @brief Read GPIO input state (respects `reverse` flag).
 * @param[in] gpio Pointer to GPIO structure
 * @return `true` if high, `false` if low
 */
bool GPIO_In(GPIO_t *gpio);

/**
 * @brief Read inverted GPIO input state.
 * @param[in] gpio Pointer to GPIO structure
 * @return `true` if low, `false` if high
 */
bool GPIO_NotIn(GPIO_t *gpio);

//------------------------------------------------------------------------------------------------- EXTI Types

typedef void (*EXTI_Handler_t)(void *arg);

/**
 * @brief External interrupt configuration.
 * @param[in] port GPIO port pointer
 * @param[in] pin Pin number (0-15)
 * @param[in] mode Pin mode (typically `GPIO_Mode_Input`)
 * @param[in] pull Pull configuration
 * @param[in] rise_detect Enable rising edge detection
 * @param[in] fall_detect Enable falling edge detection
 * @param[in] irq_enable Enable interrupt on init
 * @param[in] irq_priority Interrupt priority
 * @param[in] oneshot Disable interrupt after first trigger
 * @param[in] RiseHandler Rising edge callback
 * @param[in] rise_arg Argument for `RiseHandler`
 * @param[in] FallHandler Falling edge callback
 * @param[in] fall_arg Argument for `FallHandler`
 * Internal:
 * @param _rise_cnt Rising edge event counter
 * @param _fall_cnt Falling edge event counter
 * @param _state Current pin state
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

//------------------------------------------------------------------------------------------------- EXTI API

/**
 * @brief Initialize external interrupt.
 * @param[in,out] exti Pointer to EXTI structure
 */
void EXTI_Init(EXTI_t *exti);

/**
 * @brief Enable EXTI interrupt.
 * @param[in,out] exti Pointer to EXTI structure
 */
void EXTI_On(EXTI_t *exti);

/**
 * @brief Disable EXTI interrupt.
 * @param[in,out] exti Pointer to EXTI structure
 */
void EXTI_Off(EXTI_t *exti);

/**
 * @brief Get total edge count and clear counters.
 * @param[in,out] exti Pointer to EXTI structure
 * @return Sum of rise and fall events
 */
uint16_t EXTI_Events(EXTI_t *exti);

/**
 * @brief Get rising edge count and clear counter.
 * @param[in,out] exti Pointer to EXTI structure
 * @return Rising edge count
 */
uint16_t EXTI_Rise(EXTI_t *exti);

/**
 * @brief Get falling edge count and clear counter.
 * @param[in,out] exti Pointer to EXTI structure
 * @return Falling edge count
 */
uint16_t EXTI_Fall(EXTI_t *exti);

/**
 * @brief Read current EXTI pin state.
 * @param[in] exti Pointer to EXTI structure
 * @return `true` if high
 */
bool EXTI_In(EXTI_t *exti);

//------------------------------------------------------------------------------------------------- GPIF Types

#define GPIF_DEFAULT_TON_ms       50
#define GPIF_DEFAULT_TOFF_ms      50
#define GPIF_DEFAULT_TON_LONG_ms  2000
#define GPIF_DEFAULT_TOFF_LONG_ms 2000
#define GPIF_DEFAULT_TOGGLE_ms    400

/**
 * @brief Filtered GPIO input with debounce, edge detection and toggle.
 * @param[in] gpio Underlying GPIO configuration
 * @param[in] ton_ms Debounce time for ON [ms]
 * @param[in] toff_ms Debounce time for OFF [ms]
 * @param[in] ton_long_ms Long press threshold [ms]
 * @param[in] toff_long_ms Long release threshold [ms]
 * @param[in] toggle_ms Double-click window for toggle [ms]
 * Internal:
 * @param _rise Rising edge flag
 * @param _fall Falling edge flag
 * @param _rise_long Long press flag
 * @param _fall_long Long release flag
 * @param _input Debounced input state
 * @param _toggle Toggle output state
 * @param _toggle_changed Toggle state changed flag
 * @param _tick_debounce Debounce timer
 * @param _tick_long Long threshold timer
 * @param _tick_toggle Toggle window timer
 * @param _tick_reset Toggle reset timer
 */
typedef struct {
  GPIO_t gpio;
  uint32_t ton_ms;
  uint32_t toff_ms;
  uint32_t ton_long_ms;
  uint32_t toff_long_ms;
  uint32_t toggle_ms;
  // internal
  bool _rise;
  bool _fall;
  bool _rise_long;
  bool _fall_long;
  bool _input;
  bool _toggle;
  bool _toggle_changed;
  uint64_t _tick_debounce;
  uint64_t _tick_long;
  uint64_t _tick_toggle;
  uint64_t _tick_reset;
} GPIF_t;

//------------------------------------------------------------------------------------------------- GPIF API

/**
 * @brief Initialize GPIF (sets GPIO as input, applies defaults).
 * @param[in,out] gpif Pointer to GPIF structure
 */
void GPIF_Init(GPIF_t *gpif);

/**
 * @brief Update GPIF state (call periodically).
 * @param[in,out] gpif Pointer to GPIF structure
 */
void GPIF_Loop(GPIF_t *gpif);

/**
 * @brief Get debounced input state.
 * @param[in] gpif Pointer to GPIF structure
 * @return Current input state
 */
bool GPIF_Input(GPIF_t *gpif);

/**
 * @brief Get toggle output state.
 * @param[in] gpif Pointer to GPIF structure
 * @return Current toggle state
 */
bool GPIF_Toggle(GPIF_t *gpif);

/**
 * @brief Check and clear rising edge flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if rising edge occurred
 */
bool GPIF_Rise(GPIF_t *gpif);

/**
 * @brief Check and clear falling edge flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if falling edge occurred
 */
bool GPIF_Fall(GPIF_t *gpif);

/**
 * @brief Check and clear any edge flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if any edge occurred
 */
bool GPIF_Edge(GPIF_t *gpif);

/**
 * @brief Check and clear long press flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if long press occurred
 */
bool GPIF_RiseLong(GPIF_t *gpif);

/**
 * @brief Check and clear long release flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if long release occurred
 */
bool GPIF_FallLong(GPIF_t *gpif);

/**
 * @brief Check and clear any long edge flag.
 * @param[in,out] gpif Pointer to GPIF structure
 * @return `true` if any long edge occurred
 */
bool GPIF_EdgeLong(GPIF_t *gpif);

//-------------------------------------------------------------------------------------------------

#endif