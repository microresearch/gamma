/* ERD/GAMMA

- finished now just more or less to tweak and re-test entropy!
- changed trigger so added trigger modes to test
- test extra fast low entropy mode
- test all modes just in case esp 1...

// DEC8->

- CV modes work though 01-1 rand at rand time is SLOW!!! to fix
- trigger out modes so far to test:
0- on timer - very, very SLOW!
1- on divider - SLOW
2- speedscale fast - but not enough variation - need like bursts/no bursts but how get range?
3- trig in - again maybe too SLOW but something is wrong ---- redone with flag

////////////////////

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

0-00/ scaled random every speed time - int for timing and output
lastrandom/scaled - what is our scale for speed say fastest=10Hz to
slowest=every 30 seconds - need scale fixed array/log

slowest is like 1min40=10240! DONE with scale

1-01/ scale random every random time scaled by speed setting - int at random time = two random numbers needed TODO====>*or we say that random time is interval and not * and just divide
0-255(scale) byte time coming out? then we don't need 2 values... TODO DONE

// REPLACE! *2-10/ pulse 5v (say 300uS) every x scaled random time ->>>>based on random time interval * lastrandom * scale (log?)* - slow?* DONE

2-11/ as 0 but use low entropy %255 for speed... DONE - to test

one 2-10 mode idea is low entropy speedy CV - again with scales as 00

3-11/ 5v trigger in gives last random voltage we generated - trigger interrupt DONE

and 4 trigger modes all DONE

0-00: use timer to give pulse on lastrandom time * speedscale - match to 0 

1-01: as above lastrandom*speedscale - but we need to update this when we generate lastrandom so is slower  - match to 1
from previous code: based on random time interval * lastrandom * scale (log?)* - slow?
divider is fresco=(lastrandom*speedscale)>>2;

2-11: speedscale is divider and run as fast as possible on pulses - match to 2

3-11: ??? trigger in gives next pulse speedscaled but only a short time delay/??? - match to 3 

Notes not implemented: 

[- Store entropy in array for use just in case (then we can select random value)]
- say we have 150 cps = 1/150=1/200 say = 0.005 = average 500 microseconds every event

 */

#define F_CPU 16000000UL 
#define F_OSC 16000000UL 

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
#include <inttypes.h>

#define floor(x) ((int)(x))
#define MAX_SAM 255
#define BV(bit) (1<<(bit)) // Byte Value => converts bit into a byte value. One at bit location.
#define cbi(reg, bit) reg &= ~(BV(bit)) // Clears the corresponding bit in register reg
#define sbi(reg, bit) reg |= (BV(bit))              // Sets the corresponding bit in register reg
#define HEX__(n) 0x##n##UL

//volatile long i=0;
//unsigned char entropy[MAX_SAM];

volatile unsigned char lastrandom;
volatile unsigned char mode, scaler, speedscale, pulseflag;
volatile unsigned int highcounter=0, hightimer=0, highpulsecounter=0,highpulsetimer=0; // 100 mS
volatile unsigned int speed;

ISR (INT0_vect) // geiger interrupt
{
  static unsigned int modeonecounter=0,pulseonecounter=0,fresco=0,pulsefresco=0, newfresco=0;
  unsigned char bitt;
  unsigned long assign; 
  static unsigned char bitcount=0;
  static unsigned char temprandom=0;
  //  bit=(TCNT1L)&0x01;

  // new max trigger code - on serial exposed PIN PD1
  // put divider and maybe other trigger modes here

    if (mode==1 || mode==2){ // pulsing
    pulseonecounter++;
    if (mode==1 && pulseonecounter>=pulsefresco){
      pulseonecounter=0;
      PORTD=0x32;
      _delay_ms(0.2); //200uS pulse!
      PORTD=0x30;
      pulsefresco=(lastrandom*speedscale)>>2;
    }
    else if (mode==2 && pulseonecounter>=pulsefresco){
      pulseonecounter=0;
      PORTD=0x32;
      _delay_ms(0.2); //200uS pulse!
      PORTD=0x30;
      pulsefresco=speedscale;
    }
      }

  assign=TCNT1L;

  if (mode==2){ // low entropy mode
      modeonecounter++;
      if (modeonecounter>=newfresco){
	modeonecounter=0;
	  if (scaler==0) scaler=1;
	  OCR0A=(assign%255)%scaler;
	  newfresco=speedscale; // ?? of timing??? and maybe fresco is independent of below
      }
  }

  bitt=assign>>7;
  // accumulate into temprandom and this becomes lastrandom on finishing
  temprandom+=bitt<<bitcount;
  bitcount++;
  if (bitcount==8){
    lastrandom=temprandom;
    //    printf("%c",temprandom);
    if (mode==1){
      modeonecounter++;
      if (modeonecounter>=fresco){
	modeonecounter=0;
	  if (scaler==0) scaler=1;
	  OCR0A=temprandom%scaler;
	  fresco=(lastrandom*speedscale)>>2; // ?? of timing??? as is slow
    }
    }
    temprandom=0;
    bitcount=0;
  }
    // try reset here!
  //    TCCR1A = 0x00;
  //    TCCR1B = (1<<CS10);
  //    TCNT1=0;
}


ISR (INT1_vect) // trigger interrupt
{		
  if (mode==3){
    if (scaler==0) scaler=1;
    OCR0A=lastrandom%scaler; // still need to scale
    // set a timer/scaled which then triggers pulse in ISR below...
    highpulsecounter=0;
    pulseflag=0;
    highpulsetimer=(lastrandom*speedscale)>>2; // TEST!
  }
}

ISR(TIMER2_OVF_vect) // timer2 interrupt - count between 100ms and 30secs to output dependent on mode
{
  TCNT2=100; // 100 Hz - so for 1 second is 100 of these - WE NEED THIS!
  if (mode==0){
  highcounter++;
  if (highcounter>=hightimer){ 
    highcounter=0;
    if (scaler==0) scaler=1;
    OCR0A=lastrandom%scaler; // also if is pulse
    hightimer=speed;
  }
  // and for pulse..
  highpulsecounter++;
  if (highpulsecounter>=highpulsetimer){
    highpulsecounter=0;
    highpulsetimer=(lastrandom*speedscale)>>2; // TEST!
    PORTD=0x32;
    _delay_ms(0.2); //200uS pulse!
    PORTD=0x30;
  }
  }
  if (mode==3){
  highpulsecounter++;
  if (highpulsecounter>=highpulsetimer && pulseflag==0){ 
    // add flag so just fires once and reset by trigger
    //    highpulsecounter=0;
    pulseflag=1;
    PORTD=0x32;
    _delay_ms(0.2); //200uS pulse!
    PORTD=0x30;
  }
  }
}

#define UART_BAUD_CALC(UART_BAUD_RATE,F_OSC) ((F_OSC)/((UART_BAUD_RATE)*16l)-1)

void serial_init(int baudrate){
  UBRR0H = (uint8_t)(UART_BAUD_CALC(baudrate,F_CPU)>>8);
  UBRR0L = (uint8_t)UART_BAUD_CALC(baudrate,F_CPU); /* set baud rate */
  UCSR0B = (1<<RXEN0) | (1<<TXEN0); /* enable receiver and transmitter */
  //  UCSR0C = (1<<URSEL) | (3<<UCSZ0);   /* asynchronous 8N1 */
}

static int uart_putchar(char c, FILE *stream);

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,_FDEV_SETUP_WRITE);

static int uart_putchar(char c, FILE *stream)
{
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
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
	cbi(ADCSRA, ADIE);
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

/*unsigned long speedlook[255]={100, 102, 105, 108, 111, 114, 117, 121, 124, 127, 131, 135, 138, 142,
146, 150, 154, 159, 163, 167, 172, 177, 182, 187, 192, 197, 203, 208,
214, 220, 226, 233, 239, 246, 252, 259, 267, 274, 282, 289, 297, 306,
314, 323, 332, 341, 350, 360, 370, 380, 391, 402, 413, 424, 436, 448,
460, 473, 486, 500, 514, 528, 542, 558, 573, 589, 605, 622, 639, 657,
675, 694, 713, 733, 753, 774, 795, 817, 840, 863, 887, 911, 937, 963,
989, 1017, 1045, 1074, 1103, 1134, 1165, 1198, 1231, 1265, 1300, 1336,
1373, 1411, 1450, 1490, 1531, 1574, 1617, 1662, 1708, 1755, 1804,
1854, 1905, 1958, 2012, 2067, 2125, 2183, 2244, 2306, 2370, 2435,
2503, 2572, 2643, 2716, 2791, 2869, 2948, 3030, 3113, 3200, 3288,
3379, 3472, 3569, 3667, 3769, 3873, 3980, 4090, 4204, 4320, 4439,
4562, 4688, 4818, 4951, 5088, 5229, 5374, 5523, 5675, 5832, 5994,
6160, 6330, 6505, 6685, 6870, 7060, 7255, 7456, 7663, 7875, 8092,
8316, 8546, 8783, 9026, 9276, 9532, 9796, 10067, 10345, 10632, 10926,
11228, 11539, 11858, 12186, 12523, 12870, 13226, 13592, 13968, 14354,
14751, 15159, 15579, 16010, 16452, 16908, 17375, 17856, 18350, 18858,
19380, 19916, 20467, 21033, 21615, 22213, 22827, 23459, 24108, 24775,
25460, 26165, 26888, 27632, 28397, 29182, 29990, 30819, 31672, 32548,
33449, 34374, 35325, 36302, 37307, 38339, 39399, 40489, 41610, 42761,
43944, 45159, 46409, 47693, 49012, 50368, 51761, 53193, 54665, 56177,
57732, 59329, 60970, 62657, 64390, 66172, 68002, 69884, 71817, 73804,
75845, 77944, 80100, 82316, 84593, 86934, 89339, 91810, 94350, 96960,
			     99643, 102400};
*/

unsigned int speedlook[256]={10, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 15, 15,
15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24,
25, 25, 26, 27, 28, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
40, 41, 42, 43, 44, 46, 47, 48, 50, 51, 52, 54, 55, 57, 58, 60, 62,
63, 65, 67, 69, 71, 73, 75, 77, 79, 81, 84, 86, 88, 91, 93, 96, 98,
101, 104, 107, 110, 113, 116, 119, 123, 126, 130, 133, 137, 141, 145,
149, 153, 157, 161, 166, 170, 175, 180, 185, 190, 195, 201, 206, 212,
218, 224, 230, 237, 243, 250, 257, 264, 271, 279, 286, 294, 303, 311,
320, 328, 337, 347, 356, 366, 376, 387, 398, 409, 420, 432, 443, 456,
468, 481, 495, 508, 522, 537, 552, 567, 583, 599, 616, 633, 650, 668,
687, 706, 725, 745, 766, 787, 809, 831, 854, 878, 902, 927, 953, 979,
1006, 1034, 1063, 1092, 1122, 1153, 1185, 1218, 1252, 1287, 1322,
1359, 1396, 1435, 1475, 1515, 1557, 1601, 1645, 1690, 1737, 1785,
1835, 1885, 1938, 1991, 2046, 2103, 2161, 2221, 2282, 2345, 2410,
2477, 2546, 2616, 2688, 2763, 2839, 2918, 2999, 3081, 3167, 3254,
3344, 3437, 3532, 3630, 3730, 3833, 3939, 4048, 4161, 4276, 4394,
4515, 4640, 4769, 4901, 5036, 5176, 5319, 5466, 5617, 5773, 5932,
6097, 6265, 6439, 6617, 6800, 6988, 7181, 7380, 7584, 7794, 8010,
			     8231, 8459, 8693, 8933, 9181, 9435, 9696, 9964, 10240,12000};

void main(void)
{
  unsigned int counter=0;

  //  serial_init(9600);
  //  stdout = &mystdout;

  adc_init();

  // digital in for SW1 on right is PD4, SW2 on left is PD5 - these are modes
  // in for geiger and trigger = PD2,PD3
  // TESTING PD1 as output for trigger out now!


  DDRD=0x42; // PD6 as out and PD1 as out for serial

  // 4 and 5 need pullups for switches
  PORTD=0x30; 

  // TIMERS and interrupts
  // TIMER 0 for PWM out

      TIMSK0=0; //????
    TCCR0A=(1<<COM0A1) | (1<<WGM01) | (1<<WGM00); 
    TCCR0B=(1<<CS00) | (1<<CS01);  // divide by 64 for 1KHz

  DIDR0=0x3f; // disable digital inputs on PC0 to PC5

  OCR0A=100;

  // 
  
  //////////////////
  
  //pin change interrupt on INT0 and INT1
  //  EICRA = (1<<ISC01) | (1<<ISC00) | (1<<ISC10) | (1<<ISC11);// rising edge on INT0 and INT1
  EICRA = (1<<ISC01) | (1<<ISC10) | (1<<ISC11);// falling edge on INT0 and rise on INT1
  EIMSK = (1<<INT0) | (1<<INT1);

  //      EICRA = (1<<ISC01)  | (1<<ISC00);// falling edge on INT0 and rise on INT1
  //      EIMSK = (1<<INT0);

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
    //    (1024/16000000) *(256-100)=0.009984 // close to 0.01

    // (1024/16000000) * (256-240)=0.001024

    //(CPU frequency) / (prescaler value) = 125000 Hz = 8us. 15625 hz
    //   (desired period) / 8us = 125.  0.01 / 0.000064 = 156 
    //   MAX(uint8) + 1 - 125 = 131; 256- 156 = 100
    // divide by 10 again = 256-16=240

    //    TCNT2 = 240;// was 100 before  now is for 0.001 seconds	
    TCNT2 = 100;// 100 is for 0.01 seconds
    sei(); //turn on global interrupts - make sure this doesn't interfere with PWM?
  
    highcounter=0;
    hightimer=10; // was 100 for 1Hz is now 1000 for 1Hz
    speed=10;//1 Hz

  while(1){
    // which mode? pins 4 and 5 become
    mode=((PIND>>4)&3)^3; // how to invert it? NOW WORKS?
    //    mode=2;
    // set speed and scaling
    speedscale=adcread(0)+adcread(3);
    scaler=adcread(1)+adcread(2);
    speed=speedlook[speedscale];
  }
}

