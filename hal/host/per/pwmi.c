// hal/host/per/pwmi.c

#include "pwmi.h"
#include "xdef.h"

//-------------------------------------------------------------------------------------------------

/**
 * Capture units do not exist off-target. A pass never completes and the outputs are left
 * untouched: nothing here measures an edge.
 */

void PWMI_Init(PWMI_t *pwmi)
{
  unused(pwmi);
}

bool PWMI_Loop(PWMI_t *pwmi)
{
  unused(pwmi);
  return false;
}
