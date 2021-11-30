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

#define TIMESLICE               32000  // thread switch time in system time units
																			// clock frequency is 16 MHz, switching time is 2ms

uint32_t Switches_in;
uint32_t Switches_use;
uint32_t prev_button;
uint32_t cur_Color = 8;
uint32_t next_Color = 8;
uint32_t counter = 15; 
int32_t sLCD = 1; // LCD Semaphore

uint32_t button_pressed = 0x00;
uint32_t clear_top = 0x00;

void OS_Init(void);
void OS_AddThreads(void f1(void), void f2(void), void f3(void), void f4(void), void f5(void));
void OS_Launch(uint32_t);
void OS_Sleep(uint32_t SleepCtr);
void OS_Fifo_Put(uint32_t data);
uint32_t OS_Fifo_Get(void);
uint32_t OS_Fifo_Peek(void);
void OS_InitSemaphore(int32_t *S, int val);
void OS_Wait(int32_t *S);
void OS_Signal(int32_t *S);
int32_t get_size(void);
void clrLCD(void);

void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts

// function definitions in LCD.s
void Display_Msg(char* msg); // Disable interrupts
void Display_Char(char msg);
void Set_Position(int pos);
void Init_LCD_Ports(void);
void Init_LCD(void);
void Init_Clock(void);

void DCMotor() {

}

void ADC() {

}

void Controller() {

}

void Keypad() {

}

void LCD() {

}

int main(void){
  OS_Init();           // initialize, disable interrupts, 16 MHz
	OS_InitSemaphore(&sLCD, 1); // sLCD is initially 1
	Init_LCD_Ports();
	Init_LCD();
	Init_Clock();
	
  SYSCTL_RCGCGPIO_R |= 0x28;            // activate clock for Ports F and D
  while((SYSCTL_RCGCGPIO_R&0x28) == 0){} // allow time for clock to stabilize
  GPIO_PORTD_DIR_R &= ~0x0F;             // make PD3-0 input
  GPIO_PORTD_DEN_R |= 0x0F;             // enable digital I/O on PD3-1
	GPIO_PORTF_DIR_R |= 0x0E;								// make PF3-1 output
	GPIO_PORTF_DEN_R |= 0x0E;              // enable digital I/O on PF3-1
		
  OS_AddThreads(&DCMotor, &ADC, &Controller, &Keypad, &LCD);
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
