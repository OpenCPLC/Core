// .tests/uno/main.c

#include <opencplc.h>

void loop(void)
{
  while(1) {
    RGB_Set(&RGB, RGB_Red);
    delay(1000);
    RGB_Set(&RGB, RGB_Green);
    delay(1000);
    RGB_Set(&RGB, RGB_Blue);
    delay(1000);
    RGB_Rst(&RGB);
    delay(1000);
  }
}

stack(stack_plc, 256);
stack(stack_dbg, 256);
stack(stack_loop, 1024);

int main(void)
{
  thread(PLC_Main, stack_plc);
  thread(DBG_Loop, stack_dbg);
  thread(loop, stack_loop);
  vrts_init();
  while(1); // `vrts_init` never returns
}
