// contains threads
//*****************************************************************************
// user.c
// Runs on LM4F120/TM4C123
// An example user program that initializes the simple operating system
//   Schedule three independent threads using preemptive round robin  
//   Each thread rapidly toggles a pin on Port D and increments its counter 
//   TIMESLICE is how long each thread runs

// Daniel Valvano
// January 29, 2015
// Modified by John Tadrous
// June 25, 2020

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 
 */

#include "TM4C123GH6PM.h"
#include "tm4c123gh6pm_def.h"
#include <stdio.h>
#include "ADC.h"
#include "LCD_Logic.h"
#include <math.h>
#include "PWM.h"

#define TIMESLICE               32000  // thread switch time in system time units
																			// clock frequency is 16 MHz, switching time is 2ms

uint32_t Switches_in;
uint32_t Switches_use;
uint32_t prev_button;
// variables for controller
uint32_t N = 0;
// end of controller variables

uint8_t Key_ASCII; // contain value returned by Scan_Keypad
uint32_t button_pressed = 0x00;
uint32_t clear_top = 0x00;
int32_t sLCD = 1; // LCD Semaphore
int32_t key_rpm_pos = 0x0B;
int32_t counter = 0;
int32_t key_rpm = 0;
int32_t des_rpm = 0;
int32_t cur_rpm = 0;
int32_t test = 0;

void OS_Init(void);
void OS_AddThreads(void f1(void), void f2(void), void f3(void));
void OS_Launch(uint32_t);
void OS_Sleep(uint32_t SleepCtr);
void OS_Fifo_Put(uint32_t data);
uint32_t OS_Fifo_Get(void);
uint32_t OS_Fifo_Peek(void);
void OS_InitSemaphore(int32_t *S, int val);
void OS_Wait(int32_t *S);
void OS_Signal(int32_t *S);
int32_t get_size(void);

void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts

// function definitions in LCD.s
void Display_Msg(char* msg); // Disable interrupts
void Display_Char(char msg);
void Set_Position(int32_t pos);
void Init_LCD_Ports(void);
void Init_LCD(void);
void Init_Clock(void);
void Clear_LCD(void);

// function definitions provided in ASCII_Conversions.s
char* Hex2ASCII(char* retVal, int32_t);
int32_t ASCII2Hex(char* str);
void Clock_Init(void);

void Init_Keypad(void);
void Read_Key(void);
void Delay1ms(void);

// helper function for voltage to speed
int32_t Current_speed(int32_t Avg_volt){ // This function returns the current
                                           // DC motor RPM given the voltage in mV
  if (Avg_volt<= 1200) {return 0;}
  else {return ((21408*Avg_volt)>>16)-225;}
}

uint32_t duty_cycle;


void DisplayOrNot(uint8_t num) {
		Display_Char((char) (num+0x30));
}

// not a thread
void DCMotor(uint32_t scaled_fuzzy) {
	MOT12_Speed_Set(scaled_fuzzy);
}

// PID controller
void Controller(void) {
	uint32_t temp_controller;
	double kF = 500;
	double kP = 0.75;
	while(1) {
		cur_rpm = Current_speed(average_millivolts);
		N = kP*(des_rpm-cur_rpm) + kF;
		if(N >= 2500)
			N = 2500;
		else if(N <= 0 || des_rpm == 0)
			N = 0;
		else if(des_rpm < kF)
			N = N - kF;
		DCMotor(N); // update motor here
	}
}


// End of fuzzy logic

void Keypad(void) {
	OS_Wait(&sLCD);
	Set_Position(0x00);
	Display_Msg("Input RPM:");
	OS_Signal(&sLCD);
	
	for(;;){
		// output keypad to top of LCD
		OS_Wait(&sLCD);
		Set_Position(key_rpm_pos + counter);
		// display keypad number
		
		Read_Key();
		
		if(Key_ASCII == 0x23 || counter >= 4)
		{
			Set_Position(0x00);
			Display_Msg("Input RPM:     ");
			counter = 0;
			if(key_rpm >= 2400)
				des_rpm = 2400;
			else if (key_rpm < 400)
				des_rpm = 0;
			else
				des_rpm = key_rpm;
			key_rpm = 0;
		}
		else {
			Display_Char(Key_ASCII); //TODO- put actual variable from keypad
			key_rpm = (Key_ASCII - 0x30) * pow(10, 3-counter) + key_rpm; // start from thousandths then go to ones
			counter = counter + 1; // TODO - set this in terms of keypad
		}
		OS_Signal(&sLCD);
	} 
}

void LCD_Bottom(void) {
	for(;;) {
		OS_Wait(&sLCD);
		// display input rpm
		// next line display target and current rpm
		
		Set_Position(0x40); // next line
		Display_Msg("T: ");
		DisplayOrNot(des_rpm / 1000);
		DisplayOrNot((des_rpm / 100) % 10);
		DisplayOrNot((des_rpm / 10) % 10);
		Display_Char((char) (des_rpm % 10 + 0x30));
		Display_Msg(" C: ");
		DisplayOrNot(cur_rpm / 1000); //TODO- put actuall current rpm
		DisplayOrNot((cur_rpm / 100) % 10);
		DisplayOrNot((cur_rpm / 10) % 10);
		Display_Char((char) (cur_rpm % 10 + 0x30));
		OS_Signal(&sLCD);
	}
}

int main(void){
	DisableInterrupts();
  OS_Init();           // initialize, disable interrupts, 16 MHz
	OS_InitSemaphore(&sLCD, 1); // sLCD is initially 1
	Clock_Init();
	Init_LCD_Ports();
	Init_LCD();
	Init_Keypad();
	PWM_setup();
	Init_ADC();
	
  OS_AddThreads(&Keypad, &LCD_Bottom, &Controller);
  EnableInterrupts();
		
	OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
