#ifndef STM32G0xx_H
#define STM32G0xx_H

#if !defined (STM32G0)
#define STM32G0
#endif

//-------------------------------------------------------------------------------------------------

#if !defined (STM32G071xx) && !defined (STM32G081xx) && !defined (STM32G070xx) \
 && !defined (STM32G030xx) && !defined (STM32G031xx) && !defined (STM32G041xx) \
 && !defined (STM32G0B0xx) && !defined (STM32G0B1xx) && !defined (STM32G0C1xx) \
 && !defined (STM32G050xx) && !defined (STM32G051xx) && !defined (STM32G061xx)
  /* #define STM32G0B0xx */   /*!< STM32G0B0xx Devices */
  /* #define STM32G0B1xx */   /*!< STM32G0B1xx Devices */
  /* #define STM32G0C1xx */   /*!< STM32G0C1xx Devices */
  /* #define STM32G070xx */   /*!< STM32G070xx Devices */
  /* #define STM32G071xx */   /*!< STM32G071xx Devices */
  /* #define STM32G081xx */   /*!< STM32G081xx Devices */
  /* #define STM32G050xx */   /*!< STM32G050xx Devices */
  /* #define STM32G051xx */   /*!< STM32G051xx Devices */
  /* #define STM32G061xx */   /*!< STM32G061xx Devices */
  /* #define STM32G030xx */   /*!< STM32G030xx Devices */
  /* #define STM32G031xx */   /*!< STM32G031xx Devices */
  /* #define STM32G041xx */   /*!< STM32G041xx Devices */
#endif

// CMSIS Device version number
#define __STM32G0_CMSIS_VERSION_MAIN   (0x01U) // [31:24] main version
#define __STM32G0_CMSIS_VERSION_SUB1   (0x04U) // [23:16] sub1 version
#define __STM32G0_CMSIS_VERSION_SUB2   (0x04U) // [15:8]  sub2 version
#define __STM32G0_CMSIS_VERSION_RC     (0x00U) // [7:0]  release candidate
#define __STM32G0_CMSIS_VERSION \
  ((__STM32G0_CMSIS_VERSION_MAIN << 24) \
  |(__STM32G0_CMSIS_VERSION_SUB1 << 16) \
  |(__STM32G0_CMSIS_VERSION_SUB2 << 8 ) \
  |(__STM32G0_CMSIS_VERSION_RC))

#if defined(STM32G0B1xx)
  #include "stm32g0b1xx.h"
#elif defined(STM32G0C1xx)
  #include "stm32g0c1xx.h"
#elif defined(STM32G0B0xx)
  #include "stm32g0b0xx.h"
#elif defined(STM32G071xx)
  #include "stm32g071xx.h"
#elif defined(STM32G081xx)
  #include "stm32g081xx.h"
#elif defined(STM32G070xx)
  #include "stm32g070xx.h"
#elif defined(STM32G031xx)
  #include "stm32g031xx.h"
#elif defined(STM32G041xx)
  #include "stm32g041xx.h"
#elif defined(STM32G030xx)
  #include "stm32g030xx.h"
#elif defined(STM32G051xx)
  #include "stm32g051xx.h"
#elif defined(STM32G061xx)
  #include "stm32g061xx.h"
#elif defined(STM32G050xx)
  #include "stm32g050xx.h"
#else
  #error "Select define for STM32G0xx (STM32G0C1xx or STM32G081xx)"
#endif

// Exported macros
#define SET_BIT(REG, BIT)   ((REG) |= (BIT))
#define CLEAR_BIT(REG, BIT) ((REG) &= ~(BIT))
#define READ_BIT(REG, BIT)  ((REG) & (BIT))
#define CLEAR_REG(REG)      ((REG) = (0x0))
#define WRITE_REG(REG, VAL) ((REG) = (VAL))
#define READ_REG(REG)       ((REG))
#define MODIFY_REG(REG, CLEARMASK, SETMASK) \
  WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))

// Use of interrupt control for register exclusive access
// Atomic 32-bit register access macro to set one or several bits
#define ATOMIC_SET_BIT(REG, BIT)  \
  do {                            \
    uint32_t primask;             \
    primask = __get_PRIMASK();    \
    __set_PRIMASK(1);             \
    SET_BIT((REG), (BIT));        \
    __set_PRIMASK(primask);       \
  } while(0)

// Atomic 32-bit register access macro to clear one or several bits
#define ATOMIC_CLEAR_BIT(REG, BIT)  \
  do {                              \
    uint32_t primask;               \
    primask = __get_PRIMASK();      \
    __set_PRIMASK(1);               \
    CLEAR_BIT((REG), (BIT));        \
    __set_PRIMASK(primask);         \
  } while(0)

// Atomic 32-bit register access macro to clear and set one or several bits
#define ATOMIC_MODIFY_REG(REG, CLEARMSK, SETMASK)  \
  do {                                             \
    uint32_t primask;                              \
    primask = __get_PRIMASK();                     \
    __set_PRIMASK(1);                              \
    MODIFY_REG((REG), (CLEARMSK), (SETMASK));      \
    __set_PRIMASK(primask);                        \
  } while(0)

// Atomic 16-bit register access macro to set one or several bits
#define ATOMIC_SETH_BIT(REG, BIT) ATOMIC_SET_BIT(REG, BIT)
// Atomic 16-bit register access macro to clear one or several bits
#define ATOMIC_CLEARH_BIT(REG, BIT) ATOMIC_CLEAR_BIT(REG, BIT)
// Atomic 16-bit register access macro to clear and set one or several bits
#define ATOMIC_MODIFYH_REG(REG, CLEARMSK, SETMASK) ATOMIC_MODIFY_REG(REG, CLEARMSK, SETMASK)

//-------------------------------------------------------------------------------------------------
#endif