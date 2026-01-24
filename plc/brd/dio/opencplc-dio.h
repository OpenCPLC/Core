#ifndef OPENCPLC_DIO_H_
#define OPENCPLC_DIO_H_

#include "dout.h"
#include "din.h"
#include "ain.h"
#include "dbg.h"
#include "bash.h"
#include "rgb.h"
#include "vrts.h"
#include "sys.h"
#include "main.h"

#define PLC_RS485

#ifndef PLC_GREETING
  #define PLC_GREETING "OpenCPLC Dio"
#endif

#ifndef PLC_BOOTLOADER
  #define PLC_BOOTLOADER 0
#endif

#ifndef PLC_BASETIME
  #define PLC_BASETIME 1
#endif

#ifndef PLC_ARR_INIT
  #define PLC_ARR_INIT(clock, center_aligned) TIM_ARR_INIT(1000, clock, center_aligned)
#endif

// Wyjścia cyfrowe tranzystorowe (TO)
extern DOUT_t TO1, TO2, TO3, TO4, TO5, TO6;
extern DOUT_t TO7, TO8, TO9, TO10, TO11, TO12;

void TO149C_Frequency(float frequency);
void TO2_Frequency(float frequency);
void TO3_Frequency(float frequency);
void TO6_Frequency(float frequency);
void TO7A_Frequency(float frequency);
void TO_Frequency(float frequency);

// Wejścia cyfrowe (DI)
extern DIN_t DI1, DI2, DI3, DI4, DI5, DI6;
extern DIN_t DI7, DI8, DI9, DI10, DI11, DI12;

// Wejścia analogowe (AI)
extern AIN_t AI1;
extern AIN_t AI2;
extern AIN_t AI3;
extern AIN_t AI4;
float VCC_Value(void);

// RS485
#ifndef RS_BUFFER_SIZE
  #define RS_BUFFER_SIZE 1000
#endif
extern UART_t RS;
#define RS1 RS

// Dioda RGB
extern RGB_t RGB;
#define RGB1 RGB

// Functions
void PLC_Init(void);
void PLC_Loop(void);
void PLC_Main(void);

//-------------------------------------------------------------------------------------------------
#endif
