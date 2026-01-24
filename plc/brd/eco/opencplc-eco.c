#include "opencplc.h"

//------------------------------------------------------------------------------------------------- EEPROM

#ifdef STM32G081xx
  EEPROM_t io_eeprom[] = {
    { .page_a = 60, .page_b = 61 },
    { .page_a = 58, .page_b = 59 }
  };
#endif
#ifdef STM32G0C1xx
  EEPROM_t io_eeprom[] = {
    { .page_a = 252, .page_b = 253 },
    { .page_a = 250, .page_b = 251 }
  };
#endif

//------------------------------------------------------------------------------------------------- DOUT-RO

DOUT_t RO1 = { .name = "RO1", .relay = true, .gpio = { .port = GPIOD, .pin = 0 }, .eeprom = &io_eeprom[0], .save = true };
DOUT_t RO2 = { .name = "RO2", .relay = true, .gpio = { .port = GPIOD, .pin = 1 }, .eeprom = &io_eeprom[0], .save = true };

//------------------------------------------------------------------------------------------------- DOUT-TO

PWM_t to1_pwm = {
  .reg = TIM16,
  #if(SYS_CLOCK_FREQ == 16000000)
    .prescaler = 16,
  #elif(SYS_CLOCK_FREQ == 18432000)
    // .prescaler = TODO, 
  #endif
  .channel[TIM_CH1] = TIM16_CH1_PB8,
  .auto_reload = 1000,
  .center_aligned = false
};

DOUT_t TO1 = { .name = "TO1", .pwm = &to1_pwm, .channel = TIM_CH1, .eeprom = &io_eeprom[1], .save = false };

void TO1_Frequency(float frequency)
{
  PWM_Frequency(&to1_pwm, frequency);
};

PWM_t to2_pwm = {
  .reg = TIM17,
  #if(SYS_CLOCK_FREQ == 16000000)
    .prescaler = 16,
  #elif(SYS_CLOCK_FREQ == 18432000)
    // .prescaler = TODO, 
  #endif
  .channel[TIM_CH1] = TIM17_CH1_PB9,
  .auto_reload = 1000,
  .center_aligned = false
};

DOUT_t TO2 = { .name = "TO2", .pwm = &to2_pwm, .channel = TIM_CH1, .eeprom = &io_eeprom[1], .save = false };

void TO2_Frequency(float frequency)
{
  PWM_Frequency(&to2_pwm, frequency);
};

PWM_t to345_pwm = {
  .reg = TIM1,
  #if(SYS_CLOCK_FREQ == 16000000)
    .prescaler = 16,
  #elif(SYS_CLOCK_FREQ == 18432000)
    // .prescaler = TODO,
  #endif
  .channel[TIM_CH1] = TIM1_CH1_PA8,
  .channel[TIM_CH2] = TIM1_CH2_PB3,
  .channel[TIM_CH3] = TIM1_CH3_PB6,
  .auto_reload = 1000,
  .center_aligned = true
};

DOUT_t TO3 = { .name = "TO3", .pwm = &to345_pwm, .channel = TIM_CH2, .eeprom = &io_eeprom[1], .save = false };
DOUT_t TO4 = { .name = "TO4", .pwm = &to345_pwm, .channel = TIM_CH3, .eeprom = &io_eeprom[1], .save = false };
DOUT_t TO5 = { .name = "TO5", .pwm = &to345_pwm, .channel = TIM_CH1, .eeprom = &io_eeprom[1], .save = false };

void TO345_Frequency(float frequency)
{
  PWM_Frequency(&to345_pwm, frequency);
};

//------------------------------------------------------------------------------------------------- DIN

PWMI_t din_pwmi15 = {
  .reg = TIM15,
  .prescaler = 49,
  // .channel[TIM_CH1] = TIM15_CH1_PB14,
  // .channel[TIM_CH2] = TIM15_CH2_PB15,
  .input_prescaler = PWMI_Prescaler_1,
  .filter = TIM_Filter_NoFilter,
  .oversampling = 4
};

DIN_t DI1 = { .name = "DI1", .gpif = { .gpio = { .port = GPIOB, .pin = 14, .reverse = true } }, .eeprom = &io_eeprom[1] };
DIN_t DI2 = { .name = "DI2", .gpif = { .gpio = { .port = GPIOB, .pin = 15, .reverse = true } }, .eeprom = &io_eeprom[1] };

bool din_pwmi15_init = false;

PWMI_t din_pwmi3 = {
  .reg = TIM3,
  .prescaler = 49,
  // .channel[TIM_CH1] = TIM3_CH1_PC6,
  // .channel[TIM_CH2] = TIM3_CH2_PC7,
  .input_prescaler = PWMI_Prescaler_1,
  .filter = TIM_Filter_NoFilter,
  .oversampling = 4
};

DIN_t DI3 = { .name = "DI3", .gpif = { .gpio = { .port = GPIOC, .pin = 6, .reverse = true } }, .eeprom = &io_eeprom[1] };
DIN_t DI4 = { .name = "DI4", .gpif = { .gpio = { .port = GPIOC, .pin = 7, .reverse = true } }, .eeprom = &io_eeprom[1] };

bool din_pwmi3_init = false;

//------------------------------------------------------------------------------------------------- AIN

uint8_t ain_channels[] = {
  ADC_IN_PA0, ADC_IN_PA1, ADC_IN_PA5, ADC_IN_PA6, ADC_IN_PA7,
  ADC_IN_PB0, ADC_IN_PB1, ADC_IN_PB2, ADC_IN_PB10, ADC_IN_PB11, ADC_IN_PB12
};

#define AIN_BUFFER_SIZE adc_record_buffer_size(AIN_AVERAGE_TIME_MS, 160, 32, sizeof(ain_channels))
#define AIN_SAMPLES (AIN_BUFFER_SIZE / sizeof(ain_channels))

uint16_t ain_buffer[AIN_BUFFER_SIZE];
uint16_t ain_data[sizeof(ain_channels)][AIN_SAMPLES];

ADC_t ain_adc = {
  .frequency = ADC_Frequency_16MHz,
  .interrupt_level = 3,
  .record = {
    .channels = ain_channels,
    .count = sizeof(ain_channels),
    .dma_nbr = 1,
    .sampling_time = ADC_SamplingTime_160,
    .oversampling_enable = true,
    .oversampling_ratio = ADC_OversamplingRatio_32,
    .oversampling_shift = 1,
    .buffer = ain_buffer,
    .buffer_length = AIN_BUFFER_SIZE
  }
};

AIN_t AI1 = { .name = "AI1", .data = ain_data[1], .count = AIN_SAMPLES };
AIN_t AI2 = { .name = "AI2", .data = ain_data[2], .count = AIN_SAMPLES };
AIN_t AI3 = { .name = "AI3", .data = ain_data[3], .count = AIN_SAMPLES };
AIN_t AI4 = { .name = "AI4", .data = ain_data[4], .count = AIN_SAMPLES };
AIN_t POT1 = { .name = "POT1", .data = ain_data[10], .count = AIN_SAMPLES };
AIN_t POT2 = { .name = "POT2", .data = ain_data[9], .count = AIN_SAMPLES };
AIN_t POT3 = { .name = "POT3", .data = ain_data[8], .count = AIN_SAMPLES };
AIN_t POT4 = { .name = "POT4", .data = ain_data[7], .count = AIN_SAMPLES };
AIN_t POT5 = { .name = "POT5", .data = ain_data[6], .count = AIN_SAMPLES };
AIN_t POT6 = { .name = "POT6", .data = ain_data[5], .count = AIN_SAMPLES };
AIN_t VCC = { .name = "VCC", .data = ain_data[0], .count = AIN_SAMPLES };

float VCC_Voltage_V(void)
{
  float raw = AIN_Raw(&VCC);
  float value = resistor_divider_factor(3.3, 110, 10, 16) * raw;
  return value;
}

//------------------------------------------------------------------------------------------------- RS485

uint8_t rs_buff_buffer[RS_BUFFER_SIZE];
BUFF_t rs_buff = { .mem = rs_buff_buffer, .size = RS_BUFFER_SIZE };
GPIO_t rs_gpio_direction = { .port = GPIOA, .pin = 4, .speed = GPIO_Speed_VeryHigh };
UART_t RS = {
  .reg = USART2,
  .tx_pin = UART2_TX_PA2,
  .rx_pin = UART2_RX_PA3,
  .dma_nbr = 5,
  .UART_9600,
  .buff = &rs_buff,
  .gpio_direction = &rs_gpio_direction
};

//------------------------------------------------------------------------------------------------- RGB+BTN

GPIO_t rgb_gpio_red = { .port = GPIOA, .pin = 11 };
GPIO_t rgb_gpio_green = { .port = GPIOA, .pin = 12 };
GPIO_t rgb_gpio_blue = { .port = GPIOA, .pin = 15 };

RGB_t RGB = { .red = &rgb_gpio_red, .green = &rgb_gpio_green, .blue = &rgb_gpio_blue };
DIN_t BTN1 = { .gpif = { .gpio = { .port = GPIOD, .pin = 3, .reverse = true } } };
DIN_t BTN2 = { .gpif = { .gpio = { .port = GPIOB, .pin = 4, .reverse = true } } };
DIN_t BTN3 = { .gpif = { .gpio = { .port = GPIOB, .pin = 5, .reverse = true } } };
DIN_t SW1 = { .gpif = { .gpio = { .port = GPIOB, .pin = 7 } } };
DIN_t SW2 = { .gpif = { .gpio = { .port = GPIOC, .pin = 13 } } };

//------------------------------------------------------------------------------------------------- DBG+Bash

UART_t dbg_uart = {
  .reg = USART1,
  .tx_pin = UART1_TX_PA9,
  .rx_pin = UART1_RX_PA10,
  .dma_nbr = DMA_Nbr_4,
  .int_prioryty = INT_Prioryty_Low,
  .UART_115200
};
#ifdef STM32G081xx
  EEPROM_t cache_eeprom = { .page_a = 62, .page_b = 63 };
#endif
#ifdef STM32G0C1xx
  EEPROM_t cache_eeprom = { .page_a = 254, .page_b = 255 };
#endif
uint8_t cache_file_buffer[2048];
FILE_t cache_file = { .name = "cache", .buffer = cache_file_buffer, .limit = sizeof(cache_file_buffer) };

//------------------------------------------------------------------------------------------------- PLC

void PLC_Init(void)
{
  #if PLC_BOOTLOADER
    // SCB->VTOR = FLASH_BASE | 0x00000000U;
  #endif
  // Konfiguracja systemowa
  sys_clock_init();
  systick_init(PLC_BASETIME);
  EEPROM_Cache(&cache_eeprom);
  DBG_Init(&dbg_uart);
  RGB_Init(&RGB);
  DIN_Init(&BTN1);
  DIN_Init(&BTN2);
  DIN_Init(&BTN3);
  DIN_Init(&SW1);
  DIN_Init(&SW2);
  BASH_AddFile(&cache_file);
  BASH_AddCallback(&LED_Bash, "led");
  // Wyjścia cyfrowe przekaźnikowe (RO)
  DOUT_Init(&RO1);
  DOUT_Init(&RO2);
  // Wyjścia cyfrowe tranzystorowe (TO)
  DOUT_Init(&TO1);
  DOUT_Init(&TO2);
  DOUT_Init(&TO3);
  DOUT_Init(&TO4);
  DOUT_Init(&TO5);
  PWM_Init(&to1_pwm);
  PWM_Init(&to2_pwm);
  PWM_Init(&to345_pwm);
  // Wejścia cyfrowe (DI)
  if(DIN_Init(&DI1)) {
    din_pwmi15.channel[TIM_CH1] = TIM15_CH1_PB14;
    DI1.frequency = &din_pwmi15.frequency[0];
    DI1.fill = &din_pwmi15.fill[0];
    din_pwmi15_init = true;
  }
  if(DIN_Init(&DI2)) {
    din_pwmi15.channel[TIM_CH2] = TIM15_CH2_PB15;
    DI2.frequency = &din_pwmi15.frequency[0];
    DI2.fill = &din_pwmi15.fill[0];
    din_pwmi15_init = true;
  }
  if(DIN_Init(&DI3)) {
    din_pwmi3.channel[TIM_CH1] = TIM3_CH1_PC6;
    DI3.frequency = &din_pwmi3.frequency[0];
    DI3.fill = &din_pwmi3.fill[0];
    din_pwmi3_init = true;
  }
  if(DIN_Init(&DI4)) {
    din_pwmi3.channel[TIM_CH2] = TIM3_CH2_PC7;
    DI4.frequency = &din_pwmi3.frequency[0];
    DI4.fill = &din_pwmi3.fill[0];
    din_pwmi3_init = true;
  }
  // Wejścia analogowe (AI)
  ADC_Init(&ain_adc);
  ADC_Record(&ain_adc);
  ADC_Wait(&ain_adc);
  // Interfejsy RS485
  UART_Init(&RS);
  LOG_Init(PLC_GREETING, PRO_VERSION);
}

void PLC_Loop(void)
{
  // Dioda LED, przyciski (BTN), przełączniki (SW)
  RGB_Loop(&RGB);
  DIN_Loop(&BTN1);
  DIN_Loop(&BTN2);
  DIN_Loop(&BTN3);
  DIN_Loop(&SW1);
  DIN_Loop(&SW2);
  // Wyjścia przekaźnikowe (RO)
  DOUT_Loop(&RO1);
  DOUT_Loop(&RO2);
  // Wyjścia cyfrowe tranzystorowe (TO)
  DOUT_Loop(&TO1);
  DOUT_Loop(&TO2);
  DOUT_Loop(&TO3);
  DOUT_Loop(&TO4);
  DOUT_Loop(&TO5);
  // Wejścia cyfrowe (DI)
  DIN_Loop(&DI1);
  DIN_Loop(&DI2);
  DIN_Loop(&DI3);
  DIN_Loop(&DI4);
  if(din_pwmi15_init && PWMI_Loop(&din_pwmi3)) {
    // PWMI_Print(&din_pwmi3_init);
  }
  if(din_pwmi3_init && PWMI_Loop(&din_pwmi3)) {
    // PWMI_Print(&din_pwmi3_init);
  }
  // Wejścia analogowe (AI)
  if(ADC_IsFree(&ain_adc)) {
    AIN_Sort(ain_buffer, sizeof(ain_channels), AIN_SAMPLES, ain_data);
    ADC_Record(&ain_adc);
  }
  if(ain_adc.overrun) {
    LOG_Warning("ADC over-run %d", ain_adc.overrun);
    ain_adc.overrun = 0;
  }
}

void PLC_Main(void)
{
  PLC_Init();
  while(1) {
    PLC_Loop();
    let();
  }
}

//-------------------------------------------------------------------------------------------------