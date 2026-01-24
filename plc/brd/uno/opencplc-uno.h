#ifndef OPENCPLC_UNO_H_
#define OPENCPLC_UNO_H_

#include "dout.h"
#include "din.h"
#include "ain.h"
#include "max31865.h"
#include "log.h"
#include "bash.h"
#include "rgb.h"
#include "one-wire.h"
#include "twi.h"
#include "vrts.h"
#include "sys.h"
#include "main.h"

#define PLC_RS485
#define PLC_I2C
#define PLC_ONE_WIRE

#ifndef PLC_GREETING
  #define PLC_GREETING "OpenCPLC Uno"
#endif

#ifndef PLC_BOOTLOADER
  #define PLC_BOOTLOADER 0
#endif

#ifndef PLC_BASETIME
  #define PLC_BASETIME 1
#endif

#define PLC_ARR_INIT(clock, center_aligned) (clock / 1000 / (center_aligned + 1))

// Wyjścia cyfrowe przekaźnikowse (RO)
extern DOUT_t RO1;
extern DOUT_t RO2;
extern DOUT_t RO3;
extern DOUT_t RO4;

//  Wyjścia cyfrowe tranzystorowe (TO)
extern DOUT_t TO1;
extern DOUT_t TO2;
extern DOUT_t TO3;
extern DOUT_t TO4;
void TO_Frequency(float frequency);

// Wyjścia cyfrowe triakowe (XO)
extern DOUT_t XO1;
extern DOUT_t XO2;
void XO_Frequency(float frequency);

// Wejścia cyfrowe (DI)
extern DIN_t DI1;
extern DIN_t DI2;
extern DIN_t DI3;
extern DIN_t DI4;

// Wejścia analogowe (AI)
extern AIN_t AI1;
extern AIN_t AI2;
extern AIN_t POT;
#define POT1 POT
float VCC_Voltage_V(void);

// RS485
#ifndef RS_BUFFER_SIZE
  #define RS_BUFFER_SIZE 1000
#endif
extern UART_t RS1;
extern UART_t RS2;

// 1WIRE
// void _1WIRE_Active(void);

// Dioda RGB i przycisk BTN
extern RGB_t RGB;
#define RGB1 RGB
extern DIN_t BTN;
#define BTN1 BTN

// Functions
void PLC_Init(void);
void PLC_Loop(void);
void PLC_Main(void);

// PT100/PT1000
extern MAX31865_t RTD;
#define RTD1 RTD
void RTD_Main(void);

//-------------------------------------------------------------------------------------------------
#endif
