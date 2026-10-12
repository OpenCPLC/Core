// hal/host/per/pdb.h

#ifndef HOST_PDB_H_
#define HOST_PDB_H_

// The chip's PDB over the flash model. The model's `flash.h` and `crc.h` come first,
// so the chip header finds them included under the guards they share.
#include "flash.h"
#include "crc.h"
#include "../../stm32/per/pdb.h"

#endif
