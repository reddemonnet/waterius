#include "sleep_counter.h"
#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/power.h>    
#include <avr/wdt.h>
#include <EdgeDebounceLite.h>

EdgeDebounceLite debounce;

volatile bool wdtInterrupt;    //watchdog timer interrupt flag
volatile uint32_t btnCount;
volatile uint32_t btn2Count;

//Disabling ADC saves ~230uAF. Needs to be re-enable for the internal voltage check
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC


/* Watchdog interrupt vector */
ISR( WDT_vect ) { 
	wdtInterrupt = true;
}  

/* External interrupt */
ISR(PCINT0_vect)
{
	btnCount += (debounce.pin(BUTTON_PIN) == LOW);
#ifdef BUTTON2_PIN
	btn2Count += (debounce.pin(BUTTON2_PIN) == LOW);
#endif
}

/* Makes a deep sleep for X second using as little power as possible */
void gotoDeepSleep( uint32_t seconds, uint32_t *counter, uint32_t *counter2) 
{
	btnCount = *counter;
	btn2Count = *counter2;
	wdtInterrupt = false;

	pinMode(BUTTON_PIN, INPUT_PULLUP);
#ifdef BUTTON2_PIN
	pinMode(BUTTON2_PIN, INPUT_PULLUP);
#endif

	adc_disable();            // turn off ADC
	power_all_disable();  // power off ADC, Timer 0 and 1, serial interface

	while ( seconds > 0 ) 
	{
		set_sleep_mode( SLEEP_MODE_PWR_DOWN );
		noInterrupts();       // timed sequence coming up
		resetWatchdog();      // get watchdog ready

		GIMSK |= (1 << PCIE);   // pin change interrupt enable
		PCMSK |= BUTTON_INTERRUPT; // pin change interrupt enabled for PCINT4

		sleep_enable();       // ready to sleep
		interrupts();         // interrupts are required now

		sleep_cpu();          // sleep

		sleep_disable();      // precaution
		
		wdt_disable();  // disable watchdog
		
		noInterrupts();       // timed sequence coming up

		PCMSK &= ~BUTTON_INTERRUPT;   // Turn off PB3 as interrupt pin

		if (wdtInterrupt) {
			wdtInterrupt = false;
			seconds--;
		}
	}
	power_all_enable();   // power everything back on

	*counter = btnCount;
	*counter2 = btn2Count;

	pinMode(BUTTON_PIN, OUTPUT);
#ifdef BUTTON2_PIN
	pinMode(BUTTON2_PIN, OUTPUT);
#endif
}

/* Prepare watchdog */
void resetWatchdog() 
{
	MCUSR = 0; // clear various "reset" flags
	WDTCR = bit( WDCE ) | bit( WDE ) | bit( WDIF ); // allow changes, disable reset, clear existing interrupt
	// set interrupt mode and an interval (WDE must be changed from 1 to 0 here)
	WDTCR = bit( WDIE ) | bit( WDP2 ) | bit( WDP1 );    // set WDIE, and 1 seconds delay
	//WDTCR = bit( WDIE ) | bit( WDP3 ) | bit( WDP0 );    // set WDIE, and 8 seconds delay
														
	wdt_reset(); // pat the dog
} 