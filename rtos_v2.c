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
uint32_t Ts = 0; // Desired Speed in 9.4 rpm units
uint32_t T = 0; // Current Speed in 9.4 rpm units
uint32_t Told = 0; // initial val 0 // Previous Speed in 3.9 rpm units
int32_t D; // Change in Speed in 9.4 rpm/time units
int32_t E; // Error in Speed in 9.4 rpm units

# define TE 20
uint8_t Fast, OK, Slow;
uint8_t Down, Constant, Up;
#define TD 20
uint8_t Increase, Same;
uint8_t Decrease;
#define TN 20
int32_t dN;
int32_t N = 0; //TODO- set this variable correctly
int32_t voltage = 0; // TODO- might be different variable for the current voltage
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
char* Hex2ASCII(int32_t);
int32_t ASCII2Hex(char);
void Clock_Init(void);

void Init_Keypad(void);
void Read_Key(void);
void Delay1ms(void);

int32_t average_millivolts; // ADC will return value to here

// helper function for voltage to speed
int32_t Current_speed(int32_t Avg_volt){ // This function returns the current
                                           // DC motor RPM given the voltage in mV
  if (Avg_volt<= 1200) {return 0;}
  else {return ((21408*Avg_volt)>>16)-225;}
}


void DisplayOrNot(uint8_t num) {
	if(num == 0)
		Display_Char(' ');
	else
		Display_Char((char) (num+0x30));
}

// not a thread
void DCMotor(void) {
	for(;;){	
		MOT12_Speed_Set(N);
	}
}

// 8-bit Fuzzy Logic
void CrispInput(void){
	// -, from subtract function in pseudocode
	E = Ts - T; // E, error
	D = T - Told; // D, acceleration
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

// In Fuzzy Logic and is performed by taking the minimum and or is by the maximum
// Increase, Decrease, and Same are according to a table on the slides
void OutputMembership(void){ // relationship between the input fuzzy membership sets and fuzzy output membership values
	Same = min(OK,Constant);
	// Decrease= (OK and Up) or (Fast and Constant) or (Fast and Up)
	Decrease = min(OK,Up);
	Decrease = max(Decrease,min(Fast,Constant));
	Decrease = max(Decrease,min(Fast,Up));
	Increase = min(OK,Down);
	//Increase= (OK and Down) or (Slow and Constant) or (Slow and Down)
	Increase = max(Increase,min(Slow,Constant));
	Increase = max(Increase,min(Slow,Down));
}

void CrispOutput(void){ // Defuzzification
	dN=(TN*(Increase-Decrease))/(Decrease+Same+Increase);
}

void Controller(void) {
	while(1) {
		cur_rpm = Current_speed(average_millivolts);
		T = 256*cur_rpm/2400;
		// estimate speed, set T, 0 to 255
		Ts = 256*des_rpm/2400;

		CrispInput(); // Calculate E,D and new Told
		InputMembership(); // Sets Fast, OK, Slow, Down,
		//Constant, Up
		OutputMembership(); // Sets Increase, Same, Decrease
		CrispOutput(); // Sets dN
		N = max(0,min(N+dN,255));
		DCMotor(); // update motor here
	}; // loop infinitely
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
		Display_Char((char) (Ts % 10 + 0x30));
		Display_Msg(" C: ");
		DisplayOrNot(cur_rpm / 1000); //TODO- put actuall current rpm
		DisplayOrNot((cur_rpm / 100) % 10);
		DisplayOrNot((cur_rpm / 10) % 10);
		Display_Char((char) (cur_rpm % 10 + 0x30));
		OS_Signal(&sLCD);
	}
}

int main(void){
  OS_Init();           // initialize, disable interrupts, 16 MHz
	OS_InitSemaphore(&sLCD, 1); // sLCD is initially 1
	Init_LCD_Ports();
	Init_LCD();
	Clock_Init();
	Init_ADC();
	Init_Keypad();
	PWM_setup();
	
	SYSCTL_RCGCGPIO_R |= 0x28;            // activate clock for Ports F and D
  while((SYSCTL_RCGCGPIO_R&0x28) == 0){} // allow time for clock to stabilize

  OS_AddThreads(&LCD_Bottom, &Keypad, &Controller);

  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
