// .tests/g081/main.c

#include <vrts.h>
#include <sys.h>
#include <rtc.h>
#include <log.h>

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

//--------------------------------------------------------------------------------------------- app

GPIO_t led = { // Nucleo-G071RB green `LD2`
  .port = GPIOA,
  .pin = 5,
  .mode = GPIO_Mode_Output
};

void loop(void)
{
  GPIO_Init(&led);
  while(1) {
    GPIO_Tgl(&led);
    LOG_Info("Do nothing");
    delay(1000);
  }
}

//-------------------------------------------------------------------------------------------- main

stack(stack_dbg, 256);
stack(stack_loop, 256);

int main(void)
{
  sys_init(); // clock, systick and heap
  RTC_Init();
  DBG_Init(&dbg_uart); // logs and shell on this port
  DBG_Enter();
  LOG_Info("Build: %s %s", __DATE__, __TIME__);
  LOG_Info("Target MCU: " ANSI_PINK "STM32G081" ANSI_END);
  thread(DBG_Loop, stack_dbg);
  thread(loop, stack_loop);
  vrts_init();
  while(1); // `vrts_init` never returns
}
