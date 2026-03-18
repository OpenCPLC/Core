// hal/stm32wb/sys/irq_wb.h

#ifndef IRQ_WB_H_
#define IRQ_WB_H_

#include "stm32wbxx.h"

//-------------------------------------------------------------------------------------------------

// 16 priorities: 4 preempt × 4 sub (M4 with priority grouping)
// Compatible with M0+ which uses only VeryHigh/High/Medium/Low (0/4/8/12)
typedef enum {
  IRQ_Priority_VeryHigh   = 0,
  IRQ_Priority_VeryHigh_1 = 1,
  IRQ_Priority_VeryHigh_2 = 2,
  IRQ_Priority_VeryHigh_3 = 3,
  IRQ_Priority_High       = 4,
  IRQ_Priority_High_1     = 5,
  IRQ_Priority_High_2     = 6,
  IRQ_Priority_High_3     = 7,
  IRQ_Priority_Medium     = 8,
  IRQ_Priority_Medium_1   = 9,
  IRQ_Priority_Medium_2   = 10,
  IRQ_Priority_Medium_3   = 11,
  IRQ_Priority_Low        = 12,
  IRQ_Priority_Low_1      = 13,
  IRQ_Priority_Low_2      = 14,
  IRQ_Priority_Low_3      = 15
} IRQ_Priority_t;

//-------------------------------------------------------------------------------------------------

void IRQ_Init(void);
void IRQ_SetPriority(IRQn_Type irq, IRQ_Priority_t priority);

//-------------------------------------------------------------------------------------------------

#endif