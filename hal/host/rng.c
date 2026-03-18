// hal/host/rng.c

#include "rng.h"
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #include <bcrypt.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "bcrypt.lib")
  #endif
  static bool rng_bcrypt_available = false;
#else
  #include <fcntl.h>
  #include <unistd.h>
  static int rng_urandom_fd = -1;
#endif

static bool rng_initialized = false;

//------------------------------------------------------------------------------------------------- Internal

static uint32_t rng_fallback(void)
{
  // xorshift32 fallback if OS entropy fails
  static uint32_t state = 0;
  if(state == 0) state = (uint32_t)time(NULL) ^ 0xDEADBEEF;
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

#if defined(_WIN32) || defined(_WIN64)
static bool rng_bcrypt_gen(uint8_t *buf, uint16_t len)
{
  return BCRYPT_SUCCESS(BCryptGenRandom(NULL, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}
#else
static bool rng_urandom_gen(uint8_t *buf, uint16_t len)
{
  if(rng_urandom_fd < 0) return false;
  ssize_t total = 0;
  while(total < len) {
    ssize_t n = read(rng_urandom_fd, buf + total, len - total);
    if(n <= 0) return false;
    total += n;
  }
  return true;
}
#endif

//-------------------------------------------------------------------------------------------------

void RNG_Init(RNG_Source_t source, RNG_Divider_t div)
{
  (void)source; (void)div;
  if(rng_initialized) return;
  #if defined(_WIN32) || defined(_WIN64)
    // test if BCrypt works
    uint8_t test[4];
    rng_bcrypt_available = rng_bcrypt_gen(test, sizeof(test));
    if(!rng_bcrypt_available) {
      srand((unsigned int)time(NULL) ^ GetTickCount());
    }
  #else
    rng_urandom_fd = open("/dev/urandom", O_RDONLY);
    if(rng_urandom_fd < 0) {
      srand((unsigned int)time(NULL));
    }
  #endif
  rng_initialized = true;
}

uint32_t RNG_Run(void)
{
  if(!rng_initialized) RNG_Init(RNG_Source_Void, RNG_Divider_1);
  uint32_t value;
  #if defined(_WIN32) || defined(_WIN64)
    if(rng_bcrypt_available) {
      if(rng_bcrypt_gen((uint8_t *)&value, sizeof(value))) return value;
    }
  #else
    if(rng_urandom_gen((uint8_t *)&value, sizeof(value))) return value;
  #endif
  return rng_fallback();
}

int32_t rng(int32_t min, int32_t max)
{
  if(min >= max) return min;
  uint32_t range = (uint32_t)(max - min);
  return (int32_t)(RNG_Run() % range) + min;
}

void RNG_Fill(uint8_t *buf, uint16_t len)
{
  if(!rng_initialized) RNG_Init(RNG_Source_Void, RNG_Divider_1);
  #if defined(_WIN32) || defined(_WIN64)
    if(rng_bcrypt_available && rng_bcrypt_gen(buf, len)) return;
  #else
    if(rng_urandom_gen(buf, len)) return;
  #endif
  // fallback
  for(uint16_t i = 0; i < len; i++) {
    buf[i] = (uint8_t)rng_fallback();
  }
}

//-------------------------------------------------------------------------------------------------