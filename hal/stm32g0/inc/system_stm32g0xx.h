#ifndef SYSTEM_STM32G0XX_H
#define SYSTEM_STM32G0XX_H

extern uint32_t SystemCoreClock; // System Clock Frequency (Core Clock)
extern const uint32_t AHBPrescTable[16]; // AHB prescalers table values
extern const uint32_t APBPrescTable[8]; // APB prescalers table values

extern void SystemInit(void);
extern void SystemCoreClockUpdate(void);

#endif
