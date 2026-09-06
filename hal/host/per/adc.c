// hal/host/per/adc.c

#include "adc.h"

#include "xdef.h"

// Sampling is an act of hardware. Every entry point is accepted and returns a neutral
// value: a host suite feeds converted values to the code under test directly

ADC_TypeDef HostAdc;

const uint16_t ADC_PRESCALER_TAB[] = { 1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256 };
const uint16_t ADC_SAMPLING_TIME_TAB[] = { 14, 16, 20, 25, 32, 52, 92, 173 };
const uint16_t ADC_OVERSAMPLING_RATIO_TAB[] = { 2, 4, 8, 16, 32, 64, 128, 256 };

//--------------------------------------------------------------------------------------------- API

void ADC_Init(ADC_t *adc)
{
  unused(adc);
}

status_t ADC_Measure(ADC_t *adc)
{
  unused(adc);
  return OK;
}

uint16_t ADC_Read(ADC_t *adc, uint8_t chan)
{
  unused(adc);
  unused(chan);
  return 0;
}

uint32_t ADC_Frequency_Hz(ADC_t *adc)
{
  return 16000000u / ADC_PRESCALER_TAB[adc->prescaler];
}

uint16_t ADC_Vdda_mV(ADC_t *adc)
{
  unused(adc);
  return 3300;
}

float ADC_Temperature_C(ADC_t *adc)
{
  unused(adc);
  return 0.0f;
}

status_t ADC_Record(ADC_t *adc)
{
  unused(adc);
  return OK;
}

status_t ADC_LastSamples(ADC_t *adc, uint16_t *buffer, uint16_t count, bool sort)
{
  unused(adc);
  unused(buffer);
  unused(count);
  unused(sort);
  return OK;
}

uint16_t ADC_RecordPosition(ADC_t *adc)
{
  unused(adc);
  return 0;
}

float ADC_RecordScanTime_s(ADC_t *adc)
{
  unused(adc);
  return 0.0f;
}

uint16_t ADC_Overruns(ADC_t *adc)
{
  unused(adc);
  return 0;
}

void ADC_Stop(ADC_t *adc)
{
  unused(adc);
}

bool ADC_IsBusy(ADC_t *adc)
{
  unused(adc);
  return false;
}

bool ADC_IsFree(ADC_t *adc)
{
  unused(adc);
  return true;
}

void ADC_Wait(ADC_t *adc)
{
  unused(adc);
}

void ADC_Enable(ADC_t *adc)
{
  unused(adc);
}

void ADC_Disable(ADC_t *adc)
{
  unused(adc);
}

void ADC_InitGPIO(ADC_t *adc, uint8_t *chan, uint8_t count)
{
  unused(adc);
  unused(chan);
  unused(count);
}

//-------------------------------------------------------------------------------------------------
