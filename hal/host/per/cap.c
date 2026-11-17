// hal/host/per/cap.c

#include "cap.h"

#include "xdef.h"

// No pin is modelled behind a capture off-target: no edge is ever captured,
// and the level reads low

void CAP_Init(CAP_t *cap) { unused(cap); }

bool CAP_Level(CAP_t *cap) { unused(cap); return false; }
