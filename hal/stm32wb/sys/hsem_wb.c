// hal/stm32wb/sys/hsem_wb.c

#include "hsem_wb.h"
#include "vrts.h"

//-------------------------------------------------------------------------------------------------

// One-step lock: reading `RLR` attempts the lock for this core with process `0`
// and echoes the register, so the comparison tells whether it worked
#define HSEM_TAKEN (HSEM_R_LOCK | HSEM_CR_COREID_CPU1)

bool HSEM_Take(uint8_t id)
{
  RCC->AHB3ENR |= RCC_AHB3ENR_HSEMEN;
  return HSEM->RLR[id] == HSEM_TAKEN;
}

void HSEM_Wait(uint8_t id)
{
  // The wait lasts as long as the other core's own critical section, so the rest of the
  // system keeps running through it, the watchdog included. Before the scheduler starts
  // `let` returns at once and this is the plain spin it always was.
  while(!HSEM_Take(id)) let();
}

void HSEM_Give(uint8_t id)
{
  HSEM->R[id] = HSEM_CR_COREID_CPU1;
}

//-------------------------------------------------------------------------------------------------
