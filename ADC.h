#include <stdint.h>
#include "tm4c123gh6pm_def.h"

extern uint8_t sample_count;
extern int32_t average_millivolts;
extern int32_t accum_millivolts;

#define NUM_SAMPLES	100

void Init_ADC();
void Toggle_ADC_RC();
uint8_t Read_ADC_BUSY();
uint8_t Read_Data_Bits();
int32_t Sample_ADC();
int32_t Sample_to_Millivolts(int32_t sample);