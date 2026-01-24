// lib/per/uart.c

#include "uart.h"

//-------------------------------------------------------------------------------------------------
// DMAMUX request IDs
//-------------------------------------------------------------------------------------------------

#if defined(STM32G0)
  #ifndef DMAMUX_REQ_USART1_TX
    #define DMAMUX_REQ_USART1_TX  51
    #define DMAMUX_REQ_USART2_TX  53
    #define DMAMUX_REQ_USART3_TX  55
    #define DMAMUX_REQ_USART4_TX  57
    #define DMAMUX_REQ_LPUART1_TX 59
    #define DMAMUX_REQ_LPUART2_TX 61
  #endif
#elif defined(STM32WB)
  #define DMAMUX_REQ_USART1_TX  15
  #define DMAMUX_REQ_LPUART1_TX 17
#endif

//-------------------------------------------------------------------------------------------------
// Interrupt handlers
//-------------------------------------------------------------------------------------------------

static void UART_InterruptDMA(UART_t *uart)
{
  if(uart->dma.reg->ISR & DMA_ISR_TCIF(uart->dma.pos)) {
    uart->dma.reg->IFCR = DMA_ISR_TCIF(uart->dma.pos);
    uart->reg->CR1 |= USART_CR1_TCIE;
    uart->tx_flag = false;
  }
}

static void UART_InterruptEV(UART_t *uart)
{
  if(uart->reg->ISR & USART_ISR_RXNE_RXFNE) {
    uint8_t value = (uint8_t)uart->reg->RDR;
    BUFF_Push(uart->buff, value);
    if(uart->tim) {
      TIM_ResetValue(uart->tim);
      TIM_Enable(uart->tim);
    }
  }
  if((uart->reg->CR1 & USART_CR1_TCIE) && (uart->reg->ISR & USART_ISR_TC)) {
    uart->reg->CR1 &= ~USART_CR1_TCIE;
    uart->reg->ICR = USART_ICR_TCCF;
    uart->tc_flag = false;
    if(uart->gpio_direction) GPIO_Rst(uart->gpio_direction);
  }
  if(uart->reg->ISR & USART_ISR_RTOF) {
    uart->reg->ICR = USART_ICR_RTOCF;
    BUFF_Break(uart->buff);
  }
}

//-------------------------------------------------------------------------------------------------

static void UART_DmaSetup(UART_t *uart)
{
  DMA_SetRegisters(uart->dma_nbr, &uart->dma);
  RCC_EnableDMA(uart->dma.reg);
  uart->dma.mux->CCR &= ~0x3Fu;
  switch((uint32_t)uart->reg) {
    case (uint32_t)USART1:  uart->dma.mux->CCR |= DMAMUX_REQ_USART1_TX; break;
    #ifdef USART2
    case (uint32_t)USART2:  uart->dma.mux->CCR |= DMAMUX_REQ_USART2_TX; break;
    #endif
    #ifdef USART3
    case (uint32_t)USART3:  uart->dma.mux->CCR |= DMAMUX_REQ_USART3_TX; break;
    #endif
    #ifdef USART4
    case (uint32_t)USART4:  uart->dma.mux->CCR |= DMAMUX_REQ_USART4_TX; break;
    #endif
    #ifdef UART4
    case (uint32_t)UART4:   uart->dma.mux->CCR |= DMAMUX_REQ_USART4_TX; break;
    #endif
    #ifdef UART5
    case (uint32_t)UART5:   uart->dma.mux->CCR |= DMAMUX_REQ_USART5_TX; break;
    #endif
    case (uint32_t)LPUART1: uart->dma.mux->CCR |= DMAMUX_REQ_LPUART1_TX; break;
    #ifdef LPUART2
    case (uint32_t)LPUART2: uart->dma.mux->CCR |= DMAMUX_REQ_LPUART2_TX; break;
    #endif
  }
  DMA_ClearFlags(&uart->dma);
  uart->dma.cha->CCR = 0;
  uart->dma.cha->CPAR = (uint32_t)&uart->reg->TDR;
  uart->dma.cha->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE;
}

static void UART_SetBaudrate(UART_t *uart)
{
  bool lpuart = ((uint32_t)uart->reg == (uint32_t)LPUART1);
  #ifdef LPUART2
  lpuart |= ((uint32_t)uart->reg == (uint32_t)LPUART2);
  #endif
  if(lpuart) uart->reg->BRR = ((uint64_t)256 * SystemCoreClock + uart->baud / 2) / uart->baud;
  else uart->reg->BRR = (SystemCoreClock + uart->baud / 2) / uart->baud;
}

bool UART_IsReady(UART_t *uart)
{
  uint32_t isr = uart->reg->ISR;
  return (isr & USART_ISR_TEACK) && (isr & USART_ISR_REACK);
}

//-------------------------------------------------------------------------------------------------

/**
 * @brief Initialize UART with DMA, GPIO and optional timeout
 * @param[in] uart Pointer to `UART_t` control structure
 */
void UART_Init(UART_t *uart)
{
  // Direction GPIO (RS485)
  if(uart->gpio_direction) {
    uart->gpio_direction->mode = GPIO_Mode_Output;
    GPIO_Init(uart->gpio_direction);
  }
  // Buffer
  BUFF_Init(uart->buff);
  // DMA setup
  UART_DmaSetup(uart);
  // UART clock
  RCC_EnableUART(uart->reg);
  // GPIO
  GPIO_InitAlternate(&UART_TX_MAP[uart->tx_pin], false);
  GPIO_InitAlternate(&UART_RX_MAP[uart->rx_pin], false);
  // Baudrate
  UART_SetBaudrate(uart);
  // Reset registers
  uart->reg->CR1 = UART_CR1_RESET;
  uart->reg->CR2 = UART_CR2_RESET;
  uart->reg->CR3 = UART_CR3_RESET;
  uart->reg->ICR = UART_ICR_CLEAR;
  uart->reg->RQR = USART_RQR_RXFRQ;
  // DMA TX, overrun disable
  uart->reg->CR3 |= USART_CR3_DMAT | USART_CR3_OVRDIS;
  // Stop bits
  switch(uart->stop_bits) {
    case UART_StopBits_0_5: uart->reg->CR2 |= USART_CR2_STOP_0; break;
    case UART_StopBits_1:   break;
    case UART_StopBits_2:   uart->reg->CR2 |= USART_CR2_STOP_1; break;
    case UART_StopBits_1_5: uart->reg->CR2 |= USART_CR2_STOP_0 | USART_CR2_STOP_1; break;
  }
  // Parity
  switch(uart->parity) {
    case UART_Parity_None: break;
    case UART_Parity_Odd:  uart->reg->CR1 |= USART_CR1_PCE | USART_CR1_PS; break;
    case UART_Parity_Even: uart->reg->CR1 |= USART_CR1_PCE; break;
  }
  // Timeout (timer or hardware RTO)
  if(uart->tim) {
    uart->tim->prescaler = 100;
    uint64_t nbr = ((uint64_t)SystemCoreClock / uart->tim->prescaler) * uart->timeout + uart->baud / 2;
    uart->tim->auto_reload = (uint32_t)(nbr / uart->baud);
    uart->tim->function = (void (*)(void*))BUFF_Break;
    uart->tim->function_struct = (void*)uart->buff;
    uart->tim->irq_priority = uart->irq_priority;
    uart->tim->one_pulse_mode = true;
    if(uart->timeout) {
      uart->tim->enable = true;
      uart->tim->enable_interrupt = true;
    }
    TIM_Init(uart->tim);
  }
  else if(uart->timeout) {
    uart->reg->RTOR = uart->timeout;
    uart->reg->CR1 |= USART_CR1_RTOIE;
    uart->reg->CR2 |= USART_CR2_RTOEN;
  }
  // IRQ enable (unified API)
  uart->init_flag = true;
  IRQ_EnableDMA(uart->dma_nbr, uart->irq_priority, (void (*)(void*))&UART_InterruptDMA, uart);
  IRQ_EnableUART(uart->reg, uart->irq_priority, (void (*)(void*))&UART_InterruptEV, uart);
  // Enable UART
  uart->reg->CR1 |= USART_CR1_RXNEIE_RXFNEIE | USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
  // Wait for ready
  while(!UART_IsReady(uart)) __NOP();
}

/**
 * @brief Reinitialize UART
 * @param[in] uart Pointer to `UART_t` control structure
 */
void UART_ReInit(UART_t *uart)
{
  while(UART_SendActive(uart)) __NOP();
  uart->reg->CR3 &= ~USART_CR3_DMAT;
  uart->reg->ICR = UART_ICR_CLEAR;
  uart->reg->RQR = USART_RQR_RXFRQ;
  uart->reg->CR1 &= ~USART_CR1_UE;
  while(UART_IsReady(uart)) __NOP();
  RCC_DisableUART(uart->reg);
  UART_Init(uart);
}

/**
 * @brief Set timeout value
 * @param[in] uart Pointer to `UART_t` control structure
 * @param[in] timeout Timeout in bit times
 */
void UART_SetTimeout(UART_t *uart, uint16_t timeout)
{
  uart->timeout = timeout;
  if(uart->tim) {
    if(timeout) {
      TIM_SetAutoreload(uart->tim, (float)SystemCoreClock * timeout / uart->baud / 100);
      TIM_Disable(uart->tim);
      TIM_InterruptEnable(uart->tim);
      TIM_Enable(uart->tim);
    }
    else {
      TIM_InterruptDisable(uart->tim);
      TIM_ResetValue(uart->tim);
    }
  }
  else {
    if(timeout) {
      uart->reg->RTOR = timeout;
      uart->reg->CR1 |= USART_CR1_RTOIE;
      uart->reg->CR2 |= USART_CR2_RTOEN;
    }
    else {
      uart->reg->CR2 &= ~USART_CR2_RTOEN;
      uart->reg->CR1 &= ~USART_CR1_RTOIE;
    }
  }
}

//-------------------------------------------------------------------------------------------------
// Status flags
//-------------------------------------------------------------------------------------------------

bool UART_SendCompleted(UART_t *uart) { return !uart->tc_flag; }
bool UART_SendActive(UART_t *uart) { return uart->tc_flag; }
bool UART_IsBusy(UART_t *uart) { return uart->tx_flag; }
bool UART_IsFree(UART_t *uart) { return !uart->tx_flag; }

//-------------------------------------------------------------------------------------------------
// Send
//-------------------------------------------------------------------------------------------------

/**
 * @brief Start UART transmit via DMA
 * @param[in] uart Pointer to `UART_t` control structure
 * @param[in] data Pointer to data buffer
 * @param[in] len Number of bytes to send
 * @return `OK` if started, `ERR` if not initialized, `BUSY` if transmitting
 */
status_t UART_Send(UART_t *uart, uint8_t *data, uint16_t len)
{
  if(!uart->init_flag) return ERR;
  if(uart->tx_flag) return BUSY;
  if(uart->gpio_direction) GPIO_Set(uart->gpio_direction);
  uart->dma.cha->CCR &= ~DMA_CCR_EN;
  uart->dma.cha->CMAR = (uint32_t)data;
  uart->dma.cha->CNDTR = len;
  if(uart->prefix) uart->reg->TDR = uart->prefix;
  uart->dma.cha->CCR |= DMA_CCR_EN;
  uart->tx_flag = true;
  uart->tc_flag = true;
  return OK;
}

//-------------------------------------------------------------------------------------------------
// Receive
//-------------------------------------------------------------------------------------------------

uint16_t UART_Size(UART_t *uart) { return BUFF_Size(uart->buff); }
uint16_t UART_Read(UART_t *uart, uint8_t *array) { return BUFF_Read(uart->buff, array); }
char *UART_ReadString(UART_t *uart) { return BUFF_ReadString(uart->buff); }
bool UART_Skip(UART_t *uart) { return BUFF_Skip(uart->buff); }
void UART_Clear(UART_t *uart) { BUFF_Clear(uart->buff); }

//-------------------------------------------------------------------------------------------------
// Utils
//-------------------------------------------------------------------------------------------------

/**
 * @brief Calculate transmission time for frame
 * @param[in] uart Pointer to `UART_t` control structure
 * @param[in] len Frame length in bytes
 * @return Transmission time in milliseconds
 */
uint32_t UART_CalcTime_ms(UART_t *uart, uint16_t len)
{
  uint32_t bits = 10; // 1 start + 8 data + 1 stop
  if(uart->parity) bits++;
  switch(uart->stop_bits) {
    case UART_StopBits_0_5:
    case UART_StopBits_1:   break;
    case UART_StopBits_1_5:
    case UART_StopBits_2:   bits++; break;
  }
  uint64_t total_bits = (uint64_t)bits * len + uart->timeout;
  return (uint32_t)((total_bits * 1000) / uart->baud);
}

//-------------------------------------------------------------------------------------------------
