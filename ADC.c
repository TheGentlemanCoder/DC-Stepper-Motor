#include <stdint.h>
#include <ADC.h>

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

uint8_t Read_ADC_BUSY() {
	return GPIO_PORTF_DATA_R & 0x08;
}

uint8_t Read_Data_Bits() {
	uint8_t retVal;
	
	retVal = (GPIO_PORTB_DATA_R & (0x20 + 0x10 + 0x08 + 0x04)) << 2;
	retVal |= GPIO_PORTE_DATA_R & (0x08 + 0x04 + 0x02 + 0x01);
	
	return retVal;
}

uint32_t Sample_ADC() {
	uint32_t retVal;
	
	// start conversion
	Toggle_ADC_RC();
	
	// wait until conversion finishes
	while (Read_ADC_BUSY() != 0) {};
		
	// get MSB byte from conversion
	retVal =  (Read_Data_Bits() & 0xFF) << 4;
		
	// get LSB bits from conversion
	Toggle_ADC_BYTE();
	
	for(int i = 0; i < 100; ++i) {}; // waste time while output changes
	
	retVal |= (Read_Data_Bits() & 0xF0) >> 4;
		
	return retVal;
}