// plc/brd/opencplc.h

#ifndef OPENCPLC_H_
#define OPENCPLC_H_

#include "main.h"

#ifdef PRO_BOARD_UNO
  #include "opencplc_uno.h"
#endif
#ifdef PRO_BOARD_ECO
  #include "opencplc_eco.h"
#endif

#ifdef PRO_BOARD_DIO
  #include "opencplc_dio.h"
#endif

#endif