// hal/host/sys.c

#include "sys.h"
#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
#else
  #include <signal.h>
#endif

static void (*panic_handler)(void);
static volatile int exit_requested = 0;

//-------------------------------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WIN64)

static BOOL WINAPI ctrl_handler(DWORD type)
{
  switch(type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      exit_requested = 1;
      return TRUE;
    default:
      return FALSE;
  }
}

static void ctrlc_disable(void)
{
  SetConsoleCtrlHandler(ctrl_handler, TRUE);
}

#else

static void sig_handler(int sig)
{
  (void)sig;
  exit_requested = 1;
}

static void ctrlc_disable(void)
{
  struct sigaction sa;
  sa.sa_handler = sig_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}

#endif
//-------------------------------------------------------------------------------------------------

void sys_init(void)
{
  heap_init();
  ctrlc_disable();
}

int sys_exit_requested(void)
{
  return exit_requested;
}

void sys_exit_request(void)
{
  exit_requested = 1;
}

//-------------------------------------------------------------------------------------------------

void panic_hook(void (*handler)(void))
{
  panic_handler = handler;
}

void panic(const char *message)
{
  if(panic_handler) panic_handler();
  fprintf(stderr, "[PNC] %s\n", message);
  fflush(stderr);
  exit(1);
}

//-------------------------------------------------------------------------------------------------

bool file_save(const char *name, const uint8_t *data, size_t size)
{
  FILE *fp = fopen(name, "wb");
  if(!fp) return false;
  size_t written = fwrite(data, 1, size, fp);
  fclose(fp);
  return written == size;
}

size_t file_load(const char *name, uint8_t **data)
{
  FILE *fp = fopen(name, "rb");
  if(!fp) {
    *data = NULL;
    return 0;
  }
  fseek(fp, 0, SEEK_END);
  size_t size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  *data = (uint8_t *)malloc(size);
  if(!*data) {
    fclose(fp);
    return 0;
  }
  if(fread(*data, 1, size, fp) != size) {
    free(*data);
    *data = NULL;
    fclose(fp);
    return 0;
  }
  fclose(fp);
  return size;
}

//-------------------------------------------------------------------------------------------------
