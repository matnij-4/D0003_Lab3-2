/*
 * D0003E_Lab3-1.c
 *
 * Created: 2020-02-06 08:38:09
 * Author : Mattias Nilsson
 */ 

#include "tinythreads.h"
#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>


//Global PP
int pp;


//The Mutex for the button and blink.
mutex mutexButton = MUTEX_INIT;
mutex mutexBlink = MUTEX_INIT;

//The counter for the button.
int counterButton = 0;


void LCDInit(void) {
	
	//Set Lowpower Waveform, no frame interrupt, no blanking. LCD Enable
	LCDCRA = (1 << LCDAB) | (1 << LCDEN);
	
	//drive time 300 microseconds, contrast control voltage 3.35 V.
	LCDCCR = (1 << LCDCC0) | (1 << LCDCC1) | (1 << LCDCC2) | (1 << LCDCC3);
	
	//external asynchronous clock source, 1/3 bias, 1/4 duty cycle, 25 segments.
	LCDCRB = (1 << LCDCS) | (1<< LCDMUX0) | (1<< LCDMUX1) | (1 <<LCDPM0) | (1 <<LCDPM1) | (1 <<LCDPM2);
	
	//prescaler setting N=16, clock divider setting D=8
	LCDFRR = (1 << LCDCD0) | (1 << LCDCD1) | (1 << LCDCD2);

}



//Will write the Char on the screen as position pos.
void writeChar(char ch, int pos)
{
	
	//The Address to the first position on the LCD.
	uint8_t *lcdaddr = (uint8_t *) 0xec;
	
	//Mask to get only the nibble of a value.
	uint8_t mask;
	
	//Nibbel the number that is sent to the LCD.
	uint8_t nibbleNumber = 0x0;
	
	
	//SCC Table with the numbers from 0 to 9.
	uint16_t sccTable[10] = {0x1551, 0x0110, 0x1e11, 0x1B11, 0x0B50, 0x1B41, 0x1F41, 0x0111, 0x1F51, 0x0B51};
	
	
	// Check if position is outside or not.
	if (pos < 0 || pos > 5) {
		return;
	}
	
	// The number to print.
	uint16_t number = 0x0;
	
	// Check if it is a 0 to 9.
	if (ch >= '0' || ch <= '9')
	{
		//Get the number out of the array.
		number = sccTable[ch - '0'];
	}
	
	
	
	
	//Point to the right position. Divide by 2 you can say.
	lcdaddr += pos >> 1;
	
	
	//Check if it is odd or even possition.
	if (pos % 2 == 0)
	{
		mask = 0xf0;
	}
	else
	{
		mask = 0x0f;
	}
	
	
	//Will place out the nibbels on the right LCD address for the number.
	for (int i = 0; i < 4; i++ )
	{
		//Masking the smallest byte.
		nibbleNumber = number & 0xf;
		number = number >> 4;
		
		
		//Check position.
		if(pos % 2 != 0)
		{
			//Shift the nibble to the right pos.
			nibbleNumber = nibbleNumber << 4;
		}
		
		//Send the nibble.
		*lcdaddr = (*lcdaddr & mask) | nibbleNumber;
		
		lcdaddr += 5;
		
	}
	
}

//Calculates the prime.
bool is_prime(long i)
{
	//Loop all the numbers under i and try to divide it with them.
	for(int n = 2; n < i; n++)
	{
		// Found number it can divide with.
		if(i % n == 0)
		{
			return false;
		}
		
	}
	return true;
	
}



void printAt(long num, int pos) {
	
	
	pp = pos;
	writeChar( (num % 100) / 10 + '0', pp);
	pp++;
	writeChar( num % 10 + '0', pp);
	
}

//Make one of the segments of the LCD blink.
void blink(void)
{
	
	bool halftime = true;
	
	// Loop to make the LCD blink.
	while(true)
	{
		
		//lock the mutex or else it cat get clock blink var.
		lock(&mutexBlink);
		
		if(halftime)
		{
			halftime = false;
			continue;
		}
		
		//Will start the blinking if 1 sec have passed. 20 * 50ms = 1s
		if(LCDDR3 != 0)
		{
			//Turn in off
			LCDDR3 = 0x0;
		}
		else			
		{
			//Turn it on.
			LCDDR3 = 0x1;
		}
		
		halftime = true;
		
	}
}


//Activates lights on the LCD by using the joystick.
void button(int pos)
{
	while(true)
	{
		lock(&mutexButton);
		printAt(counterButton, 3);
	}
	
	
}
//Counts the primes.
void computePrimes(int pos) {
	long n;

	for(n = 1; ; n++) {
		if (is_prime(n)) {
			printAt(n, pos);
		}
	}
}

int main() {
	
	CLKPR = 0x80;
	CLKPR = 0x00;
	
	LCDInit();
	
	lock(&mutexBlink);
	lock(&mutexButton);
	
	spawn(computePrimes, 0);
	spawn(button, 3);
	blink();
	
}



ISR(TIMER1_COMPA_vect) {
	unlock(&mutexBlink);
	yield();
}


ISR(PCINT1_vect){
	//Read the pin.
	if((PINB >> 7) == 0) {
		
		//Need global counter or the mutex won't work.
		counterButton++;
		
		//Unlock and change.
		unlock(&mutexButton);
		yield();
	}
}