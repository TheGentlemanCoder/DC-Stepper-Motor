#include <stdint.h>
#include "ADC.h"
#include "tm4c123gh6pm.h"

// how many samples have been taken
// since the average was taken
uint8_t sample_count = 0;

// the average voltage at the DC motor
int32_t average_millivolts = 0;

// accumulator variable used for calculating rolling average
int32_t accum_millivolts = 0;

void Init_ADC() {
	// The ADC uses the following mapping:
	// 	DB7 - DB4 => PE5 - PE2
	// 	DB3 - DB0 => PB5 - PB2
	// 	R/C  => PD7
	//	BUSY => PD6
	//  BYTE => Constant low (selects MSB from ADC)
	
	// Initialize data pins PE5 - PE2 as digital inputs
	SYSCTL_RCGCGPIO_R |= 0x10;
	while ((SYSCTL_PRGPIO_R & 0x10) == 0) {};
		
	GPIO_PORTE_DIR_R &= ~(0x20 + 0x10 + 0x08 + 0x04);
	GPIO_PORTE_DEN_R |=   0x20 + 0x10 + 0x08 + 0x04;
	
	// Initialize data pins PB5 - PB2 as digital inputs
	SYSCTL_RCGCGPIO_R |= 0x02;
	while ((SYSCTL_PRGPIO_R  & 0x02) == 0) {};
	
	GPIO_PORTB_DIR_R &= ~(0x20 + 0x10 + 0x08 + 0x04);
	GPIO_PORTB_DEN_R |=   0x20 + 0x10 + 0x08 + 0x04;
	
	// PB7 is used for the RC output signal on the ADC
	GPIO_PORTB_DIR_R |= 0x80;
	GPIO_PORTB_DEN_R |= 0x80;
		
	// PB6 is used for the BUSY signal input from the ADC
	GPIO_PORTB_DIR_R &= ~(0x40);
	GPIO_PORTB_DEN_R |=  (0x40);
		
	// default control signals
	GPIO_PORTB_DATA_R |=  0x80; // set RC signal high
		
	// Initialize GPIO
		
	/* configure PORTB6 for rising edge trigger interrupt */
	GPIO_PORTB_IS_R &= ~(1<<6)|~(1<<0);        /* make bit 4, 0 edge sensitive */
	GPIO_PORTB_IBE_R &=~(1<<6)|~(1<<0);         /* trigger is controlled by IEV */
	GPIO_PORTB_IEV_R &= ~(1<<6)|~(1<<0);        /* falling edge trigger */
	GPIO_PORTB_ICR_R |= (1<<6)|(1<<0);          /* clear any prior interrupt */
	GPIO_PORTB_IM_R  |= (1<<6)|(1<<0);          /* unmask interrupt */
	
	/* enable interrupt in NVIC and set priority to 3 */
	NVIC->IP[1] = 3 << 5;     /* set interrupt priority to 3 */
	NVIC->ISER[0] |= (1<<1);  /* enable IRQ01 (D01 of ISER[0]) */
}

void Toggle_ADC_RC() {
	GPIO_PORTB_DATA_R ^= 0x80;
}

// Active low signal, returns 0 if busy,
// not zero if not busy
uint8_t Read_ADC_BUSY() {
	return GPIO_PORTB_DATA_R & 0x40;
}

uint8_t Read_Data_Bits() {
	uint8_t retVal;
	
	uint32_t temp = GPIO_PORTE_DATA_R;
	uint32_t temp2 = GPIO_PORTB_DATA_R;
	retVal =  (GPIO_PORTE_DATA_R & (0x20 + 0x10 + 0x08 + 0x04)) << 2;
	retVal |= (GPIO_PORTB_DATA_R & (0x20 + 0x10 + 0x08 + 0x04)) >> 2;
	
	return retVal;
}

void Start_Sample_ADC() {	
	// start conversion
	Toggle_ADC_RC();
	Toggle_ADC_RC();
}

// Returns a sample in the 12 LSBs of the return value
int32_t Retrieve_Sample_ADC() {
	int32_t retVal;
		
	// get MSB byte from conversion
	retVal = (Read_Data_Bits() & 0xFF) << 4;
	
	return retVal;
}

// Assumes that the input signal to the ADC is between +10V and -10V,
// returns the voltage of the sample in millivolts
int32_t Sample_to_Millivolts(int32_t sample) {
	int32_t retVal;
	int32_t temp;
	uint32_t scale_constant = 205; // 205 sample ticks = 1 V
	
	if (sample & 0x00000800) {
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

void TIMER0A_Handler(void) {
	// start next sample
	Start_Sample_ADC();
	
	TIMER0_ICR_R = 0x01; // acknowledge timer0A periodic
}

void GPIOB_Handler(void) {
	if (GPIOB->MIS & 0x40) {  
		if (Read_ADC_BUSY() != 0) {
			// sample is ready
			accum_millivolts += Retrieve_Sample_ADC();
			++sample_count;
			
			if (sample_count >= NUM_SAMPLES) {
				// enough samples taken, calculate new average voltage
				average_millivolts = accum_millivolts / NUM_SAMPLES;
				
				// reset accumulator variables
				accum_millivolts = 0;
				sample_count = 0;
			}
		}
	}
}