#include "opencplc.h"

//------------------------------------------------------------------------------------------------- EEPROM

#ifdef STM32G081xx
  EEPROM_t io_eeprom[] = {
    { .page_a = 60, .page_b = 61 },
    { .page_a = 58, .page_b = 59 },
    { .page_a = 56, .page_b = 57 },
    { .page_a = 54, .page_b = 55 },
  };
#endif
#ifdef STM32G0C1xx
  EEPROM_t io_eeprom[] = {
    { .page_a = 252, .page_b = 253 },
    { .page_a = 250, .page_b = 251 },
    { .page_a = 248, .page_b = 249 },
    { .page_a = 246, .page_b = 247 }
  };
#endif

//------------------------------------------------------------------------------------------------- DOUT-TO

PWM_t to149c_pwm = {
  .reg = TIM1,
  .auto_reload = PLC_ARR_INIT(SYS_CLOCK_FREQ, true),
  .channel[TIM_CH1] = TIM1_CH1_PA8,
  .channel[TIM_CH2] = TIM1_CH2_PA9,
  .channel[TIM_CH3] = TIM1_CH3_PC10,
  .channel[TIM_CH4] = TIM1_CH4_PC11,
  .center_aligned = true
};

DOUT_t TO1 = { .name = "TO1", .pwm = &to149c_pwm, .channel = TIM_CH4, .eeprom = &io_eeprom[0], .save = false };
DOUT_t TO4 = { .name = "TO4", .pwm = &to149c_pwm, .channel = TIM_CH3, .eeprom = &io_eeprom[0], .save = false };
DOUT_t TO9 = { .name = "TO9", .pwm = &to149c_pwm, .channel = TIM_CH1, .eeprom = &io_eeprom[1], .save = false };
DOUT_t TO12 = { .name = "TO12", .pwm = &to149c_pwm, .channel = TIM_CH2, .eeprom = &io_eeprom[1], .save = false };

void TO149C_Frequency(float frequency)
{
  PWM_Frequency(&to149c_pwm, frequency);
};

PWM_t to5_pwm = {
  .reg = TIM3,
  .auto_reload = PLC_ARR_INIT(SYS_CLOCK_FREQ, true),
  .channel[TIM_CH1] = TIM3_CH1_PB4,
  .center_aligned = false
};

DOUT_t TO5 = { .name = "TO5", .pwm = &to5_pwm, .channel = TIM_CH1, .eeprom = &io_eeprom[0], .save = false };

PWM_t to2_pwm = {
  .reg = TIM14,
  .auto_reload = PLC_ARR_INIT(SYS_CLOCK_FREQ, false),
  .channel[TIM_CH1] = TIM14_CH1_PC12,
  .center_aligned = false
};

DOUT_t TO2 = { .name = "TO2", .pwm = &to2_pwm, .channel = TIM_CH1, .eeprom = &io_eeprom[0], .save = false };

void TO2_Frequency(float frequency)
{
  PWM_Frequency(&to2_pwm, frequency);
};

PWM_t to3_pwm = {
  .reg = TIM17,
  .auto_reload = PLC_ARR_INIT(SYS_CLOCK_FREQ, false),
  .channel[TIM_CH1] = TIM17_CH1_PB9,
  .center_aligned = false
};

DOUT_t TO3 = { .name = "TO3", .pwm = &to3_pwm, .channel = TIM_CH1, .eeprom = &io_eeprom[0], .save = false };

void TO3_Frequency(float frequency)
{
  PWM_Frequency(&to2_pwm, frequency);
};

PWM_t to6_pwm = {
  .reg = TIM16,
  .auto_reload = PLC_ARR_INIT(SYS_CLOCK_FREQ, false),
  .channel[TIM_CH1] = TIM16_CH1_PB8,
  .center_aligned = false
};

DOUT_t TO6 = { .name = "TO6", .pwm = &to6_pwm, .channel = TIM_CH1, .eeprom = &io_eeprom[0], .save = false };

void TO6_Frequency(float frequency)
{
  PWM_Frequency(&to6_pwm, frequency);
};

PWM_t to7a_pwm = {
  .reg = TIM15,
  .auto_reload = PLC_ARR_INIT(SYS_CLOCK_FREQ, false),
  .channel[TIM_CH1] = TIM15_CH1_PB14,
  .channel[TIM_CH2] = TIM15_CH2_PB15,
  .center_aligned = false
};

DOUT_t TO7 = { .name = "TO7", .pwm = &to7a_pwm, .channel = TIM_CH1, .eeprom = &io_eeprom[1], .save = false };
DOUT_t TO10 = { .name = "TO10", .pwm = &to7a_pwm, .channel = TIM_CH2, .eeprom = &io_eeprom[1], .save = false };

void TO7A_Frequency(float frequency)
{
  PWM_Frequency(&to7a_pwm, frequency);
};

DOUT_t TO8 = { .name = "TO8", .gpio = { .port = GPIOB, .pin = 13 }, .eeprom = &io_eeprom[1], .save = false };
DOUT_t TO11 = { .name = "TO11", .gpio = { .port = GPIOC, .pin = 6 }, .eeprom = &io_eeprom[1], .save = false };

void TO_Frequency(float frequency)
{
  TO149C_Frequency(frequency);
  TO2_Frequency(frequency);
  TO3_Frequency(frequency);
  TO6_Frequency(frequency);
  TO7A_Frequency(frequency);
}

//------------------------------------------------------------------------------------------------- DIN

// EXTI_t din_trig3, din_trig4;

// PWMI_t din_pwmi3 = {
//   .reg = TIM3,
//   .prescaler = 49,
//   .capture_prescaler = PWMI_CapturePrescaler_1,
//   .filter = TIM_Filter_FCLK_N2,
//   #if(!PWMI_AUTO_OVERSAMPLING)
//     .oversampling = 16,
//   #endif
//   .trig3 = &din_trig3,
//   .trig4 = &din_trig4
// };

// PWMI_t din_pwmi2 = {
//   .reg = TIM2,
//   .prescaler = 49,
//   .capture_prescaler = PWMI_CapturePrescaler_1,
//   .filter = TIM_Filter_FCLK_N2,
//   #if(!PWMI_AUTO_OVERSAMPLING)
//     .oversampling = 16,
//   #endif
// };

DIN_t DI12 = { .name = "DI12", .gpif = { .gpio = { .port = GPIOA, .pin = 6, .reverse = true } }, .eeprom = &io_eeprom[2] };
DIN_t DI11 = { .name = "DI11", .gpif = { .gpio = { .port = GPIOA, .pin = 7, .reverse = true } }, .eeprom = &io_eeprom[2] };
DIN_t DI10 = { .name = "DI10", .gpif = { .gpio = { .port = GPIOB, .pin = 0, .reverse = true } }, .eeprom = &io_eeprom[2] };
DIN_t DI9 = { .name = "DI9", .gpif = { .gpio = { .port = GPIOB, .pin = 1, .reverse = true } }, .eeprom = &io_eeprom[2] };

DIN_t DI1 = { .name = "DI1", .gpif = { .gpio = { .port = GPIOA, .pin = 15, .reverse = true } }, .eeprom = &io_eeprom[2] };
DIN_t DI6 = { .name = "DI6", .gpif = { .gpio = { .port = GPIOB, .pin = 3, .reverse = true } }, .eeprom = &io_eeprom[2] };

DIN_t DI2 = { .name = "DI2", .gpif = { .gpio = { .port = GPIOD, .pin = 3, .reverse = true } }, .eeprom = &io_eeprom[2] };
DIN_t DI3 = { .name = "DI3", .gpif = { .gpio = { .port = GPIOD, .pin = 2, .reverse = true } }, .eeprom = &io_eeprom[2] };
DIN_t DI4 = { .name = "DI4", .gpif = { .gpio = { .port = GPIOD, .pin = 1, .reverse = true } }, .eeprom = &io_eeprom[2] };
DIN_t DI5 = { .name = "DI5", .gpif = { .gpio = { .port = GPIOD, .pin = 0, .reverse = true } }, .eeprom = &io_eeprom[2] };
DIN_t DI7 = { .name = "DI7", .gpif = { .gpio = { .port = GPIOC, .pin = 1, .reverse = true } }, .eeprom = &io_eeprom[2] };
DIN_t DI8 = { .name = "DI8", .gpif = { .gpio = { .port = GPIOC, .pin = 2, .reverse = true } }, .eeprom = &io_eeprom[2] };

// bool din_pwmi2_init = false;
// bool din_pwmi3_init = false;

//------------------------------------------------------------------------------------------------- AIN

uint8_t ain_channels[] = {
  ADC_IN_PA0, ADC_IN_PA1, ADC_IN_PA2, ADC_IN_PA3, ADC_IN_PA5, ADC_IN_PC4, ADC_IN_PC5, ADC_IN_PB2
};

#define AIN_BUFFER_SIZE adc_record_buffer_size(AIN_AVERAGE_TIME_MS, 160, 64, sizeof(ain_channels))
#define AIN_SAMPLES (AIN_BUFFER_SIZE / sizeof(ain_channels))

uint16_t ain_buffer[AIN_BUFFER_SIZE];
uint16_t ain_data[sizeof(ain_channels)][AIN_SAMPLES];

ADC_t ain_adc = {
  .frequency = ADC_Frequency_16MHz,
  .int_prioryty = INT_Prioryty_Low,
  .record = {
    .channels = ain_channels,
    .count = sizeof(ain_channels),
    .dma_nbr = DMA_Nbr_1,
    .sampling_time = ADC_SamplingTime_160,
    .oversampling_enable = true,
    .oversampling_ratio = ADC_OversamplingRatio_64,
    .oversampling_shift = 2,
    .buffer = ain_buffer,
    .buffer_length = AIN_BUFFER_SIZE
  }
};

AIN_t AI1 = { .name = "AI1", .data = ain_data[2], .count = AIN_SAMPLES };
AIN_t AI2 = { .name = "AI2", .data = ain_data[3], .count = AIN_SAMPLES };
AIN_t AI3 = { .name = "AI3", .data = ain_data[5], .count = AIN_SAMPLES };
AIN_t AI4 = { .name = "AI4", .data = ain_data[6], .count = AIN_SAMPLES };
AIN_t POT1 = { .name = "POT1", .data = ain_data[0], .count = AIN_SAMPLES };
AIN_t POT2 = { .name = "POT2", .data = ain_data[1], .count = AIN_SAMPLES };
AIN_t POT3 = { .name = "POT3", .data = ain_data[7], .count = AIN_SAMPLES };
AIN_t VCC = { .name = "VCC", .data = ain_data[4], .count = AIN_SAMPLES };

float VCC_Value(void)
{
  uint16_t raw = ain_adc.measurements.output[4];
  float value = resistor_divider_factor(3.3, 110, 10, 16) * raw;
  return value;
}

//------------------------------------------------------------------------------------------------- RS485

uint8_t rs_buff_buffer[RS_BUFFER_SIZE];
BUFF_t rs_buff = { .mem = rs_buff_buffer, .size = RS_BUFFER_SIZE };
GPIO_t rs_gpio_direction = { .port = GPIOB, .pin = 5, .speed = GPIO_Speed_VeryHigh };
UART_t RS = {
  .reg = USART1,
  .tx_pin = UART1_TX_PB6,
  .rx_pin = UART1_RX_PB7,
  .dma_nbr = DMA_Nbr_5,
  .UART_9600,
  .buff = &rs_buff,
  .gpio_direction = &rs_gpio_direction,
  .timeout = 40
};

//------------------------------------------------------------------------------------------------- RGB+BTN

GPIO_t rgb_gpio_red = { .port = GPIOC, .pin = 7 };
GPIO_t rgb_gpio_green = { .port = GPIOA, .pin = 11 };
GPIO_t rgb_gpio_blue = { .port = GPIOA, .pin = 12 };

RGB_t RGB = { .red = &rgb_gpio_red, .green = &rgb_gpio_green, .blue = &rgb_gpio_blue };

//------------------------------------------------------------------------------------------------- DBG+Bash

UART_t dbg_uart = {
  .reg = USART2,
  .tx_pin = UART2_TX_PD5,
  .rx_pin = UART2_RX_PD6,
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
  RTC_Init();
  EEPROM_Cache(&cache_eeprom);
  RGB_Init(&RGB);
  DBG_Init(&dbg_uart);
  BASH_AddFile(&cache_file);
  BASH_AddCallback(&LED_Bash, "led");
  // Wyjścia cyfrowe tranzystorowe (TO)
  DOUT_Init(&TO1);
  DOUT_Init(&TO2);
  DOUT_Init(&TO3);
  DOUT_Init(&TO4);
  DOUT_Init(&TO5);
  DOUT_Init(&TO6);
  DOUT_Init(&TO7);
  DOUT_Init(&TO8);
  DOUT_Init(&TO9);
  DOUT_Init(&TO10);
  DOUT_Init(&TO11);
  DOUT_Init(&TO12);
  PWM_Init(&to149c_pwm);
  PWM_Init(&to2_pwm);
  PWM_Init(&to3_pwm);
  PWM_Init(&to5_pwm);
  PWM_Init(&to6_pwm);
  PWM_Init(&to7a_pwm);
  // Wejścia cyfrowe (DI)
  // DIN_Init(&DI1);
  // DIN_Init(&DI2);
  // DIN_Init(&DI3);
  // DIN_Init(&DI4);
  // DIN_Init(&DI5);
  // DIN_Init(&DI6);
  // DIN_Init(&DI7);
  // DIN_Init(&DI8);
  // DIN_Init(&DI9);
  // DIN_Init(&DI10);
  // DIN_Init(&DI11);
  // DIN_Init(&DI12);
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
  // Dioda LED i przycisk (BTN)
  RGB_Loop(&RGB);
  // Wyjścia cyfrowe tranzystorowe (TO)
  DOUT_Loop(&TO1);
  DOUT_Loop(&TO2);
  DOUT_Loop(&TO3);
  DOUT_Loop(&TO4);
  DOUT_Loop(&TO5);
  DOUT_Loop(&TO6);
  DOUT_Loop(&TO7);
  DOUT_Loop(&TO8);
  DOUT_Loop(&TO9);
  DOUT_Loop(&TO10);
  DOUT_Loop(&TO11);
  DOUT_Loop(&TO12);
  // Wejścia cyfrowe (DI)
  // DIN_Loop(&DI1);
  // DIN_Loop(&DI2);
  // DIN_Loop(&DI3);
  // DIN_Loop(&DI4);
  // DIN_Loop(&DI5);
  // DIN_Loop(&DI6);
  // DIN_Loop(&DI7);
  // DIN_Loop(&DI8);
  // DIN_Loop(&DI9);
  // DIN_Loop(&DI10);
  // DIN_Loop(&DI11);
  // DIN_Loop(&DI12);
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
