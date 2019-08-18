/*
 * BinClock.c
 * Jarrod Olivier
 * Modified for EEE3095S/3096S by Keegan Crankshaw
 * August 2019
 * 
 * GEOJNW001 MNJSIN002
 * Date: 20 August 2019
*/

#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <stdio.h> //For printf functions
#include <stdlib.h> // For system functions
#include <math.h>
#include <signal.h>

#include "BinClock.h"
#include "CurrentTime.h"

//Global variables
int hours, mins, secs;
long lastInterruptTime = 0; //Used for button debounce
int RTC; //Holds the RTC instance

int HH,MM,SS;

void initGPIO(void){
	/* 
	 * Sets GPIO using wiringPi pins. see pinout.xyz for specific wiringPi pins
	 * You can also use "gpio readall" in the command line to get the pins
	 * Note: wiringPi does not use GPIO or board pin numbers (unless specifically set to that mode)
	 */
	printf("Setting up\n");
	wiringPiSetup(); //This is the default mode. If you want to change pinouts, be aware
	
	RTC = wiringPiI2CSetup(RTCAddr); //Set up the RTC
	
	//Set up the LEDS
	for(int i=0; i < sizeof(LEDS)/sizeof(LEDS[0]); i++){
	    pinMode(LEDS[i], OUTPUT);
	}
	
	//Set Up the Seconds LED for PWM
	pinMode(SECS, PWM_OUTPUT);
	//softPwmCreate (SECS, 0, 59);

	printf("LEDS done\n");
	
	//Set up the Buttons
	for(int j=0; j < sizeof(BTNS)/sizeof(BTNS[0]); j++){
		pinMode(BTNS[j], INPUT);
		pullUpDnControl(BTNS[j], PUD_UP);
	}

	//Attach interrupts to Buttons
    wiringPiISR (5, INT_EDGE_FALLING, hourInc);
    wiringPiISR (BTNS[1], INT_EDGE_FALLING, minInc);
	//Write your logic here
	
	printf("BTNS done\n");
	printf("Setup done\n");
}

void cleanGPIO(void) {
    digitalWrite(LEDS[0],0);
	for(int i = 0; i < sizeof(LEDS)/sizeof(LEDS[0]); i++){
		pinMode(LEDS[i], INPUT);
	}
	pinMode(SECS, INPUT);
	exit(1);
}

/*
 * The main function
 * This function is called, and calls all relevant functions we've written
 */
int main(void){
	initGPIO();
	signal(SIGINT, cleanGPIO);

	//Set random time (3:04PM)
	//You can comment this file out later
	wiringPiI2CWriteReg8(RTC, HOUR, 0x13+TIMEZONE);
	wiringPiI2CWriteReg8(RTC, MIN, 0x58);
	wiringPiI2CWriteReg8(RTC, SEC, 0x80);
	
	// Repeat this until we shut down
	for (;;){
		//Fetch the time from the RTC
		//Write your logic here
		hours = wiringPiI2CReadReg8(RTC, HOUR);
		mins = wiringPiI2CReadReg8(RTC, MIN);
		secs = wiringPiI2CReadReg8(RTC, SEC);
		secs = secs & 127;
		
		lightHours(hours);
		lightMins(mins);
		secPWM(secs);

		// Print out the time we have stored on our RTC
		printf("The current time is: %x:%x:%x\n", hours, mins, secs);

		//using a delay to make our program "less CPU hungry"
		delay(1000); //milliseconds
	}
	return 0;
}

/*
 * Change the hour format to 12 hours
 */
int hFormat(int hours){
	/*formats to 12h*/
	if (hours >= 24){
		hours = 0;
	}
	else if (hours > 12){
		hours -= 12;
	}
	return (int)hours;
}

/*
 * Turns on corresponding LED's for hours
 */
void lightHours(int units){
	// Write your logic to light up the hour LEDs here
	int hours = units;
	
	int hoursTens = hours & 48;
	hoursTens = hoursTens >> 4;
	int hoursOnes = hours & 15;
	hours = hoursTens*10 + hoursOnes;
	
	int n = hFormat(hours);

    for (int i = 3; i >= 0; i--) {
        int k = n >> i;
        if (k & 1)
            digitalWrite(LEDS[3-i], 1);
        else
            digitalWrite(LEDS[3-i], 0);
    }
}
//https://www.geeksforgeeks.org/program-decimal-binary-conversion/

/*
 * Turn on the Minute LEDs
 */
void lightMins(int units){
	//Write your logic to light up the minute LEDs here
	int mins = units;
	
	int minsTens = mins & 112;
	minsTens = minsTens >> 4;
	int minsOnes = mins & 15;
	mins = minsTens*10 + minsOnes;
	
    int n = mins;
    for (int i = 5; i >= 0; i--) {
        int k = n >> i;
        if (k & 1)
            digitalWrite(LEDS[9-i], 1);
        else
            digitalWrite(LEDS[9-i], 0);
    }
}

/*
 * PWM on the Seconds LED
 * The LED should have 60 brightness levels
 * The LED should be "off" at 0 seconds, and fully bright at 59 seconds
 */
void secPWM(int units){
	// Write your logic here
	int secs = units;
	int secsTens = secs & 112;
	secsTens = secsTens >> 4;
	int secsOnes = secs & 15;
	secs = secsTens*10 + secsOnes;
	double pwmValue = round(((double)secs/59) * 1023);
	int value = pwmValue;
	//softPwmWrite (SECS, secs);
	pwmWrite (SECS, value);
}

/*
 * hexCompensation
 * This function may not be necessary if you use bit-shifting rather than decimal checking for writing out time values
 */
int hexCompensation(int units){
	/*Convert HEX or BCD value to DEC where 0x45 == 0d45
	  This was created as the lighXXX functions which determine what GPIO pin to set HIGH/LOW
	  perform operations which work in base10 and not base16 (incorrect logic)
	*/
	int unitsU = units%0x10;

	if (units >= 0x50){
		units = 50 + unitsU;
	}
	else if (units >= 0x40){
		units = 40 + unitsU;
	}
	else if (units >= 0x30){
		units = 30 + unitsU;
	}
	else if (units >= 0x20){
		units = 20 + unitsU;
	}
	else if (units >= 0x10){
		units = 10 + unitsU;
	}
	return units;
}


/*
 * decCompensation
 * This function "undoes" hexCompensation in order to write the correct base 16 value through I2C
 */
int decCompensation(int units){
	int unitsU = units%10;

	if (units >= 50){
		units = 0x50 + unitsU;
	}
	else if (units >= 40){
		units = 0x40 + unitsU;
	}
	else if (units >= 30){
		units = 0x30 + unitsU;
	}
	else if (units >= 20){
		units = 0x20 + unitsU;
	}
	else if (units >= 10){
		units = 0x10 + unitsU;
	}
	return units;
}


/*
 * hourInc
 * Fetch the hour value off the RTC, increase it by 1, and write back
 * Be sure to cater for there only being 23 hours in a day
 * Software Debouncing should be used
 */
void hourInc(void){
	//Debounce
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		printf("Interrupt 1 triggered, %x\n", hours);
		//Fetch RTC Time
		//Increase hours by 1, ensuring not to overflow
		//Write hours back to the RTC
		hours = wiringPiI2CReadReg8(RTC, HOUR);
		
		int hoursTens = hours & 48;
		hoursTens = hoursTens >> 4;
		int hoursOnes = hours & 15;
		hours = hoursTens*10 + hoursOnes;
		
		hours = hours + 1;
		if(hours == 25){
		    hours = 0;
		}
		
		hoursTens = hours/10;
		hoursOnes = hours%10;
		hoursTens = hoursTens << 4;
		hours = hoursTens | hoursOnes;
		wiringPiI2CWriteReg8(RTC, HOUR, hours);
    }
	lastInterruptTime = interruptTime;
}

/* 
 * minInc
 * Fetch the minute value off the RTC, increase it by 1, and write back
 * Be sure to cater for there only being 60 minutes in an hour
 * Software Debouncing should be used
 */
void minInc(void){
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		printf("Interrupt 2 triggered, %x\n", mins);
		//Fetch RTC Time
		//Increase minutes by 1, ensuring not to overflow
		//Write minutes back to the RTC
		mins = wiringPiI2CReadReg8(RTC, MIN);
		
		int minsTens = mins & 112;
		minsTens = minsTens >> 4;
		int minsOnes = mins & 15;
		mins = minsTens*10 + minsOnes;
		
		mins = mins + 1;
		if(mins == 60){
		    mins = 0;
		    hourInc();
		}
		
		minsTens = mins/10;
		minsOnes = mins%10;
		minsTens = minsTens << 4;
		mins = minsTens | minsOnes;
		wiringPiI2CWriteReg8(RTC, MIN, mins);
	}
	lastInterruptTime = interruptTime;
}

//This interrupt will fetch current time from another script and write it to the clock registers
//This functions will toggle a flag that is checked in main
void toggleTime(void){
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		HH = getHours();
		MM = getMins();
		SS = getSecs();

		HH = hFormat(HH);
		HH = decCompensation(HH);
		wiringPiI2CWriteReg8(RTC, HOUR, HH);

		MM = decCompensation(MM);
		wiringPiI2CWriteReg8(RTC, MIN, MM);

		SS = decCompensation(SS);
		wiringPiI2CWriteReg8(RTC, SEC, 0b10000000+SS);

	}
	lastInterruptTime = interruptTime;
}

