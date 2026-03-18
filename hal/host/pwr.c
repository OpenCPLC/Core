// hal/host/pwr.c

#include "pwr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #include <process.h>
#else
  #include <unistd.h>
#endif

//------------------------------------------------------------------------------------------------- Program path (for restart)

static char pwr_exe_path[1024] = {0};
static char **pwr_argv = NULL;

/**
 * @brief Store program path and arguments for restart
 * @note Call this from main() if you want PWR_Reset() to work
 */
void PWR_StoreArgs(int argc, char **argv)
{
  if(argc > 0 && argv && argv[0]) {
    #if defined(_WIN32) || defined(_WIN64)
      GetModuleFileNameA(NULL, pwr_exe_path, sizeof(pwr_exe_path));
    #else
      // Try /proc/self/exe first (Linux)
      ssize_t len = readlink("/proc/self/exe", pwr_exe_path, sizeof(pwr_exe_path) - 1);
      if(len > 0) {
        pwr_exe_path[len] = '\0';
      }
      else {
        strncpy(pwr_exe_path, argv[0], sizeof(pwr_exe_path) - 1);
      }
    #endif
    pwr_argv = argv;
  }
}

//------------------------------------------------------------------------------------------------- PWR

void PWR_Reset(void)
{
  printf("[PWR] Reset (program termination)\n");
  fflush(stdout);
  exit(0);
}

void PWR_Sleep(PWR_SleepMode_t mode)
{
  const char *mode_names[] = {
    "Stop0", "Stop1", "Stop2", "StandbySRAM", "Standby", "Shutdown", "Error"
  };
  const char *name = (mode <= PWR_SleepMode_Error) ? mode_names[mode] : "Unknown";
  printf("[PWR] Sleep mode: %s (program termination)\n", name);
  fflush(stdout);
  exit(0);
}

//------------------------------------------------------------------------------------------------- BKPR (RAM-based, lost on restart)

static uint32_t bkpr_regs[5] = {0};

void BKPR_Write(BKPR_t reg, uint32_t value)
{
  if(reg <= BKPR_4) {
    bkpr_regs[reg] = value;
  }
}

uint32_t BKPR_Read(BKPR_t reg)
{
  if(reg <= BKPR_4) {
    return bkpr_regs[reg];
  }
  return 0;
}

//-------------------------------------------------------------------------------------------------
