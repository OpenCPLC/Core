// plc/brd/uno/opencplc_uno.h

#ifndef OPENCPLC_UNO_H_
#define OPENCPLC_UNO_H_

#include "dout.h"
#include "din.h"
#include "ain.h"
#include "max31865.h"
#include "log.h"
#include "cmd.h"
#include "rgb.h"
#include "one_wire.h"
#include "twi.h"
#include "vrts.h"
#include "sys.h"
#include "main.h"

//---------------------------------------------------------------------------------------- Features

#define PLC_RS485
#define PLC_I2C
#define PLC_ONE_WIRE

//------------------------------------------------------------------------------------------ Config

#ifndef PLC_GREETING
  #define PLC_GREETING "OpenCPLC Uno"
#endif

#ifndef PLC_BASETIME
  // System tick [ms]
  #define PLC_BASETIME 1
#endif

#ifndef PLC_BOR_LEVEL
  // Brown-out reset level, `2V8` for 3V3 automation; `BOR_Level_1V7` = power-down only
  #define PLC_BOR_LEVEL BOR_Level_2V8
#endif

#ifndef RS_BUFFER_SIZE
  // Receive buffer of each RS485 port [bytes]
  #define RS_BUFFER_SIZE 1000
#endif

//--------------------------------------------------------------------------------------------- I/O

// Relay outputs (RO)
extern DOUT_t RO1;
extern DOUT_t RO2;
extern DOUT_t RO3;
extern DOUT_t RO4;

// Transistor outputs (TO), one PWM timer
extern DOUT_t TO1;
extern DOUT_t TO2;
extern DOUT_t TO3;
extern DOUT_t TO4;
void TO_Frequency(float frequency);

// Triac outputs (XO), one PWM timer
extern DOUT_t XO1;
extern DOUT_t XO2;
void XO_Frequency(float frequency);

// Digital inputs (DI)
extern DIN_t DI1;
extern DIN_t DI2;
extern DIN_t DI3;
extern DIN_t DI4;

// Analog inputs (AI)
extern AIN_t AI1;
extern AIN_t AI2;
extern AIN_t POT;
#define POT1 POT
float VCC_Voltage_V(void);

// RS485 ports
extern UART_t RS1;
extern UART_t RS2;

// RGB LED and the BTN button
extern RGB_t RGB;
#define RGB1 RGB
extern DIN_t BTN;
#define BTN1 BTN

// I2C bus, ready for external drivers
extern I2C_Master_t i2c_master;

// PT100/PT1000 front end
extern MAX31865_t RTD;
#define RTD1 RTD

//--------------------------------------------------------------------------------------------- API

// Board bring-up, the main loop, and both in one call that never returns
void PLC_Init(void);
void PLC_Loop(void);
void PLC_Main(void);

// RTD thread: bus and front end, then measurements forever
void RTD_Main(void);

//-------------------------------------------------------------------------------------------------
#endif
