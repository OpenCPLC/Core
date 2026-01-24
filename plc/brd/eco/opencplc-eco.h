#ifndef OPENCPLC_ECO_H_
#define OPENCPLC_ECO_H_

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
  #define PLC_GREETING "OpenCPLC Eco"
#endif

#ifndef PLC_BOOTLOADER
  #define PLC_BOOTLOADER 0
#endif

#ifndef PLC_BASETIME
  #define PLC_BASETIME 10
#endif

// Wyjścia cyfrowe przekaźnikowse (RO)
extern DOUT_t RO1;
extern DOUT_t RO2;

// Wyjścia cyfrowe tranzystorowe (TO)
extern DOUT_t TO1;
extern DOUT_t TO2;
extern DOUT_t TO3;
extern DOUT_t TO4;
extern DOUT_t TO5;
void TO1_Frequency(float frequency);
void TO2_Frequency(float frequency);
void TO345_Frequency(float frequency);

// Wejścia cyfrowe (DI)
extern DIN_t DI1;
extern DIN_t DI2;
extern DIN_t DI3;
extern DIN_t DI4;

// Wejścia analogowe (AI)
extern AIN_t AI1;
extern AIN_t AI2;
extern AIN_t AI3;
extern AIN_t AI4;
extern AIN_t POT1;
extern AIN_t POT2;
extern AIN_t POT3;
extern AIN_t POT4;
extern AIN_t POT5;
extern AIN_t POT6;
float VCC_Voltage_V(void);

// RS485
#ifndef RS_BUFFER_SIZE
  #define RS_BUFFER_SIZE 1000
#endif
extern UART_t RS;

// Dioda RGB
extern RGB_t RGB;
#define RGB1 RGB
extern DIN_t BTN1;
extern DIN_t BTN2;
extern DIN_t BTN3;
extern DIN_t SW1;
extern DIN_t SW2;

// Functions
void PLC_Init(void);
void PLC_Loop(void);
void PLC_Main(void);

//-------------------------------------------------------------------------------------------------
#endif