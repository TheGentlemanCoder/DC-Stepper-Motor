#define FIFOSIZE 4
extern uint8_t Target_Speed_FIFO[FIFOSIZE];

void OS_FIFO_Init(void);
int OS_FIFO_Put(uint8_t data);
uint8_t OS_FIFO_Get(void);
int OS_FIFO_Full(void);
int OS_FIFO_Empty(void);
uint8_t OS_FIFO_Peek();