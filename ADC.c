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

void Timer0A_Init(void){
  SYSCTL->RCGCTIMER |= 0x01;      // activate timer0
	TIMER0->CTL &= ~0x00000001;     // disable timer0A during setup
  TIMER0->CFG = 0x00000000;       // configure for timer mode
  TIMER0->TAMR = 0x00000002;      // configure for periodic counting
  TIMER0->TAILR = 0x00000640;     // start value
	TIMER0->ICR = 0x00000004;       // clear timer0A capture match flag
  TIMER0->IMR |= 0x00000001;      // enable timer interrupt
	NVIC->IP[19] = 2 << 5;
  NVIC->ISER[0] = 1 << 19;              // enable interrupt 19 in NVIC
	TIMER0->CTL |= 0x00000001;      // timer0A 24-b, +edge, interrupts
  __enable_irq();
}

void Init_ADC() {
	// The ADC uses the following mapping:
	// 	DB7 - DB4 => PE5 - PE2
	// 	DB3 - DB0 => PB5 - PB2
	// 	R/C  => PC4
	//	BUSY => PC5
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
	
	// Initialize GPIO
	SYSCTL_RCGCGPIO_R |= 0x04; // start clock for Port C
	while ((SYSCTL_PRGPIO_R & 0x04) == 0) {};
		
	// PC4 is used for the RC output signal on the ADC
	GPIO_PORTC_DIR_R |= 0x10;
	GPIO_PORTC_DEN_R |= 0x10;
		
	// PC5 is used for the BUSY signal input from the ADC
	GPIO_PORTC_DIR_R &= ~(0x20);
	GPIO_PORTC_DEN_R |=  (0x20);
		
	// default control signals
	GPIO_PORTC_DATA_R |= 0x10; // set RC signal high
	
	/* configure PORTC5 for rising edge trigger interrupt */
	GPIO_PORTC_IS_R  &= ~(0x20);        /* make bit 5, 0 edge sensitive */
	GPIO_PORTC_IBE_R &= ~(0x20);         /* trigger is controlled by IEV */
	GPIO_PORTC_IEV_R |=  (0x20);        /* rising edge trigger */
	GPIO_PORTC_ICR_R |=  (0x20);          /* clear any prior interrupt */
	GPIO_PORTC_PUR_R &= ~(0x20);					/* enable pull-up resistor for PC5 */
	GPIO_PORTC_IM_R  |=  (0x20);          /* unmask interrupt */
	
	/* enable interrupt in NVIC and set priority to 3 */
	NVIC->IP[2] = 3 << 5;     /* set interrupt priority to 3 */
	NVIC->ISER[0] |= (1<<2);  /* enable IRQ01 (D02 of ISER[0]) */
	
	// initialize timer
	Timer0A_Init();
}

void Toggle_ADC_RC() {
	// toggle PC4
	GPIO_PORTC_DATA_R ^= 0x10;
}

// Active low signal, returns 0 if busy,
// not zero if not busy
uint8_t Read_ADC_BUSY() {
	// read PC5
	return GPIO_PORTC_DATA_R & 0x20;
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
	GPIO_PORTC_DATA_R &= ~0x10;
	GPIO_PORTC_DATA_R |= 0x10;
	//Toggle_ADC_RC();
	//Toggle_ADC_RC();
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
		temp = 0 - (sample);
	} else {
		// sample is positive in two's complement, copy sign bit over
		temp = sample;  
	}
	
	// project sample range onto signal's voltage range
	retVal = (1000 * temp) / scale_constant; // millivolts
	
	return retVal;
}

void TIMER0A_Handler(void) {
	DisableInterrupts();
	// start next sample
	Start_Sample_ADC();
	
	TIMER0_ICR_R = 0x01; // acknowledge timer0A periodic
	EnableInterrupts();
}

void GPIOC_Handler(void) {
	DisableInterrupts();
	if (GPIOC->MIS & 0x20) {  
		if (Read_ADC_BUSY() != 0) {
			// sample is ready
			accum_millivolts += Sample_to_Millivolts(Retrieve_Sample_ADC());
			++sample_count;
			
			if (sample_count >= NUM_SAMPLES) {
				// enough samples taken, calculate new average voltage
				average_millivolts = accum_millivolts / NUM_SAMPLES;
				
				// reset accumulator variables
				accum_millivolts = 0;
				sample_count = 0;
			}
		}
		GPIOC->ICR |= 0x20; /* clear the interrupt flag */
	}
	EnableInterrupts();
}