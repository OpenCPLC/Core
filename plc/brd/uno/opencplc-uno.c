#include "opencplc.h"

//------------------------------------------------------------------------------------------------- EEPROM

#ifdef STM32G081xx
  EEPROM_t eeprom_plc = { .page_start = 62, .page_count = 2 };
  EEPROM_t eeprom_relay = { .page_start = 58, .page_count = 4 };
  EEPROM_t eeprom_io = { .page_start = 54, .page_count = 4 };
#endif
#ifdef STM32G0C1xx
  EEPROM_t eeprom_plc = { .page_start = 254, .page_count = 2 };
  EEPROM_t eeprom_relay = { .page_start = 250, .page_count = 4 };
  EEPROM_t eeprom_io = { .page_start = 246, .page_count = 4 };
#endif

//------------------------------------------------------------------------------------------------- DOUT-RO

DOUT_t RO1 = { .name = "RO1", .relay = true, .gpio = { .port = GPIOB, .pin = 7 }, .eeprom = &eeprom_relay, .save = true };
DOUT_t RO2 = { .name = "RO2", .relay = true, .gpio = { .port = GPIOB, .pin = 6 }, .eeprom = &eeprom_relay, .save = true };
DOUT_t RO3 = { .name = "RO3", .relay = true, .gpio = { .port = GPIOB, .pin = 5 }, .eeprom = &eeprom_relay, .save = true };
DOUT_t RO4 = { .name = "RO4", .relay = true, .gpio = { .port = GPIOB, .pin = 4 }, .eeprom = &eeprom_relay, .save = true };

//------------------------------------------------------------------------------------------------- DOUT-TO

PWM_t to_pwm = {
  .reg = TIM1,
  .auto_reload = PLC_ARR_INIT(SYS_CLOCK_FREQ, true), // 1kHz
  .channel[TIM_CH1] = TIM1_CH1_PC8,
  .channel[TIM_CH2] = TIM1_CH2_PC9,
  .channel[TIM_CH3] = TIM1_CH3_PC10,
  .channel[TIM_CH4] = TIM1_CH4_PC11,
  .center_aligned = true
};

DOUT_t TO1 = { .name = "TO1", .pwm = &to_pwm, .channel = TIM_CH4, .eeprom = &eeprom_io, .save = false };
DOUT_t TO2 = { .name = "TO2", .pwm = &to_pwm, .channel = TIM_CH3, .eeprom = &eeprom_io, .save = false };
DOUT_t TO3 = { .name = "TO3", .pwm = &to_pwm, .channel = TIM_CH2, .eeprom = &eeprom_io, .save = false };
DOUT_t TO4 = { .name = "TO4", .pwm = &to_pwm, .channel = TIM_CH1, .eeprom = &eeprom_io, .save = false };

void TO_Frequency(float frequency)
{
  PWM_Frequency(&to_pwm, frequency);
};

//------------------------------------------------------------------------------------------------- DOUT-XO

PWM_t xo_pwm = {
  .reg = TIM2,
  .prescaler = 1000,
  .auto_reload = PLC_ARR_INIT(SYS_CLOCK_FREQ, true), // 1Hz
  .channel[TIM_CH1] = TIM2_CH1_PA15,
  .channel[TIM_CH2] = TIM2_CH2_PB3,
  .center_aligned = true
};

DOUT_t XO1 = { .name = "XO1", .pwm = &xo_pwm, .channel = TIM_CH2, .eeprom = &eeprom_io, .save = false };
DOUT_t XO2 = { .name = "XO2", .pwm = &xo_pwm, .channel = TIM_CH1, .eeprom = &eeprom_io, .save = false };

void XO_Frequency(float frequency)
{
  PWM_Frequency(&xo_pwm, frequency);
}

//------------------------------------------------------------------------------------------------- DIN

EXTI_t din_trig3, din_trig4;

PWMI_t din_pwmi = {
  .reg = TIM3,
  .prescaler = 64,
  .capture_prescaler = PWMI_CapturePrescaler_1,
  .filter = TIM_Filter_FCLK_N2,
  .int_prioryty = IRQ_Priority_High,
  #if(!PWMI_AUTO_OVERSAMPLING)
    .oversampling = 16,
  #endif
  .trig3 = &din_trig3,
  .trig4 = &din_trig4
};

DIN_t DI1 = { .name = "DI1", .pwmi = &din_pwmi, .channel = TIM_CH1, .gpif = { .gpio = { .port = GPIOA, .pin = 6, .reverse = true } }, .eeprom = &eeprom_io };
DIN_t DI2 = { .name = "DI2", .pwmi = &din_pwmi, .channel = TIM_CH2, .gpif = { .gpio = { .port = GPIOA, .pin = 7, .reverse = true } }, .eeprom = &eeprom_io };
DIN_t DI3 = { .name = "DI3", .pwmi = &din_pwmi, .channel = TIM_CH3, .gpif = { .gpio = { .port = GPIOB, .pin = 0, .reverse = true } }, .eeprom = &eeprom_io };
DIN_t DI4 = { .name = "DI4", .pwmi = &din_pwmi, .channel = TIM_CH4, .gpif = { .gpio = { .port = GPIOB, .pin = 1, .reverse = true } }, .eeprom = &eeprom_io };

bool din_pwmi_init = false;

//------------------------------------------------------------------------------------------------- AIN

uint8_t ain_channels[] = {
  ADC_IN_PA0, ADC_IN_PA1, ADC_IN_PA5, ADC_IN_PB10
};

#define AIN_BUFFER_SIZE adc_record_buffer_size(AIN_AVERAGE_TIME_ms, 160, 64, sizeof(ain_channels))
#define AIN_SAMPLES (AIN_BUFFER_SIZE / sizeof(ain_channels))

uint16_t ain_buffer[AIN_BUFFER_SIZE];
uint16_t ain_data[sizeof(ain_channels)][AIN_SAMPLES];

ADC_t ain_adc = {
  .int_prioryty = IRQ_Priority_Low,
  .freq_16Mhz = false,
  .prescaler = ADC_Prescaler_4,
  .record = {
    .channels = ain_channels,
    .count = sizeof(ain_channels),
    .dma_nbr = DMA_Nbr_1,
    .sampling_time = ADC_SamplingTime_173,
    .oversampling.enable = true,
    .oversampling.ratio = ADC_OversamplingRatio_64,
    .oversampling.shift = 2,
    .buffer = ain_buffer,
    .buffer_length = AIN_BUFFER_SIZE
  }
};

AIN_t AI1 = { .name = "AI1", .data = ain_data[0], .count = AIN_SAMPLES };
AIN_t AI2 = { .name = "AI2", .data = ain_data[1], .count = AIN_SAMPLES };
AIN_t POT = { .name = "POT", .data = ain_data[3], .count = AIN_SAMPLES };
AIN_t VCC = { .name = "VCC", .data = ain_data[2], .count = AIN_SAMPLES };

float VCC_Voltage_V(void)
{
  float raw = AIN_Raw(&VCC);
  float value = resistor_divider_factor(3.3, 110, 10, 16) * raw;
  return value;
}

//------------------------------------------------------------------------------------------------- RS485

uint8_t rs1_buff_buffer[RS_BUFFER_SIZE];
BUFF_t rs1_buff = { .memory = rs1_buff_buffer, .size = RS_BUFFER_SIZE };
GPIO_t rs1_gpio_direction = { .port = GPIOA, .pin = 4, .speed = GPIO_Speed_VeryHigh };
UART_t RS1 = {
  .reg = USART2,
  .tx_pin = UART2_TX_PA2,
  .rx_pin = UART2_RX_PA3,
  .dma_nbr = 5,
  .UART_9600,
  .buff = &rs1_buff,
  .gpio_direction = &rs1_gpio_direction,
  .timeout = 40
};

uint8_t rs2_buff_buffer[RS_BUFFER_SIZE];
BUFF_t rs2_buff = { .memory = rs2_buff_buffer, .size = RS_BUFFER_SIZE };
GPIO_t rs2_gpio_direction = { .port = GPIOB, .pin = 2, .speed = GPIO_Speed_VeryHigh };
UART_t RS2 = {
  .reg = USART1,
  .tx_pin = UART1_TX_PC4,
  .rx_pin = UART1_RX_PC5,
  .dma_nbr = 6,
  .UART_9600,
  .buff = &rs2_buff,
  .gpio_direction = &rs2_gpio_direction,
  .timeout = 40
};

//------------------------------------------------------------------------------------------------- I2C+1WIRE

I2C_Master_t i2c_master = {
  .reg = I2C1,
  .scl_pin = I2C1_SCL_PA9,
  .sda_pin = I2C1_SDA_PA10,
  .pull_up = true,
  .int_prioryty = 1,
  .I2C_TIMING_100kHz
};

GPIO_t onewire_gpio = { .port = GPIOA, .pin = 10 };
TIM_t sleep_us_tim = { .reg = TIM7 };

void ONEWIRE_Active(void)
{
  sleep_us_init(&sleep_us_tim);
  I2C_Master_Disable(&i2c_master);
  // WIRE_Init(&onewire);
  return;
}

//------------------------------------------------------------------------------------------------- RGB+BTN

GPIO_t rgb_gpio_red = { .port = GPIOC, .pin = 7 };
GPIO_t rgb_gpio_green = { .port = GPIOA, .pin = 11 };
GPIO_t rgb_gpio_blue = { .port = GPIOA, .pin = 12 };

RGB_t RGB = { .red = &rgb_gpio_red, .green = &rgb_gpio_green, .blue = &rgb_gpio_blue };
DIN_t BTN = { .gpif = { .gpio = { .port = GPIOC, .pin = 12, .reverse = true } } };

//------------------------------------------------------------------------------------------------- DBG+Bash

#ifdef STM32G081xx
  TIM_t dbg_tim = { .reg = TIM6 };
#endif
UART_t dbg_uart = {
  .reg = USART3,
  .tx_pin = UART3_TX_PB8,
  .rx_pin = UART3_RX_PB9,
  .dma_nbr = DMA_Nbr_4,
  .irq_priority = IRQ_Priority_Low,
  .UART_115200,
  #ifdef STM32G081xx
    .tim = &dbg_tim
  #endif
};
uint8_t cache_file_buffer[2048];
FILE_t cache_file = { .name = "cache", .buffer = cache_file_buffer, .limit = sizeof(cache_file_buffer) };

//------------------------------------------------------------------------------------------------- Functions PLC

void PLC_Init(void)
{
  vrts_lock();
  #if PLC_BOOTLOADER
    // SCB->VTOR = FLASH_BASE | 0x00000000U;
  #endif
  // Konfiguracja systemowa
  clock_init();
  systick_init(PLC_BASETIME);
  heap_init();
  RTC_Init();
  CACHE_Init(&eeprom_plc);
  RGB_Init(&RGB);
  DIN_Init(&BTN);
  DBG_Init(&dbg_uart);
  BASH_AddFile(&cache_file);
  BASH_AddCallback(&LED_Bash, "LED");
  BASH_AddCallback(&DOUT_Bash, "DOUT");
  // Magistrala I2C
  TWI_Init(&i2c_master);
  // Wyjścia cyfrowe przekaźnikowe (RO)
  DOUT_Init(&RO1); DOUT_Add2Bash(&RO1);
  DOUT_Init(&RO2); DOUT_Add2Bash(&RO2);
  DOUT_Init(&RO3); DOUT_Add2Bash(&RO3);
  DOUT_Init(&RO4); DOUT_Add2Bash(&RO4);
  // Wyjścia cyfrowe tranzystorowe (TO)
  DOUT_Init(&TO1); DOUT_Add2Bash(&TO1);
  DOUT_Init(&TO2); DOUT_Add2Bash(&TO2);
  DOUT_Init(&TO3); DOUT_Add2Bash(&TO3);
  DOUT_Init(&TO4); DOUT_Add2Bash(&TO4);
  PWM_Init(&to_pwm);
  // Wyjścia cyfrowe triakowe (XO)
  DOUT_Init(&XO1); DOUT_Add2Bash(&XO1);
  DOUT_Init(&XO2); DOUT_Add2Bash(&XO2);
  PWM_Init(&xo_pwm);
  // Wejścia cyfrowe (DI)
  if(!DIN_Init(&DI1)) {
    din_pwmi.channel[TIM_CH1] = TIM3_CH1_PA6;
    din_pwmi_init = true;
  }
  if(!DIN_Init(&DI2)) {
    din_pwmi.channel[TIM_CH2] = TIM3_CH2_PA7;
    din_pwmi_init = true;
  }
  if(!DIN_Init(&DI3)) {
    din_pwmi.channel[TIM_CH3] = TIM3_CH3_PB0;
    din_pwmi_init = true;
  }
  if(!DIN_Init(&DI4)) {
    din_pwmi.channel[TIM_CH4] = TIM3_CH4_PB1;
    din_pwmi_init = true;
  }
  if(din_pwmi_init) PWMI_Init(&din_pwmi);
  // Wejścia analogowe (AI)
  ADC_Init(&ain_adc);
  ADC_Record(&ain_adc);
  ADC_Wait(&ain_adc);
  // Interfejsy RS485
  UART_Init(&RS1);
  UART_Init(&RS2);
  LOG_Init(PLC_GREETING, PRO_VERSION);
  vrts_unlock();
}

void PLC_Loop(void)
{
  while(1) {
    // Dioda LED i przycisk (BTN)
    RGB_Loop(&RGB);
    DIN_Loop(&BTN);
    // Wyjścia przekaźnikowe (RO)
    DOUT_Loop(&RO1);
    DOUT_Loop(&RO2);
    DOUT_Loop(&RO3);
    DOUT_Loop(&RO4);
    // Wyjścia cyfrowe tranzystorowe (TO)
    DOUT_Loop(&TO1);
    DOUT_Loop(&TO2);
    DOUT_Loop(&TO3);
    DOUT_Loop(&TO4);
    // Wyjścia cyfrowe triakowe (XO)
    DOUT_Loop(&XO1);
    DOUT_Loop(&XO2);
    // Wejścia cyfrowe (DI)
    DIN_Loop(&DI1);
    DIN_Loop(&DI2);
    DIN_Loop(&DI3);
    DIN_Loop(&DI4);
    if(din_pwmi_init && PWMI_Loop(&din_pwmi)) {
      // PWMI_Print(&din_pwmi);
    }
    // Wejścia analogowe (AI)
    if(ADC_IsFree(&ain_adc)) {
      if(ain_adc.overrun) LOG_Debug("ADC overrun"), ain_adc.overrun = 0;
      else AIN_Sort(ain_buffer, sizeof(ain_channels), AIN_SAMPLES, ain_data);
      ADC_Record(&ain_adc);
    }
    let();
  }
}

void PLC_Main(void)
{
  PLC_Init();
  PLC_Loop();
}

//------------------------------------------------------------------------------------------------- RTD

GPIO_t rtd_gpio_drdy = { .port = GPIOB, .pin = 11, .reverse = true };
GPIO_t rtd_gpio_cs = { .port = GPIOB, .pin = 12, .reverse = true };

SPI_Master_t rtd_spi = {
  .reg = SPI2,
  .tx_dma_nbr = DMA_Nbr_2,
  .rx_dma_nbr = DMA_Nbr_3,
  .int_prioryty = IRQ_Priority_Medium,
  .miso_pin = SPI2_MISO_PB14,
  .mosi_pin = SPI2_MOSI_PB15,
  .sck_pin = SPI2_SCK_PB13,
  .cs_gpio = &rtd_gpio_cs,
  .cs_delay = 10,
  .prescaler = SPI_Prescaler_32,
  .cpol = true,
  .cpha = true
};

MAX31865_t RTD = {
  .name = "RTD",
  .spi = &rtd_spi,
  .ready = &rtd_gpio_drdy,
  .nominal_ohms = RTD_Type_PT100,
  .wire = RTD_Wire_3,
  .reject = RTD_Reject_60Hz
};

void RTD_Main(void)
{
  SPI_Master_Init(&rtd_spi);
  MAX31865_Init(&RTD);
  while(1) {
    MAX31865_Loop(&RTD);
    let();
  }
}

//-------------------------------------------------------------------------------------------------
