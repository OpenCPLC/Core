#include "gpio.h"

#if(GPIO_INCLUDE_WAKEUP)
extern void GPIO_BackendWakeup(GPIO_t *gpio);
#endif

//------------------------------------------------------------------------------------------------- GPIO

/**
 * @brief Initialize GPIO pin according to `GPIO_t` configuration.
 * @param gpio Pointer to GPIO configuration structure.
 */
void GPIO_Init(GPIO_t *gpio)
{
  RCC_EnableGPIO(gpio->port);
  gpio->port->PUPDR = (gpio->port->PUPDR & ~(3U << (2U * gpio->pin))) | (gpio->pull << (2U * gpio->pin));
  gpio->port->OSPEEDR = (gpio->port->OSPEEDR & ~(3U << (2U * gpio->pin))) | (gpio->speed << (2U * gpio->pin));
  if(gpio->out_type) gpio->port->OTYPER |= (1U << gpio->pin);
  else gpio->port->OTYPER &= ~(1U << gpio->pin);
  if(gpio->mode == GPIO_Mode_Alternate) {
    if(gpio->pin < 8) {
      gpio->port->AFR[0] = (gpio->port->AFR[0] & ~(0x0FU << (4U * gpio->pin))) | (gpio->alternate << (4U * gpio->pin));
    }
    else {
      gpio->port->AFR[1] = (gpio->port->AFR[1] & ~(0x0FU << (4U * (gpio->pin - 8U)))) | (gpio->alternate << (4U * (gpio->pin - 8U)));
    }
  }
  gpio->port->MODER = (gpio->port->MODER & ~(3U << (2U * gpio->pin))) | (gpio->mode << (2U * gpio->pin));
  if(gpio->set) GPIO_Set(gpio);
  else GPIO_Rst(gpio);
  #if(GPIO_INCLUDE_WAKEUP)
  GPIO_BackendWakeup(gpio);
  #endif
}

/**
 * @brief Initialize multiple GPIO pins.
 * @param gpio First `GPIO_t` pointer, followed by more `GPIO_t *` arguments.
 *   List must be terminated with `NULL`.
 * @note Example: GPIO_InitList(&pin_led, &pin_button, NULL);
 */
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

/**
 * @brief Initialize GPIO pin in alternate mode.
 * @param gpio_map Pointer to `GPIO_Map_t` with port, pin and alternate.
 * @param open_drain_pull_up If `true`: configure as open-drain with pull-up (I2C-style).
 *   If `false`: push-pull, very high speed.
 */
void GPIO_InitAlternate(const GPIO_Map_t *gpio_map, bool open_drain_pull_up)
{
  GPIO_t gpio = GPIO_ALTERNATE;
  gpio.port = gpio_map->port;
  gpio.pin = gpio_map->pin;
  gpio.alternate = gpio_map->alternate;
  if(open_drain_pull_up) {
    gpio.speed = GPIO_Speed_VeryLow;
    gpio.out_type = GPIO_OutType_OpenDrain;
    gpio.pull = GPIO_Pull_Up;
  }
  else {
    gpio.speed = GPIO_Speed_VeryHigh;
  }
  GPIO_Init(&gpio);
}

void GPIO_AlternateInit(const GPIO_Map_t *gpio_map, bool open_drain_pull_up)
{
  GPIO_InitAlternate(gpio_map, open_drain_pull_up);
}

/**
 * @brief Initialize GPIO as high-speed output (e.g. power supply control).
 * @param gpio Pointer to GPIO configuration.
 */
void GPIO_SupplyInit(GPIO_t *gpio)
{
  gpio->mode = GPIO_Mode_Output;
  gpio->speed = GPIO_Speed_VeryHigh;
  GPIO_Init(gpio);
}

/**
 * @brief Set GPIO pin mode.
 * @param gpio Pointer to GPIO configuration.
 * @param mode New mode (`GPIO_Mode_t`).
 */
void GPIO_Mode(GPIO_t *gpio, GPIO_Mode_t mode)
{
  gpio->mode = mode;
  gpio->port->MODER = (gpio->port->MODER & ~(3U << (2U * gpio->pin))) | (gpio->mode << (2U * gpio->pin));
}

void GPIO_ModeInput(GPIO_t *gpio)
{
  GPIO_Mode(gpio, GPIO_Mode_Input);
}

void GPIO_ModeOutput(GPIO_t *gpio)
{
  GPIO_Mode(gpio, GPIO_Mode_Output);
}

/**
 * @brief Set GPIO pin high.
 * @param gpio Pointer to `GPIO_t` structure.
 */
void GPIO_Set(GPIO_t *gpio)
{
  gpio->set = true;
  if(gpio->reverse) gpio->port->BRR = (1U << gpio->pin);
  else gpio->port->BSRR = (1U << gpio->pin);
}

/**
 * @brief Reset GPIO pin low.
 * @param gpio Pointer to `GPIO_t` structure.
 */
void GPIO_Rst(GPIO_t *gpio)
{
  gpio->set = false;
  if(gpio->reverse) gpio->port->BSRR = (1U << gpio->pin);
  else gpio->port->BRR = (1U << gpio->pin);
}

/**
 * @brief Toggle GPIO pin.
 * @param gpio Pointer to `GPIO_t` structure.
 */
void GPIO_Tgl(GPIO_t *gpio)
{
  if(gpio->set) GPIO_Rst(gpio);
  else GPIO_Set(gpio);
}

/**
 * @brief Read GPIO input state.
 * @param gpio Pointer to `GPIO_t` structure.
 * @return `true` if pin is high, `false` if low.
 */
bool GPIO_In(GPIO_t *gpio)
{
  bool raw = (gpio->port->IDR & (1U << gpio->pin)) != 0;
  return gpio->reverse ? !raw : raw;
}

bool GPIO_NotIn(GPIO_t *gpio)
{
  return !GPIO_In(gpio);
}
//------------------------------------------------------------------------------------------------- GPIF

/**
 * @brief Initialize GPIF input filter.
 * Sets GPIO as input, applies default values if zero,
 * reads initial state, clears flags and resets timers.
 * @param gpif Pointer to `GPIF_t` instance
 */
void GPIF_Init(GPIF_t *gpif)
{
  gpif->gpio.mode = GPIO_Mode_Input;
  if(!gpif->ton_ms) gpif->ton_ms = GPIF_DEFAULT_TON_ms;
  if(!gpif->toff_ms) gpif->toff_ms = GPIF_DEFAULT_TOFF_ms;
  if(!gpif->ton_long_ms) gpif->ton_long_ms = GPIF_DEFAULT_TON_LONG_ms;
  if(!gpif->toff_long_ms) gpif->toff_long_ms = GPIF_DEFAULT_TOFF_LONG_ms;
  if(!gpif->toggle_ms) gpif->toggle_ms = GPIF_DEFAULT_TOGGLE_ms;
  GPIO_Init(&gpif->gpio);
  gpif->input_state = GPIO_In(&gpif->gpio);
  gpif->toggle_state = false;
  gpif->toggle_change = false;
  gpif->rise = false;
  gpif->fall = false;
  gpif->rise_long = false;
  gpif->fall_long = false;
  gpif->tick_debounce = gpif->input_state ? tick_keep(gpif->toff_ms) : tick_keep(gpif->ton_ms);
  gpif->tick_long = 0;
  gpif->tick_toggle = 0;
  gpif->tick_reset = 0;
}

/**
 * @brief Update GPIF state, edges, long events and toggle.
 * Call this function periodically. It reads raw GPIO, applies debounce,
 * detects edges, long press/release, and updates toggle output.
 *
 * Toggle logic: Double-click within `toggle_ms` window toggles state.
 * First edge starts window, second edge within window triggers toggle.
 * Reset timer prevents immediate re-trigger.
 */
void GPIF_Loop(GPIF_t *gpif)
{
  bool raw = GPIO_In(&gpif->gpio);
  // Debounce: reset timer if state matches, else check for state change
  if(raw == gpif->input_state) {
    gpif->tick_debounce = raw ? tick_keep(gpif->toff_ms) : tick_keep(gpif->ton_ms);
  }
  else if(tick_over(&gpif->tick_debounce)) {
    gpif->input_state = raw;
    // Toggle logic: second edge within window triggers toggle
    if(!gpif->tick_reset) gpif->tick_toggle = tick_keep(gpif->toggle_ms);
    if(!gpif->toggle_state) gpif->tick_reset = tick_keep(gpif->toggle_ms / 2);
    // Edge detection
    if(raw) {
      gpif->rise = true;
      gpif->tick_long = tick_keep(gpif->ton_long_ms);
    }
    else {
      gpif->fall = true;
      gpif->tick_long = tick_keep(gpif->toff_long_ms);
    }
  }
  // Long press/release detection
  if(tick_over(&gpif->tick_long)) {
    if(gpif->input_state) gpif->rise_long = true;
    else gpif->fall_long = true;
  }
  // Toggle reset window expired
  if(tick_over(&gpif->tick_reset)) gpif->tick_toggle = 0;
  // Toggle trigger
  if(tick_over(&gpif->tick_toggle)) {
    gpif->toggle_state = !gpif->toggle_state;
    gpif->toggle_change = true;
  }
}

/**
 * @brief Get debounced input state.
 * @param gpif Pointer to `GPIF_t` instance
 * @return Current input state
 */
bool GPIF_Input(GPIF_t *gpif)
{
  return gpif->input_state;
}

/**
 * @brief Get toggle output state.
 * @param gpif Pointer to `GPIF_t` instance
 * @return Current toggle state
 */
bool GPIF_Toggle(GPIF_t *gpif)
{
  return gpif->toggle_state;
}

/**
 * @brief Check rising edge and clear flag.
 * @param gpif Pointer to `GPIF_t` instance
 * @return True if rising edge occurred
 */
bool GPIF_Rise(GPIF_t *gpif)
{
  if(gpif->rise) {
    gpif->rise = false;
    return true;
  }
  return false;
}

/**
 * @brief Check falling edge and clear flag.
 * @param gpif Pointer to `GPIF_t` instance
 * @return True if falling edge occurred
 */
bool GPIF_Fall(GPIF_t *gpif)
{
  if(gpif->fall) {
    gpif->fall = false;
    return true;
  }
  return false;
}

/**
 * @brief Check any edge and clear flag.
 * @param gpif Pointer to `GPIF_t` instance
 * @return True if rising or falling edge occurred
 */
bool GPIF_Edge(GPIF_t *gpif)
{
  return GPIF_Rise(gpif) || GPIF_Fall(gpif);
}

/**
 * @brief Check long rising edge and clear flag.
 * @param gpif Pointer to `GPIF_t` instance
 * @return True if long rising edge occurred
 */
bool GPIF_RiseLong(GPIF_t *gpif)
{
  if(gpif->rise_long) {
    gpif->rise_long = false;
    return true;
  }
  return false;
}

/**
 * @brief Check long falling edge and clear flag.
 * @param gpif Pointer to `GPIF_t` instance
 * @return True if long falling edge occurred
 */
bool GPIF_FallLong(GPIF_t *gpif)
{
  if(gpif->fall_long) {
    gpif->fall_long = false;
    return true;
  }
  return false;
}

/**
 * @brief Check any long edge and clear flag.
 * @param gpif Pointer to `GPIF_t` instance
 * @return True if long rising or long falling edge occurred
 */
bool GPIF_EdgeLong(GPIF_t *gpif)
{
  return GPIF_RiseLong(gpif) || GPIF_FallLong(gpif);
}

//-------------------------------------------------------------------------------------------------
