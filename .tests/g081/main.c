// .tests/g081/main.c

// On a Nucleo with G0 it tests clocks: `make flash` with the console on PC4/PC5,
// then the last console line is the verdict.

#include <vrts.h>
#include <sys.h>
#include <rtc.h>
#include <log.h>
#include <test.h>

#include "main.h"

//--------------------------------------------------------------------------------------------- dbg

UART_t dbg_uart = {
  .reg = USART1,
  .tx = UART1_TX_PC4,
  .rx = UART1_RX_PC5,
  .dma = DMA_CH4,
  .irq_priority = IRQ_Priority_Low,
  .UART_115200
};

//------------------------------------------------------------------------------------------- clock

// Least wait states RM0444 allows: 24MHz a state in range 1, 8MHz in range 2
static uint32_t latency_min(uint32_t freq_Hz, bool range_2)
{
  return (freq_Hz - 1) / (range_2 ? 8000000 : 24000000);
}

// Low-power run caps the clock at 2MHz, so each faster clock after `RCC_2MHz` has to leave it
static void clocks_fit_their_range(void)
{
  uint32_t (*clocks[])(void) = {
    RCC_2MHz, RCC_64MHz, RCC_2MHz, RCC_16MHz, RCC_2MHz, RCC_48MHz, RCC_16MHz
  };
  uint32_t freq_Hz[array_len(clocks)], cr1[array_len(clocks)], latency[array_len(clocks)];
  delay(2); // the last bytes of the console leave after its DMA ends, before the baud rate goes
  for(uint8_t i = 0; i < array_len(clocks); i++) {
    freq_Hz[i] = clocks[i]();
    cr1[i] = PWR->CR1;
    latency[i] = FLASH->ACR & FLASH_ACR_LATENCY;
  }
  // The console speaks at the clock of the project, the last of the list, so checks wait for it
  for(uint8_t i = 0; i < array_len(clocks); i++) {
    bool range_2 = (cr1[i] & PWR_CR1_VOS) == PWR_CR1_VOS_1;
    TEST_Check(!(cr1[i] & PWR_CR1_LPR) || freq_Hz[i] <= 2000000,
      "low-power run at %uHz", freq_Hz[i]);
    TEST_Check(latency[i] >= latency_min(freq_Hz[i], range_2),
      "%u wait states at %uHz", latency[i], freq_Hz[i]);
  }
}

//-------------------------------------------------------------------------------------------- test

void TEST_Cases(void)
{
  TEST(clocks_fit_their_range);
}

GPIO_t led = { // green `LD2` of Nucleo-G071RB
  .port = GPIOA,
  .pin = 5,
  .mode = GPIO_Mode_Output
};

// Cases once, then the LED blinks: the board runs on
static void test(void)
{
  TEST_Suite(TEST_Cases);
  GPIO_Init(&led);
  while(1) {
    GPIO_Tgl(&led);
    delay(500);
  }
}

//-------------------------------------------------------------------------------------------- main

stack(stack_dbg, 256);
stack(stack_test, 512);

int main(void)
{
  sys_init(); // clock, systick and heap
  RTC_Init();
  DBG_Init(&dbg_uart); // logs and shell on this port
  DBG_Enter();
  LOG_Info("Build: %s %s", __DATE__, __TIME__);
  LOG_Info("Target MCU: " ANSI_PINK "STM32G081" ANSI_END);
  thread(DBG_Loop, stack_dbg);
  thread(test, stack_test);
  vrts_init();
  while(1); // `vrts_init` never returns
}
