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
#include <math.h>
#include "PWM.h"

#define TIMESLICE               32000  // thread switch time in system time units
																			// clock frequency is 16 MHz, switching time is 2ms

uint32_t Switches_in;
uint32_t Switches_use;
uint32_t prev_button;
// variables for controller
uint32_t Ts = 0; // Desired Speed in 3.9 rpm units
uint32_t T = 0; // Current Speed in 3.9 rpm units
uint32_t Told = 0; // initial val 0 // Previous Speed in 3.9 rpm units
int32_t D; // Change in Speed in 3.9 rpm/time units
int32_t E; // Error in Speed in 3.9 rpm units

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
int32_t test = 0;

void OS_Init(void);
void OS_AddThreads(void f1(void), void f2(void), void f3(void), void f4(void));
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
void Set_Position(int32_t pos);
void Init_LCD_Ports(void);
void Init_LCD(void);
void Clock_Init(void);

void Init_Keypad(void);
void Read_Key(void);
void Delay1ms(void);

void TIMER0A_Handler(void); // thread function header

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

void DCMotor(void) {
	MOT12_Speed_Set(2000);
	for(;;){	
		
	}
}

// get this working first
void ADC(void) {
	for(;;){} 
}

// Fuzzy Logic
void CrispInput(void){
	E = Ts - T; // from subtract function in pseudocode
	D = T - Told;
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

void OutputMembership(void){ // relationship between the input fuzzy membership sets and fuzzy output membership values
	Same = min(OK,Constant);
	Decrease = min(OK,Up);
	Decrease = max(Decrease,min(Fast,Constant));
	Decrease = max(Decrease,min(Fast,Up));
	Increase = min(OK,Down);
	Increase = max(Increase,min(Slow,Constant));
	Increase = max(Increase,min(Slow,Down));
}

void CrispOutput(void){ // Defuzzification
	dN=(TN*(Increase-Decrease))/(Decrease+Same+Increase);
}

// TODO- set up PWM
void PWM1C_Duty(uint16_t duty){
	PWM1_3_CMPA_R = duty - 1;
	// count value when output rises
}

void TIMER0A_Handler(void){
	for(;;){} // TODO - remove as this is for testing only
	
	
	T = Current_speed(voltage); // TODO- edit
		// estimate speed, set T, 0 to 255
	Ts = Ts; // TODO- variables might be changed around for equalling Ts

	CrispInput(); // Calculate E,D and new Told
	InputMembership(); // Sets Fast, OK, Slow, Down,
	//Constant, Up
	OutputMembership(); // Sets Increase, Same, Decrease
	CrispOutput(); // Sets dN
	N = max(0,min(N+dN,255));
	PWM1C_Duty(N); // output to actuator
	TIMER0_ICR_R = 0x01; // acknowledge timer0A periodic
	// timer
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
			Ts = key_rpm;
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
		DisplayOrNot(Ts / 1000);
		DisplayOrNot((Ts / 100) % 10);
		DisplayOrNot((Ts / 10) % 10);
		Display_Char((char) (Ts % 10 + 0x30));
		Display_Msg(" C: ");
		DisplayOrNot(T / 1000); //TODO- put actuall current rpm
		DisplayOrNot((T / 100) % 10);
		DisplayOrNot((T / 10) % 10);
		Display_Char((char) (T % 10 + 0x30));
		OS_Signal(&sLCD);
	}
}

int main(void){
  OS_Init();           // initialize, disable interrupts, 16 MHz
	OS_InitSemaphore(&sLCD, 1); // sLCD is initially 1
	Init_LCD_Ports();
	Init_LCD();
	Clock_Init();
	Init_Keypad();
	PWM_setup();
	MOT12_Speed_Set(2000);

	
  SYSCTL_RCGCGPIO_R |= 0x28;            // activate clock for Ports F and D
  while((SYSCTL_RCGCGPIO_R&0x28) == 0){} // allow time for clock to stabilize
  GPIO_PORTD_DIR_R &= ~0x0F;             // make PD3-0 input
  GPIO_PORTD_DEN_R |= 0x0F;             // enable digital I/O on PD3-1
	GPIO_PORTF_DIR_R |= 0x0E;								// make PF3-1 output
	GPIO_PORTF_DEN_R |= 0x0E;              // enable digital I/O on PF3-1
		
  OS_AddThreads(&DCMotor, &ADC, &Keypad, &LCD_Bottom);
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
