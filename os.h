#include <stdint.h>

void OS_Init(void);
void SetInitialStack(int i);
int OS_AddThreads(void(*task0)(void),void(*task1)(void),void(*task2)(void));
void OS_Launch(uint32_t theTimeSlice);
void Clock_Init(void);
void OS_Wait(int32_t *S);
void OS_Signal(int32_t *S);
void OS_Sleep(uint32_t SleepCtr);
void OS_InitSemaphore(int32_t*, int32_t);