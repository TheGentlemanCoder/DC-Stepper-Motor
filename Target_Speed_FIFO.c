#include <stdint.h>
#include "Target_Speed_FIFO.h"
#include "os.h"

uint8_t Target_Speed_FIFO[FIFOSIZE];

uint8_t volatile *PutPt;	// put next
uint8_t volatile *GetPt;	// get next
int32_t CurrentSize;		// 0 means FIFO is empty
int32_t FIFOMutex;			// exclusive access to FIFO
uint32_t LostData;

void OS_FIFO_Init(void) {
	PutPt = &Target_Speed_FIFO[0];
	GetPt = &Target_Speed_FIFO[0];
	OS_InitSemaphore(&CurrentSize, 0);
	OS_InitSemaphore(&FIFOMutex, 1);
	LostData = 0;
	
	for (int i = 0; i < FIFOSIZE; ++i) {
		Target_Speed_FIFO[i] = 0;
	}
}

int OS_FIFO_Put(uint8_t data) {
	if (CurrentSize == FIFOSIZE) {
		LostData++;	// error
		return -1;
	}
	
	*(PutPt) = data;	// Put
	PutPt++;					// place for next
	
	if (PutPt == &Target_Speed_FIFO[FIFOSIZE]) {
		PutPt = &Target_Speed_FIFO[0]; // wrap
	}
	
	OS_Signal(&CurrentSize);
	return 0; // success
}

uint8_t OS_FIFO_Get(void) {
	uint8_t data;
	OS_Wait(&CurrentSize);
	OS_Wait(&FIFOMutex);
	data = *(GetPt);	// CurrentSize != 0 and exclusive access
	GetPt++; // points to next data to get
	
	if (GetPt == &Target_Speed_FIFO[FIFOSIZE]) {
		GetPt = &Target_Speed_FIFO[0];	// wrap
	}
	
	OS_Signal(&FIFOMutex);
	return data;
}

int OS_FIFO_Full(void) {
	return CurrentSize == FIFOSIZE;
}

int OS_FIFO_Empty(void) {
	return CurrentSize == 0;
}

// Get the data at GetPt's position
// without changing the FIFO.
uint8_t OS_FIFO_Peek() {
	uint8_t data;
	OS_Wait(&FIFOMutex);

	// exclusive access to FIFO
	
	if (OS_FIFO_Empty()) {
		// Not enough elements in FIFO
		OS_Signal(&FIFOMutex);
		return 0;
	}
	
	data = *(GetPt);
	OS_Signal(&FIFOMutex);
	
	return data;
}