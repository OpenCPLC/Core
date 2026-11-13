// .tests/wb55/main.c

// On Nucleo-WB55 it tests clocks and RTC: `make flash` with the console on its ST-Link VCP,
// one reset of its own, then the last console line is the verdict.

#include <vrts.h>
#include <sys.h>
#include <rtc.h>
#include <log.h>
#include <test.h>

#include "main.h"

//------------------------------------------------------------------------------------------ Config

#define KNOWN_TIME_s 1767268800 // 2026-01-01 12:00:00, set by the run before the reset

//--------------------------------------------------------------------------------------------- dbg

UART_t dbg_uart = {
  .reg = USART1,
  .tx = UART1_TX_PB6,
  .rx = UART1_RX_PB7,
  .dma = DMA_CH4,
  .irq_priority = IRQ_Priority_Low,
  .UART_115200
};

//------------------------------------------------------------------------------------------- clock

// Least wait states RM0434 allows: 18MHz a state in range 1, 6MHz in range 2
static uint32_t latency_min(uint32_t freq_Hz, bool range_2)
{
  return (freq_Hz - 1) / (range_2 ? 6000000 : 18000000);
}

static void clocks_fit_their_range(void)
{
  uint32_t (*clocks[])(void) = { RCC_2MHz, RCC_64MHz, RCC_16MHz, RCC_48MHz, RCC_2MHz, RCC_16MHz };
  uint32_t freq_Hz[array_len(clocks)], vos[array_len(clocks)], latency[array_len(clocks)];
  delay(2); // the last bytes of the console leave after its DMA ends, before the baud rate goes
  for(uint8_t i = 0; i < array_len(clocks); i++) {
    freq_Hz[i] = clocks[i]();
    vos[i] = PWR->CR1 & PWR_CR1_VOS;
    latency[i] = FLASH->ACR & FLASH_ACR_LATENCY;
  }
  // The console speaks at the clock of the project, the last of the list, so checks wait for it
  for(uint8_t i = 0; i < array_len(clocks); i++) {
    bool range_2 = vos[i] == PWR_CR1_VOS_1;
    TEST_Check(!range_2 || freq_Hz[i] <= 16000000, "range 2 at %uHz", freq_Hz[i]);
    TEST_Check(latency[i] >= latency_min(freq_Hz[i], range_2),
      "%u wait states at %uHz", latency[i], freq_Hz[i]);
  }
}

//--------------------------------------------------------------------------------------------- rtc

static status_t rtc_status; // what `RTC_Init` reported at start

static void rtc_starts(void)
{
  TEST_Equal(rtc_status, OK, "LSE runs and RTC takes its settings");
}

// A reset keeps the backup domain, and with it the time the run before set
static void rtc_keeps_time_over_reset(void)
{
  uint32_t now = (uint32_t)RTC_Timestamp();
  TEST_Check(now >= KNOWN_TIME_s && now < KNOWN_TIME_s + 60, "RTC at %u after the reset", now);
}

static void rtc_takes_a_time(void)
{
  TEST_Equal(RTC_SetTimestamp(KNOWN_TIME_s + 3600), OK, "write accepted");
  TEST_Near((double)RTC_Timestamp(), KNOWN_TIME_s + 3600.0, 1.0, "time read back");
}

// LSE runs and the prescaler divides it: 2s of `delay` are 2s of RTC, the HSI16 tolerance aside
static void rtc_second_lasts_a_second(void)
{
  uint64_t start_ms = RTC_TimestampMs();
  delay(2000);
  TEST_Near((double)(RTC_TimestampMs() - start_ms), 2000.0, 100.0, "2s of RTC");
}

//-------------------------------------------------------------------------------------------- test

void TEST_Cases(void)
{
  TEST(rtc_starts);
  TEST(rtc_keeps_time_over_reset);
  TEST(rtc_takes_a_time);
  TEST(rtc_second_lasts_a_second);
  TEST(clocks_fit_their_range);
}

GPIO_t led = { // green `LD2` of Nucleo-WB55RG
  .port = GPIOB,
  .pin = 0,
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
  // A software reset is the one of the run before; flags stay until cleared, so they go now
  bool again = RCC->CSR & RCC_CSR_SFTRSTF;
  RCC->CSR |= RCC_CSR_RMVF;
  sys_init(); // clock, systick and heap
  rtc_status = RTC_Init();
  if(!again) {
    RTC_SetTimestamp(KNOWN_TIME_s); // the time the run after the reset has to find
    PWR_Reset();
  }
  DBG_Init(&dbg_uart); // logs and shell on this port
  DBG_Enter();
  LOG_Info("Build: %s %s", __DATE__, __TIME__);
  LOG_Info("Target MCU: " ANSI_PINK "STM32WB55" ANSI_END);
  thread(DBG_Loop, stack_dbg);
  thread(test, stack_test);
  vrts_init();
  while(1); // `vrts_init` never returns
}
