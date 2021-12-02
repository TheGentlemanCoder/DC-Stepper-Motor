#include <stdint.h>
#include "tm4c123gh6pm_def.h"

void Init_ADC();
void Toggle_ADC_BYTE();
void Toggle_ADC_RC();
uint8_t Read_ADC_BUSY();
uint8_t Read_Data_Bits();
uint32_t Sample_ADC();
int32_t Sample_to_Millivolts(int32_t sample);