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
// variables for controller
uint8_t Ts; // Desired Speed in 3.9 rpm units
uint8_t T; // Current Speed in 3.9 rpm units
uint8_t Told; // Previous Speed in 3.9 rpm units
int8_t D; // Change in Speed in 3.9 rpm/time units
int8_t E; // Error in Speed in 3.9 rpm units

# define TE 20
uint8_t Fast, OK, Slow;
uint8_t Down, Constant, Up;
#define TD 20
uint8_t Increase, Same;
uint8_t Decrease;
#define TN 20
int32_t dN; 
// end of controller variables

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

void DCMotor(void) {

}

void ADC(void) {

}

// Fuzzy Logic
void CrispInput(void){
	E = Subtract(Ts,T);
	D = Subtract(T,Told);
	Told = T; // Set up Told for next time
}

void InputMembership(void){ // like the graphs for the membership
	if(E <= -TE) { // E≤-TE
		Fast = 255;
		OK = 0;
		Slow = 0;}
	else if(E < 0){ // -TE<E<0
		Fast = (255*(-E))/TE;
		OK = 255-Fast;
		Slow = 0;}
	else if(E < TE){ // 0<E<TE
		Fast = 0;
		Slow = (255*E)/TE;
		OK = 255-Slow;}
	else { // +TE≤E
		Fast = 0;
		OK = 0;
		Slow = 255;}
	if(D <= -TD) {// D≤-TD
		Down = 255;
		Constant = 0;
		Up = 0;}
	else if(D < 0){// -TD<D<0
		Down = (255*(-D))/TD;
		Constant = 255-Down;
		Up = 0;}
	else if(D < TD){// 0<D<TD
		Down = 0;
		Up = (255*D)/TD;
		Constant = 255-Up;}
	else{ // +TD≤D
		Down = 0;
		Constant = 0;
		Up = 255;} }

uint8_t static min(uint8_t u1,uint8_t u2){
	if(u1>u2) return(u2);
	else return(u1);}

uint8_t static max(uint8_t u1,uint8_t u2){
	if(u1<u2) return(u2);
	else return(u1);}

void OutputMembership(void){
	Same = min(OK,Constant);
	Decrease = min(OK,Up)
	Decrease = max(Decrease,min(Fast,Constant));
	Decrease = max(Decrease,min(Fast,Up));
	Increase = min(OK,Down);
	Increase = max(Increase,min(Slow,Constant));
	Increase = max(Increase,min(Slow,Down));
}

void CrispOutput(void){
	dN=(TN*(Increase-Decrease))/(Decrease+Same+Increase);
}

void Timer0A_Handler(void){
	T = SE(); // estimate speed, set T, 0 to 255
	CrispInput(); // Calculate E,D and new Told
	InputMembership(); // Sets Fast, OK, Slow, Down,
	//Constant, Up
	OutputMembership(); // Sets Increase, Same, Decrease
	CrispOutput(); // Sets dN
	N = max(0,min(N+dN,255));
	PWM0A_Duty(N); // output to actuator
	TIMER0_ICR_R = 0x01; // acknowledge timer0A periodic
	// timer
}

void PWM1C_Duty(uint16_t duty){
	PWM1_3_CMPA_R = duty - 1;
	// count value when output rises
}
// End of fuzzy logic

void Keypad(void) {

}

void LCD(void) {

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
		
  OS_AddThreads(&DCMotor, &ADC, &Timer0A_Handler, &Keypad, &LCD);
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
