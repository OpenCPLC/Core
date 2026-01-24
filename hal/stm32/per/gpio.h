#ifndef GPIO_H_
#define GPIO_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#if defined(STM32G0)
  #include "stm32g0xx.h"
  #include "pwr.h"
#elif defined(STM32WB)
  #include "stm32wbxx.h"
  // RCC_EnableGPIO is provided by the target's power/clock module
  void RCC_EnableGPIO(GPIO_TypeDef *gpio_typedef);
#endif
#include "irq.h"
#include "vrts.h"
#include "main.h"

#ifndef GPIO_INCLUDE_WAKEUP
  #define GPIO_INCLUDE_WAKEUP 0
#endif

//------------------------------------------------------------------------------------------------- GPIO

typedef enum {
  GPIO_Mode_Input = 0x00,
  GPIO_Mode_Output = 0x01,
  GPIO_Mode_Alternate = 0x02,
  GPIO_Mode_Analog = 0x03
} GPIO_Mode_t;

typedef enum {
  GPIO_Pull_Floating = 0x00,
  GPIO_Pull_Up = 0x01,
  GPIO_Pull_Down = 0x02,
} GPIO_Pull_t;

typedef enum {
  GPIO_OutType_PushPull = 0x00,
  GPIO_OutType_OpenDrain = 0x01
} GPIO_OutType_t;

typedef enum {
  GPIO_Speed_VeryLow = 0x00,
  GPIO_Speed_Low = 0x01,
  GPIO_Speed_High = 0x02,
  GPIO_Speed_VeryHigh = 0x03 // for power-supply
} GPIO_Speed_t;

#if(GPIO_INCLUDE_WAKEUP)
typedef enum {
  GPIO_WakeupPull_Floating = 0x00,
  GPIO_WakeupPull_Up = 0x01,
  GPIO_WakeupPull_Down = 0x02
} GPIO_WakeupPull_t;
#endif

#if(GPIO_INCLUDE_WAKEUP)
  #define GPIO_DEFAULT { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
  #define GPIO_ALTERNATE { NULL, 0, 0, GPIO_Mode_Alternate, 0, 0, GPIO_Speed_VeryHigh, 0, 0, 0 }
#else
  #define GPIO_DEFAULT { NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
  #define GPIO_ALTERNATE { NULL, 0, 0, GPIO_Mode_Alternate, 0, 0, GPIO_Speed_VeryHigh, 0, 0 }
#endif

/**
 * @brief GPIO configuration and state.
 * @param port GPIO port `GPIO_TypeDef` pointer
 * @param pin GPIO pin index
 * @param reverse Invert logic level
 * @param mode GPIO mode
 * @param pull GPIO pull mode
 * @param out_type GPIO output type
 * @param speed GPIO speed
 * @param wakeup_pull Wakeup pull mode (only if `GPIO_INCLUDE_WAKEUP`)
 * @param alternate Alternate function index
 * @param set Current output state
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

#pragma pack(1)
typedef struct {
  GPIO_TypeDef *port;
  uint8_t pin;
  uint8_t alternate;
} GPIO_Map_t;
#pragma pack()

void GPIO_Init(GPIO_t *gpio);
void GPIO_InitList(GPIO_t *gpio, ...);
void GPIO_InitAlternate(const GPIO_Map_t *gpio_map, bool open_drain_pull_up);
void GPIO_AlternateInit(const GPIO_Map_t *gpio_map, bool open_drain_pull_up);
void GPIO_SupplyInit(GPIO_t *gpio);
void GPIO_Set(GPIO_t *gpio);
void GPIO_Rst(GPIO_t *gpio);
void GPIO_Tgl(GPIO_t *gpio);
bool GPIO_In(GPIO_t *gpio);
bool GPIO_NotIn(GPIO_t *gpio);
void GPIO_Mode(GPIO_t *gpio, GPIO_Mode_t mode);
void GPIO_ModeInput(GPIO_t *gpio);
void GPIO_ModeOutput(GPIO_t *gpio);

//------------------------------------------------------------------------------------------------- EXTI

/**
 * @brief External interrupt line configuration.
 * @param port GPIO port `GPIO_TypeDef` pointer
 * @param pin GPIO pin index
 * @param mode GPIO mode
 * @param pull GPIO pull mode
 * @param rise_detect Enable rising edge detection
 * @param fall_detect Enable falling edge detection
 * @param irq_enable Enable interrupt request
 * @param irq_priority Interrupt priority
 * @param oneshot Trigger only once until cleared
 * @param RiseHandler Function called on rising edge
 * @param rise_arg Argument passed to `RiseHandler`
 * @param rise_events Counter of rising edge events
 * @param FallHandler Function called on falling edge
 * @param fall_arg Argument passed to `FallHandler`
 * @param fall_events Counter of falling edge events
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
  IRQ_Priority_t irq_subpriority;
  bool oneshot;
  void (*RiseHandler)(void *);
  void *rise_arg;
  volatile uint16_t rise_events;
  void (*FallHandler)(void *);
  void *fall_arg;
  volatile uint16_t fall_events;
  void (*int_function)(void *);
  void *int_struct;
  volatile uint16_t int_cnt;
  bool state;
} EXTI_t;

void EXTI_Init(EXTI_t *exti);
void EXTI_On(EXTI_t *exti);
void EXTI_Off(EXTI_t *exti);
uint16_t EXTI_Events(EXTI_t *exti);
uint16_t EXTI_Rise(EXTI_t *exti);
uint16_t EXTI_Fall(EXTI_t *exti);
bool EXTI_In(EXTI_t *exti);

//------------------------------------------------------------------------------------------------- GPIF

#define GPIF_DEFAULT_TON_ms 50
#define GPIF_DEFAULT_TOFF_ms 50
#define GPIF_DEFAULT_TON_LONG_ms 2000
#define GPIF_DEFAULT_TOFF_LONG_ms 2000
#define GPIF_DEFAULT_TOGGLE_ms 400

/**
 * @brief Filtered input with debounce, edge detection and toggle.
 * @param gpio Underlying GPIO configuration
 * @param ton_ms Minimum high time [ms] to accept ON
 * @param toff_ms Minimum low time [ms] to accept OFF
 * @param ton_long_ms Long press threshold [ms] when ON
 * @param toff_long_ms Long release threshold [ms] when OFF
 * @param toggle_ms Toggle period [ms]
 * @param rise Flag for rising edge (0→1)
 * @param fall Flag for falling edge (1→0)
 * @param rise_long Flag for long rising edge
 * @param fall_long Flag for long falling edge
 * @param input_state Debounced input state
 * @param toggle_state Toggle output state
 * @param toggle_change Toggle state changed flag
 * @param tick_debounce Internal timer for debounce
 * @param tick_long Internal timer for long threshold
 * @param tick_toggle Internal timer for toggle
 * @param tick_reset Internal timer for toggle reset window
 */
typedef struct {
  GPIO_t gpio;
  uint32_t ton_ms;
  uint32_t toff_ms;
  uint32_t ton_long_ms;
  uint32_t toff_long_ms;
  uint32_t toggle_ms;
  bool rise;
  bool fall;
  bool rise_long;
  bool fall_long;
  bool input_state;
  bool toggle_state;
  bool toggle_change;
  uint64_t tick_debounce;
  uint64_t tick_long;
  uint64_t tick_toggle;
  uint64_t tick_reset;
} GPIF_t;

void GPIF_Init(GPIF_t *gpif);
void GPIF_Loop(GPIF_t *gpif);
bool GPIF_Input(GPIF_t *gpif);
bool GPIF_Toggle(GPIF_t *gpif);
bool GPIF_Rise(GPIF_t *gpif);
bool GPIF_Fall(GPIF_t *gpif);
bool GPIF_Edge(GPIF_t *gpif);
bool GPIF_RiseLong(GPIF_t *gpif);
bool GPIF_FallLong(GPIF_t *gpif);
bool GPIF_EdgeLong(GPIF_t *gpif);

//-------------------------------------------------------------------------------------------------
#endif
