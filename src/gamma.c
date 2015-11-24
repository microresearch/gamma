/* ERD/GAMMA

commencing geiger test code

basic test code now in place without speed or scale and set to mode 0 so should output random bit at 10Hz

////////////////////

//TODO: thinking to switch design to input of geiger on PB0 = ICP1 and using ICP!

//BUT make some tests first using what we have - after all with bit scheme we don;t need so accurate

- what are interrupts?

Geiger is on INT0/PD2
Trigger is on INT1/PD3

- what are inputs and outputs?

Output as euromicro/SIR = - out is on PD6 now // was PD6=0C0A DONE

SW1 on right is PD4, SW2 on left is PD5

Top knob is ADC0/PC0=speed, lower is ADC1/PC1=scale say

CS first top is ADC3/PC3=speed

CV second down is ADC2/PC2=scale

- what are modes?

From switches:

0-00/ scaled random every speed time - int for timing and output lastrandom/scaled - what is our scale for speed say fastest=10Hz to slowest=every 30 seconds - need scale fixed array/log
1-01/ scale random every random time scaled by speed setting - int at random time = two random numbers needed 
2-10/ pulse 5v every x scaled random time - as above
3-11/ 5v trigger in gives last random voltage we generated - trigger interrupt

Notes: 

[- Store entropy in array for use just in case (then we can select random value)]
- say we have 150 cps = 1/150=1/200 say = 0.005 = average 500 microseconds every event

 */

#define F_CPU 16000000UL 

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#define floor(x) ((int)(x))
#define MAX_SAM 255
#define BV(bit) (1<<(bit)) // Byte Value => converts bit into a byte value. One at bit location.
#define cbi(reg, bit) reg &= ~(BV(bit)) // Clears the corresponding bit in register reg
#define sbi(reg, bit) reg |= (BV(bit))              // Sets the corresponding bit in register reg
#define HEX__(n) 0x##n##UL

//volatile long i=0;
static unsigned char entropy[MAX_SAM];
volatile unsigned char lastrandom,otherrandom;
static unsigned char mode;
volatile unsigned int highcounter, hightimer; // 100 mS
static unsigned int speed; // should these be longs?

// need use timer1 for geiger

ISR (INT0_vect) // geiger interrupt
{
  unsigned char bit;
  static unsigned char bitcount=0;
  static unsigned int lasttime=0;
  unsigned int thistime;
  static unsigned char temprandom=0;
  // what is timer count? TIMER1 - has it overflowed though? does it matter?
  thistime=TCNT1;//L+(TCNT1H<<8);
  // generate bits and count upto byte which is lastrandom
  if (lasttime<thistime) bit=0;
  else bit=1;
  // accumulate into temprandom and this becomes lastrandom on finishing
  temprandom+=bit<<bitcount;
  if (bitcount==7){
    lastrandom=temprandom;
    temprandom=0;
  }
  bitcount++;
  lasttime=thistime;
  // in mode 1 we need two random bytes  - do we just keep on accumulating?

  // reset timer?
  TCCR1A = 0x00;
  TCNT1=0;
  //TCNT1H=0;
  // TCNT1L=0; 
}

ISR (INT1_vect) // trigger interrupt
{		
  if (mode==3){
    OCR0A=lastrandom; // still need to scale
  }
}


ISR(TIMER2_OVF_vect) // timer2 interrupt - count between 100ms and 30secs to output dependent on mode
{
  highcounter++;
  TCNT2=100; // 100 Hz - so for 1 second is 100 of these
  // so we can output lastrandom at a specified time?
  if (highcounter==hightimer){
    highcounter=0;
    if (mode!=3) OCR0A=lastrandom; // still need to scale and also if is pulse
    if (mode==1 || mode==2) hightimer=otherrandom; //to be scaled by speed
    else hightimer=speed;
  }
}


void adc_init(void)
{
	cbi(ADMUX, REFS1);
	sbi(ADMUX, REFS0); // AVCC
	sbi(ADMUX, ADLAR); //8 bits
	sbi(ADCSRA, ADPS2);
	sbi(ADCSRA, ADPS1); // change speed here! now div by 64
	//	sbi(ADCSRA, ADPS0); // change speed here!
	sbi(ADCSRA, ADEN);
	sbi(ADCSRA, ADIE);
	DDRC = 0x00;
	PORTC = 0x00;
}

unsigned char adcread(unsigned char channel){
  unsigned char result, high;
  ADMUX &= 0xF8; // clear existing channel selection                
  ADMUX |=(channel & 0x07); // set channel/pin
  ADCSRA |= (1 << ADSC);  // Start A2D Conversions 
  loop_until_bit_is_set(ADCSRA, ADIF); /* Wait for ADIF, will happen soon */
  result=ADCH;
  return result;
}

unsigned int speedlook[255]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,}; // speed scaling TODO

void main(void)
{
  unsigned int counter=0;

  adc_init();

  // digital in for SW1 on right is PD4, SW2 on left is PD5 - these are modes
  // in for geiger and trigger = pd1,pd2
  DDRD=0x40; // PD6 as out

  // TIMERS and interrupts
  // TIMER 0 for PWM out

  TIMSK0=0; //????
  TCCR0A=(1<<COM0A1) | (1<<WGM01) | (1<<WGM00); 
  TCCR0B=(1<<CS00) | (1<<CS01);  // divide by 64 for 1KHz

  DIDR0=0x3f; // disable digital inputs on PC0 to PC5

  OCR0A=128;

  //////////////////

  //pin change interrupt on INT0 and INT1
  EICRA = (1<<ISC01) | (1<<ISC00) | (1<<ISC10) | (1<<ISC11);// rising edge on INT0 and INT1
  EIMSK = (1<<INT0) | (1<<INT1);

  //////////////////

  // Setting Timer 1:
  // normal mode
  TCCR1A = 0x00;
  // Set to clk/1
  TCCR1B |= (1<<CS10);
  // how long to 16 bits with no prescalar=16MHz - 65536/16000000=0.004seconds to overflow?


  // Setting Timer 2: - this one is 8 bit
  // normal mode
  TCCR2A = 0x00;
    // Set to clk/256 
  TCCR2B |= (1<<CS20)|(1<<CS21)|(1<<CS22);// 1024
    //enable overflow interrupt
    TIMSK2 |= (1<<TOIE2);
    //load timer with a value to optimize for 1 second, (256/8MHz)*(65536bits-34000)~=1.009s
    // what we want is say every 0.01 seconds for 100Hz

    //(CPU frequency) / (prescaler value) = 125000 Hz = 8us. 15625 hz
    //   (desired period) / 8us = 125.  0.01 / 0.000064 = 156 
    //   MAX(uint8) + 1 - 125 = 131; 256- 156 = 100

    TCNT2 = 100; 
	
    sei(); //turn on global interrupts - make sure this doesn't interfere with PWM?

    highcounter=0;
    hightimer=10; // 10Hz

  while(1){

    // which mode? pins 4 and 5 become
    //   mode=PIND>>4;
    mode=0;

    // set speed and scaling
    unsigned char speedscale=adcread(0)+adcread(3);
    speed=speedlook[speedscale]*10; // 10 Hz basis todo - now is just 10 still!!
    unsigned char valuescale=adcread(1)+adcread(2);
  }

}

