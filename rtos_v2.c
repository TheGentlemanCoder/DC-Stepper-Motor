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

// index this array
char* colors[] = {"None ", "Red  ", "Blue ", "Purp ", "Grn  ", "Ylw  ", "Cyan ", "Whit ", "???  "};
char disp_count[3];

// Tasks
  // Read Switches (display to LCD) <- check if currentSize is full
	// Button press, debouncing (display LCD)
		// associate event thread, signal semaphore, then sleep it then wake up
		// scan val then reblock, interrupt then one thread read and one thread
		// store. Main thread scan then if press then wait thread times then apply
		// input. Just sleep for timer. OS_Sleep(5). old val PD0 and new val PD0
	// Display to LED

// Read Switches
// Display switch value to LCD top row
void Read_Switches(void){
	for(;;) {
		if ((GPIO_PORTD_DATA_R&0x01) == 0x00) {
			Switches_in = (GPIO_PORTD_DATA_R&0x0E);
		}
	  OS_Wait(&sLCD);
	  Set_Position(0x00);
		if(get_size() == 10)
		{
			Display_Msg(" Buffer is full ");
			clear_top = 1; // flag to clear the top
		}
		else{
			if(get_size() == 9 & clear_top == 1) {
				Set_Position(0x00);
				Display_Msg("                ");
				Set_Position(0x00);
				clear_top = 0; // reset the flag to clear the top
		  }
			Display_Msg("Switches:"); 
			// display at next available 
			Display_Msg(colors[Switches_in >> 1]); 
			// if no work put switches_in in temp var 
		}
		OS_Signal(&sLCD);
	}
}

void Button(void) {
	prev_button = 0;
	uint32_t curr_button = 0;
	for(;;) {
		prev_button = (GPIO_PORTD_DATA_R&0x01);
		OS_Sleep(5);
		curr_button = (GPIO_PORTD_DATA_R&0x01); // compare if this button value is the same for debouncing
		if(prev_button == 1 && curr_button == 1 && button_pressed == 0) {
			OS_Fifo_Put(Switches_in >> 1);
			if(cur_Color == 8) // if at the state where nothing there but not from switch
				cur_Color = OS_Fifo_Get();
			else if(next_Color == 8)
				next_Color = OS_Fifo_Peek();
			button_pressed = 1;
		}
		else if (curr_button == 0)
			button_pressed = 0;
  }
}

void LCD_Bottom(void){
	for(;;) {
		// display current color, next color, and then (optional counting down counter)
		OS_Wait(&sLCD);
		Set_Position(0x40);
		if(next_Color == 8 && cur_Color == 8) {
			Display_Msg("   Input Color  ");
		}
		else {
			Display_Msg("C:");
			Display_Msg(colors[cur_Color]);
			Display_Msg("N:");
			Display_Msg(colors[next_Color]);
			//snprintf(disp_count, 3, "%d", counter);
			//Display_Msg(disp_count);
			Display_Char((char) counter/10 + 0x30);
			Display_Char((char) counter%10 + 0x30);
		}
		OS_Signal(&sLCD);
	}
}

void Display_LED(void){
	for(;;) {
		if(cur_Color == 0) {
		  GPIO_PORTF_DATA_R = 0;	
		}
		else {
		  GPIO_PORTF_DATA_R = cur_Color << 1;
		}
	}
}

// semaphore and variable count 15 to 0 
// and if count 0 and Sleep for 1 updates and semaphore update
void Fifteen_Periodic(void) {
	for(;;) {
		OS_Sleep(500); //sleep for 1s
		--counter;
		if(counter <= 0) { 
			counter = 15;
			// update next and cur colors
			if(get_size() >= 1) {
				cur_Color = OS_Fifo_Get();
				next_Color = OS_Fifo_Peek();
				if(get_size() <= 0) {
					next_Color = 8;
				}
			}
			else if(get_size() <= 0) {
				cur_Color = 8;
			}
		}
	}
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
		
  OS_AddThreads(&Read_Switches, &Button, &LCD_Bottom, &Display_LED, &Fifteen_Periodic);
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
