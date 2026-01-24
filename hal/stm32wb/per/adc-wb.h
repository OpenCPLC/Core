#ifndef ADC_WB_H_
#define ADC_WB_H_

typedef enum {
  ADC_IN_PA0 = 0,
  ADC_IN_PA1 = 1,
  ADC_IN_PA2 = 2,
  ADC_IN_PA3 = 3,
  ADC_IN_PA4 = 4,
  ADC_IN_PA5 = 5,
  ADC_IN_PA6 = 6,
  ADC_IN_PA7 = 7,
  ADC_IN_PB0 = 8,
  ADC_IN_PB1 = 9,
  ADC_IN_PC0 = 10,
  ADC_IN_PC1 = 11,
  ADC_IN_PC2 = 12,
  ADC_IN_PC3 = 13,
  ADC_IN_PC4 = 14,
  ADC_IN_PC5 = 15,
  ADC_IN_VREFEN = 16,
  ADC_IN_TSEN = 17,
  ADC_IN_VBATEN = 18
} ADC_IN_t;

typedef enum {
  ADC_SamplingTime_2 = 0,
  ADC_SamplingTime_6 = 1,
  ADC_SamplingTime_12 = 2,
  ADC_SamplingTime_24 = 3,
  ADC_SamplingTime_47 = 4,
  ADC_SamplingTime_92 = 5,
  ADC_SamplingTime_247 = 6,
  ADC_SamplingTime_640 = 7
} ADC_SamplingTime_t;

#endif
