#ifndef __STM32WBxx_H
#define __STM32WBxx_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined (STM32WB)
#define STM32WB
#endif

#define __STM32WBxx_CMSIS_VERSION_MAIN   (0x01U) // [31:24] main version
#define __STM32WBxx_CMSIS_VERSION_SUB1   (0x0CU) // [23:16] sub1 version
#define __STM32WBxx_CMSIS_VERSION_SUB2   (0x02U) // [15:8]  sub2 version
#define __STM32WBxx_CMSIS_VERSION_RC     (0x00U) // [7:0]  release candidate
#define __STM32WBxx_CMSIS_DEVICE_VERSION        ((__STM32WBxx_CMSIS_VERSION_MAIN << 24)\
                                                |(__STM32WBxx_CMSIS_VERSION_SUB1 << 16)\
                                                |(__STM32WBxx_CMSIS_VERSION_SUB2 << 8 )\
                                                |(__STM32WBxx_CMSIS_VERSION_RC))

#if defined(STM32WB55xx)
#include "stm32wb55xx.h"
#elif defined(STM32WB5Mxx)
#include "stm32wb5mxx.h"
#elif defined(STM32WB50xx)
#include "stm32wb50xx.h"
#elif defined(STM32WB35xx)
#include "stm32wb35xx.h"
#elif defined(STM32WB30xx)
#include "stm32wb30xx.h"
#elif defined(STM32WB15xx)
#include "stm32wb15xx.h"
#elif defined(STM32WB10xx)
#include "stm32wb10xx.h"
#elif defined(STM32WB1Mxx)
#include "stm32wb1mxx.h"
#else
#error "Please select first the target STM32WBxx device used in your application, for instance xxx (in stm32wbxx.h file)"
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

/* Use of CMSIS compiler intrinsics for register exclusive access */
/* Atomic 32-bit register access macro to set one or several bits */
#define ATOMIC_SET_BIT(REG, BIT)                             \
  do {                                                       \
    uint32_t val;                                            \
    do {                                                     \
      val = __LDREXW((__IO uint32_t *)&(REG)) | (BIT);       \
    } while ((__STREXW(val,(__IO uint32_t *)&(REG))) != 0U); \
  } while(0)

/* Atomic 32-bit register access macro to clear one or several bits */
#define ATOMIC_CLEAR_BIT(REG, BIT)                           \
  do {                                                       \
    uint32_t val;                                            \
    do {                                                     \
      val = __LDREXW((__IO uint32_t *)&(REG)) & ~(BIT);      \
    } while ((__STREXW(val,(__IO uint32_t *)&(REG))) != 0U); \
  } while(0)

/* Atomic 32-bit register access macro to clear and set one or several bits */
#define ATOMIC_MODIFY_REG(REG, CLEARMSK, SETMASK)                          \
  do {                                                                     \
    uint32_t val;                                                          \
    do {                                                                   \
      val = (__LDREXW((__IO uint32_t *)&(REG)) & ~(CLEARMSK)) | (SETMASK); \
    } while ((__STREXW(val,(__IO uint32_t *)&(REG))) != 0U);               \
  } while(0)

/* Atomic 16-bit register access macro to set one or several bits */
#define ATOMIC_SETH_BIT(REG, BIT)                            \
  do {                                                       \
    uint16_t val;                                            \
    do {                                                     \
      val = __LDREXH((__IO uint16_t *)&(REG)) | (BIT);       \
    } while ((__STREXH(val,(__IO uint16_t *)&(REG))) != 0U); \
  } while(0)

/* Atomic 16-bit register access macro to clear one or several bits */
#define ATOMIC_CLEARH_BIT(REG, BIT)                          \
  do {                                                       \
    uint16_t val;                                            \
    do {                                                     \
      val = __LDREXH((__IO uint16_t *)&(REG)) & ~(BIT);      \
    } while ((__STREXH(val,(__IO uint16_t *)&(REG))) != 0U); \
  } while(0)

/* Atomic 16-bit register access macro to clear and set one or several bits */
#define ATOMIC_MODIFYH_REG(REG, CLEARMSK, SETMASK)                         \
  do {                                                                     \
    uint16_t val;                                                          \
    do {                                                                   \
      val = (__LDREXH((__IO uint16_t *)&(REG)) & ~(CLEARMSK)) | (SETMASK); \
    } while ((__STREXH(val,(__IO uint16_t *)&(REG))) != 0U);               \
  } while(0)

#define POSITION_VAL(VAL)     (__CLZ(__RBIT(VAL)))

#ifdef __cplusplus
}
#endif
#endif
