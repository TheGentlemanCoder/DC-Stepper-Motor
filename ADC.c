#include <stdint.h>
#include "ADC.h"

void Init_ADC() {
	// Initialize data pins PB5 - PB2
	SYSCTL_RCGCGPIO_R |= 0x02;
	while ((SYSCTL_PRGPIO_R  & 0x02) == 0) {};
	
	GPIO_PORTB_DIR_R &= ~(0x20 + 0x10 + 0x08 + 0x04);
	GPIO_PORTB_DEN_R |=   0x20 + 0x10 + 0x08 + 0x04;
	
	// Initialize data pins PE3 - PE0
	SYSCTL_RCGCGPIO_R |= 0x10;
	while ((SYSCTL_PRGPIO_R & 0x10) == 0) {};
		
	GPIO_PORTE_DIR_R &= ~(0x08 + 0x04 + 0x02 + 0x01);
	GPIO_PORTE_DEN_R |=   0x08 + 0x04 + 0x02 + 0x01;
	
	// Initialize control signal pins PF3 - PF1
	SYSCTL_RCGCGPIO_R |= 0x20;
	while ((SYSCTL_PRGPIO_R & 0x20) == 0) {};
	
	// PF1 is used for the BYTE output signal on the ADC
	GPIO_PORTF_DIR_R |= 0x02;
	GPIO_PORTF_DEN_R |= 0x02;
		
	// PF2 is used for the R/C output signal on the ADC
	GPIO_PORTF_DIR_R |= 0x04;
	GPIO_PORTF_DEN_R |= 0x04;
	
	// PF3 is used for the BUSY signal output from the ADC
	GPIO_PORTF_DIR_R &= ~0x08;
	GPIO_PORTF_DEN_R |=  0x08;
		
	// default control signals
	GPIO_PORTF_DATA_R &= ~0x02; // set BYTE signal low
	GPIO_PORTF_DATA_R |=  0x04; // set RC signal high
}

void Toggle_ADC_BYTE() {
	GPIO_PORTF_DATA_R ^= 0x02;
}

void Toggle_ADC_RC() {
	GPIO_PORTF_DATA_R ^= 0x04;
}

// Active low signal, returns 0 if busy,
// not zero if not busy
uint8_t Read_ADC_BUSY() {
	return GPIO_PORTF_DATA_R & 0x08;
}

uint8_t Read_Data_Bits() {
	uint8_t retVal;
	
	retVal = (GPIO_PORTB_DATA_R & (0x20 + 0x10 + 0x08 + 0x04)) << 2;
	retVal |= GPIO_PORTE_DATA_R & (0x08 + 0x04 + 0x02 + 0x01);
	
	return retVal;
}

// Returns a sample in the 12 LSBs of the return value
int32_t Sample_ADC() {
	int32_t retVal;
	
	// start conversion
	Toggle_ADC_RC();
	Toggle_ADC_RC();
	
	// wait until conversion finishes
	while (Read_ADC_BUSY() != 0) {};
		
	// get MSB byte from conversion
	retVal =  (Read_Data_Bits() & 0xFF) << 4;
		
	// get LSB bits from conversion
	Toggle_ADC_BYTE();
	
	for(int i = 0; i < 100; ++i) {}; // waste time while output changes
	
	retVal |= (Read_Data_Bits() & 0xF0) >> 4;
	
	// return ADC to its original state
	Toggle_ADC_BYTE();
		
	return retVal;
}

// Assumes that the input signal to the ADC is between +10V and -10V,
// returns the voltage of the sample in millivolts
int32_t Sample_to_Millivolts(int32_t sample) {
	int32_t retVal;
	int32_t temp;
	uint32_t scale_constant = 205; // 205 sample ticks = 1 V
	
	if (sample & 0x00000000) {
		// sample is negative in two's complement, copy sign bit over
		temp =   (0x00000FFF & sample);
		temp |=  (0xFFFFF000);
	} else {
		// sample is positive in two's complement, copy sign bit over
		temp =   (0x00000FFF & sample);
		temp &= ~(0xFFFFF000);
	}
	
	// project sample range onto signal's voltage range
	retVal = (1000 * temp) / scale_constant; // millivolts
	
	return retVal;
}