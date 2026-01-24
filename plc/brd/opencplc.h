#ifndef OPENCPLC_H_
#define OPENCPLC_H_

#include "main.h"

#ifdef PRO_BOARD_UNO
  #include "opencplc-uno.h"
#endif
#ifdef PRO_BOARD_ECO
  #include "opencplc-eco.h"
#endif

#ifdef PRO_BOARD_DIO
  #include "opencplc-dio.h"
#endif

#endif