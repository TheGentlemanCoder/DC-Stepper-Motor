#include "TM4C123GH6PM.h"
#include "tm4c123gh6pm_def.h"
#include "delay.h"

void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts

void MOT12_Dir_Set_Forward(void) {
	GPIOB->DATA &= ~0x01;
	GPIOB->DATA |= 0x02;
}

void MOT12_Dir_Set_Backward(void) {
	GPIOB->DATA |= 0x01;
	GPIOB->DATA &= ~0x02;
}

void MOT12_Init(uint16_t period, uint16_t duty)
{  // motor connects MOT1 & MOT2
	  OS_DisableInterrupts();
		SYSCTL->RCGCPWM |= 0x02;        // enable clock to PWM1
	  //setting up PF2
    SYSCTL->RCGCGPIO |= 0x20;       // Activate port F
    delayMs(1);   
    GPIOF->DIR |= 0x04;             // set PF2 pins as output (LED) pin
    GPIOF->DEN |= 0x04;             // set PF2 pins as digital pins
    GPIOF->AFSEL |= 0x04;           // PF2 enable alternate function
    GPIOF->PCTL &= ~0x00000F00;     // clear PF2 alternate function
    GPIOF->PCTL |= 0x00000500;      // set PF2 alternate function to PWM1
    GPIOF->AMSEL |= 0x04;           // disable analog functions on PF2
    delayMs(1);   		    //setting up PWM1_3
				   // PWM6 seems to take a while to start
    SYSCTL->RCC |= 0x001E0000;     // use PWM DIV and divide clock by 64 --> 16 MHz/64 = 250 KHz
    PWM1->_3_CTL = 0;               // disable PWM1_3 during configuration
    PWM1->_3_GENA = 0x000000C8;   // output low for load, high for match
    PWM1->_3_LOAD = period-1;       // 2499
    PWM1->_3_CMPA = duty-1;         // percent*(loadvalue+1)-1
    PWM1->_3_CTL = 1;               // enable PWM1_3
    PWM1->ENABLE |= 0x40;           // enable PWM1
	
    // enable Port B for setting up motor direction
    SYSCTL->RCGCGPIO |= 0x02;
    while ((SYSCTL->PRGPIO & 0x02) == 0) {};		
    GPIOB->DEN |= 0x03;
    GPIOB->DIR |= 0x03;		
    OS_EnableInterrupts();
}

// Subroutine to set the DC motor duty cycle. Higher duty cycle
// means that the motor will spin faster. Note that duty must be
// less than period, greater than 100, and that the programmer,
// not the code, that enforces that. Be warned!
// Inputs: Duty cycle of PWM (must be less than period).
// Outputs: None
void MOT12_Speed_Set(uint16_t duty)
{
    PWM1->_3_CMPA = duty-1;
}


void PWM_setup(void){
	//period of the PWM
	uint16_t period = 2500; // 250KHz/100Hz -1 = 2500
	
	// required to keep a minimum duty cycle
	uint16_t minmum_duty = 450; //18% of 2500
	
	//initalization
	MOT12_Init(period,minimum_duty);
	MOT12_Dir_Set_Forward();
}
