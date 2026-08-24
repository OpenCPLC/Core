// plc/brd/opencplc.h

#ifndef OPENCPLC_H_
#define OPENCPLC_H_

#include "main.h"

#ifdef PRO_BOARD_UNO
  #include "opencplc_uno.h"
#endif

// ECO and DIO have not been retargeted to the current HAL. Sources stay in the tree
#ifdef PRO_BOARD_ECO
  #error "Board ECO targets an older HAL and does not build. Only UNO is supported."
#endif
#ifdef PRO_BOARD_DIO
  #error "Board DIO targets an older HAL and does not build. Only UNO is supported."
#endif

#endif