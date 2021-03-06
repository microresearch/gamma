
/* ERD/GAMMA

- 2019 - this one and gamma.hex compiled from this is for PD1 as trigger out

- new PCB JAN 14 2016 -trigger out is now on PD0 (was PD1)

///

- trigger mode (4) changed to lower ent!

- re-test entropy

- finished now just more or less to tweak and re-test entropy!
- only one trigger mode now
- to test and try new divider code for trigger and 1 and 2 CV mode - seems okayTODO!
- also timing of 0 to tweak.. but mode 3 trigger in is fine

- recheck ent now with floatDONE, also if we can detect end of GM pulse
  and if this effects entropyNOT MUCH, 

- ideally knobs will restrict cv in but how=DONE - to be tested (and
  also does away with comps for scaler)

- swop trigger modes roundDONE

quick modes:
0-random at x time from 10Hz to 120 seconds **DONE
1-random at divider time 10Hz to XX seconds max(255*floaty[255]=255*32=8160 >>3 = 1020 /10pulses = 102seconds - TODO test with finished source
2- low ent rand at divider time 100Hz to XX seconds = 8160 as above but 100pulses=81seconds - TODO test with finished source
3- triggerIN **DONE

trigger out timing is thus also as mode 2

also we can add a new trigger mode= on modes 2 and 3 = random time
scaled by random (scaled by speedscale) ADDED but this is now MODES 0 and 1 !!!

=======
- re-test entropy

- finished now just more or less to tweak and re-test entropy!
- only one trigger mode now
- to test and try new divider code for trigger and 1 and 2 CV mode - seems okayTODO!
- also timing of 0 to tweak.. but mode 3 trigger in is fine

- recheck ent now with floatDONE, also if we can detect end of GM pulse
  and if this effects entropyNOT MUCH, 

- ideally knobs will restrict cv in but how=DONE - to be tested (and
  also does away with comps for scaler)

- swop trigger modes roundDONE

quick modes:
0-random at x time from 10Hz to 120 seconds **DONE
1-random at divider time 10Hz to XX seconds max(255*floaty[255]=255*32=8160 >>3 = 1020 /10pulses = 102seconds - TODO test with finished source
2- low ent rand at divider time 100Hz to XX seconds = 8160 as above but 100pulses=81seconds - TODO test with finished source
3- triggerIN **DONE

trigger out timing is thus also as mode 2

also we can add a new trigger mode= on modes 2 and 3 = random time
scaled by random (scaled by speedscale) ADDED but this is now MODES 0 and 1 !!!

>>>>>>> 8b5090e1ce7b483bf6d2b16e93b5b427294ee132
pulsefresco=(float)lessrandom*pgm_read_float(&floaty[lastless%speedscale]);

//////////////////// OLDER- what are interrupts?

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

=======

1-01: as above lastrandom*speedscale - but we need to update this when we generate lastrandom so is slower  - match to 1
from previous code: based on random time interval * lastrandom * scale (log?)* - slow?
divider is fresco=(lastrandom*speedscale)>>2;

>>>>>>> 8b5090e1ce7b483bf6d2b16e93b5b427294ee132
2-11: speedscale is divider and run as fast as possible on pulses - match to 2

3-11: ??? trigger in gives next pulse speedscaled but only a short time delay/??? - match to 3 

Notes not implemented: 

[- Store entropy in array for use just in case (then we can select random value)]
- say we have 150 cps = 1/150=1/200 say = 0.005 = average 500 microseconds every event

 */

#define F_CPU 16000000UL 
//#define F_OSC 16000000UL 

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
#include <avr/pgmspace.h>

#define floor(x) ((int)(x))
#define MAX_SAM 255
#define BV(bit) (1<<(bit)) // Byte Value => converts bit into a byte value. One at bit location.
#define cbi(reg, bit) reg &= ~(BV(bit)) // Clears the corresponding bit in register reg
#define sbi(reg, bit) reg |= (BV(bit))              // Sets the corresponding bit in register reg
#define HEX__(n) 0x##n##UL

#define CONSTRAIN(var, min, max) \
  if (var < (min)) { \
    var = (min); \
  } else if (var > (max)) { \
    var = (max); \
  }

volatile unsigned char lastrandom;
volatile unsigned char mode, trigmode; 
volatile signed int scaler,speedscale;
volatile unsigned int highcounter=0, hightimer=0;
volatile unsigned int speed;

const unsigned int speedlook[256] PROGMEM ={10, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24,
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
8231, 8459, 8693, 8933, 9181, 9435, 9696, 9964, 10240, 12000};

const float floaty[256] PROGMEM ={0.0078125, 0.00807257186418, 0.00834130131229, 0.00861897654838, 0.00890589537055, 0.00920236549038, 0.00950870486291, 0.00982524202766, 0.010152316461, 0.01049027894, 0.0108394919192, 0.0112003299186, 0.0115731799258, 0.0119584418109, 0.0123565287555, 0.0127678676954, 0.013192899779, 0.01363208084, 0.0140858818867, 0.0145547896064, 0.0150393068884, 0.0155399533625, 0.0160572659564, 0.016591799472, 0.01714412718, 0.0177148414348, 0.0183045543098, 0.0189138982537, 0.0195435267693, 0.0201941151135, 0.0208663610224, 0.0215609854588, 0.0222787333861, 0.0230203745666, 0.0237867043878, 0.0245785447145, 0.0253967447712, 0.026242182052, 0.0271157632624, 0.0280184252912, 0.0289511362156, 0.0299148963392, 0.030910739265, 0.0319397330037, 0.0330029811194, 0.0341016239127, 0.0352368396442, 0.0364098457977, 0.037621900386, 0.0388743033002, 0.0401683977037, 0.0415055714729, 0.0428872586853, 0.0443149411577, 0.0457901500355, 0.0473144674346, 0.0488895281384, 0.0505170213507, 0.0521986925079, 0.0539363451502, 0.0557318428564, 0.0575871112424, 0.0595041400262, 0.0614849851619, 0.0635317710448, 0.0656466927894, 0.0678320185841, 0.0700900921231, 0.0724233351207, 0.0748342499079, 0.0773254221162, 0.079899523451, 0.0825593145564, 0.0853076479761, 0.0881474712129, 0.0910818298895, 0.0941138710149, 0.0972468463595, 0.100484115943, 0.103829151636, 0.107285540887, 0.110856990566, 0.114547330942, 0.118360519793, 0.122300646644, 0.126371937161, 0.130578757677, 0.134925619876, 0.139417185634, 0.144058272017, 0.148853856446, 0.153809082038, 0.158929263118, 0.164219890924, 0.169686639489, 0.175335371734, 0.181172145747, 0.187203221291, 0.193435066505, 0.199874364853, 0.206528022283, 0.213403174636, 0.220507195301, 0.227847703122, 0.235432570565, 0.243269932167, 0.251368193256, 0.259736038966, 0.268382443555, 0.277316680023, 0.286548330063, 0.296087294335, 0.305943803084, 0.316128427109, 0.326652089107, 0.337526075379, 0.34876204794, 0.360372057023, 0.372368554005, 0.384764404757, 0.397572903446, 0.410807786793, 0.4244832488, 0.438613955978, 0.453215063073, 0.468302229323, 0.483891635246, 0.5, 0.516644599307, 0.533843283987, 0.551614499096, 0.569977303715, 0.588951391384, 0.608557111227, 0.62881548977, 0.649748253501, 0.671377852161, 0.693727482827, 0.716821114788, 0.740683515249, 0.765340275898, 0.79081784035, 0.817143532506, 0.844345585856, 0.872453173763, 0.901496440746, 0.931506534812, 0.96251564086, 0.994557015198, 1.02766502121, 1.06187516621, 1.09722413952, 1.13374985183, 1.17149147582, 1.21048948824, 1.25078571323, 1.29242336726, 1.33544710543, 1.37990306936, 1.42583893671, 1.47330397226, 1.52234908082, 1.57302686173, 1.62539166535, 1.67949965133, 1.73540884879, 1.79317921864, 1.8528727178, 1.91455336571, 1.97828731296, 2.04414291224, 2.11219079164, 2.18250393041, 2.25515773723, 2.33023013105, 2.4078016247, 2.48795541121, 2.57077745304, 2.65635657427, 2.74478455586, 2.83615623409, 2.93056960227, 3.02812591581, 3.12892980085, 3.23308936645, 3.3407163205, 3.45192608961, 3.56683794281, 3.68557511952, 3.80826496168, 3.93503905036, 4.06603334687, 4.20138833852, 4.34124918938, 4.48576589588, 4.63509344773, 4.78939199411, 4.94882701544, 5.11356950087, 5.28379613161, 5.45968947047, 5.64143815763, 5.82923711293, 6.02328774495, 6.22379816701, 6.43098342032, 6.64506570469, 6.86627461674, 7.0948473962, 7.33102918031, 7.57507326674, 7.82724138524, 8.08780397831, 8.3570404913, 8.63523967204, 8.92269988057, 9.21972940907, 9.52664681254, 9.84378125042, 10.1714728396, 10.5100730191, 10.8599449273, 11.221463791, 11.5950173278, 11.9810061626, 12.3798442563, 12.7919593506, 13.2177934261, 13.6578031767, 14.1124604993, 14.5822529998, 15.0676845162, 15.5692756587, 16.0875643684, 16.6231064938, 17.1764763875, 17.7482675215, 18.339093124, 18.9495868375, 19.5804033974, 20.232219335, 20.9057337029, 21.6016688243, 22.3207710682, 23.0638116495, 23.8315874563, 24.6249219044, 25.4446658206, 26.2916983547, 27.1669279232, 28.0712931826, 29.0057640367, 29.9713426767, 30.9690646558, 32.0,64.0}; // this one is /128

inline unsigned char adcread(unsigned char channel);

inline void readpots(void){
    speedscale=adcread(0); // knob
    speedscale -= adcread(3); // minus CV
    CONSTRAIN(speedscale, 0, 255);
    speed=pgm_read_word(&speedlook[speedscale]); // this works
    scaler=adcread(1);
    scaler -= adcread(2);
    CONSTRAIN(scaler, 1, 255);
}

inline void outit(unsigned char outittt){
  //  static unsigned char digflag:
    unsigned char res;
  // invert it
    //    outittt=128;
  res=255-outittt;
  if (res==0){
    // PD6 low
    cbi(TCCR0A, COM0A1); // fixed
    cbi(PORTD,6); // pd6 IS OUT
  }
  else{
      // reconnect
     TCCR0A=(1<<COM0A1) | (1<<WGM01) | (1<<WGM00); 
     OCR0A=res;
         }
}


ISR (INT0_vect) // geiger interrupt
{
  static unsigned int modeonecounter=0,modetwocounter=0,pulseonecounter=0,fresco=0,pulsefresco=0, newfresco=0;
  unsigned char bitt;
  unsigned long assign,cnt=0; 
  static unsigned char bitcount=0;
  static unsigned char lastless,temprandom=0;
  static unsigned char lessrandom;
  //  bit=(TCNT1L)&0x01;


  lastless=lessrandom;
  assign=TCNT1L;
  lessrandom=(assign%255);
  //    printf("%c",lessrandom);
  // new max trigger code - on serial exposed PIN PD1
  // put divider and maybe other trigger modes here

    pulseonecounter++;
    if (pulseonecounter>=pulsefresco){
      pulseonecounter=0;
      /* PORTD=0x32;
      _delay_ms(0.2); //200uS pulse!
      PORTD=0x30;
      */
      // redo pulse as:
      sbi(PORTD,1); // was 1
      _delay_ms(0.2); //200uS pulse!
      cbi(PORTD,1); // was 1

      
      if (trigmode==0) {
	readpots();
	//  printf("%d\n\r",speedscale);

	if (speedscale==0) speedscale=1;
      pulsefresco=(float)lessrandom*pgm_read_float(&floaty[lastless%speedscale]); //NEW MODE!
      }
      else {
	readpots();
	pulsefresco=(float)lessrandom*pgm_read_float(&floaty[speedscale]);
	}
    } //pulsecounter
      
  if (mode==2){ // low entropy mode
    // we need two bits though - ignore this for now TODO as only in case we run fast!
    //    cntt++;
    //    if (cntt==2){
    //      cntt=0;

    readpots();

      modetwocounter++;
      if (modetwocounter>=newfresco){
	modetwocounter=0;
	//	  if (scaler==0) scaler=1;
	//	  OCR0A=lessrandom%scaler;
	outit(lessrandom%scaler);
	  newfresco=(float)lastless*pgm_read_float(&floaty[speedscale]);

      //      }
  }
  }

  //  bitt=assign>>7; //TODO: rather than &0x01 ??? check entropy
  bitt=assign&0x01; //TODO: rather than &0x01 ??? check entropy - this is better
  // accumulate into temprandom and this becomes lastrandom on finishing
  temprandom+=bitt<<bitcount;
  bitcount++;
  if (bitcount==8){
    lastrandom=temprandom;

    //    printf("%c",lastrandom);
    if (mode==1){
      modeonecounter++;
      if (modeonecounter>=fresco){
	readpots();

	modeonecounter=0;
	//	  if (scaler==0) scaler=1;
	//	  OCR0A=temprandom%scaler;
	outit(temprandom%scaler);
	  //	  fresco=(lastrandom*speedscale)>>4; // ?? of timing??? as is slow try now >>4
	  // TODO: but we need two independent randoms which we don;t have! TO TEST this?
	  // or we need 2 randoms???? TODO! for now we could use last less 
	  //	  fresco=((lastless>>(shifter[speedscale])+1)*sclerxxx[speedscale])>>4;
	//	speedscale=255; lastless=255;
	  fresco=((float)lastless*pgm_read_float(&floaty[speedscale]));
	  fresco=fresco>>3; 
	  //	  printf("fresco %d\n\r",fresco);

    }
    }
    temprandom=0;
    bitcount=0;
  }
  // try to wait for end here but need an escape!
  /*  while((PIND&0x04) == 0){ // if it stays low!
    cnt++;
    if (cnt>65536) break;
  }
    // try reset here!
  TCCR1A = 0x00;
  TCCR1B = (1<<CS10);
  TCNT1=0;*/
}


ISR (INT1_vect) // trigger interrupt
{
  unsigned long assign=0; 
  unsigned char lessrandom;

  if (mode==3){
    //    if (scaler==0) scaler=1;
    //     OCR0A=lastrandom%scaler; // still need to scale
    readpots();
    assign=TCNT1L;
    lessrandom=(assign%255);
    outit(lessrandom%scaler); // was lastrandom!
  }
}

ISR(TIMER2_OVF_vect) // timer2 interrupt - count between 100ms and 30secs to output dependent on mode
{
  TCNT2=100; // 100 Hz - so for 1 second is 100 of these - WE NEED THIS!
  if (mode==0){
  highcounter++;
  if (highcounter>=hightimer){ 
	readpots();

    highcounter=0;
    //    if (scaler==0) scaler=1;
    //            OCR0A=lastrandom%scaler; // also if is pulse
    outit(lastrandom%scaler);
    //    OCR0A=scaler; // also if is pulse
    hightimer=speed;
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

inline unsigned char adcread(unsigned char channel){
  unsigned char result, high;
  ADMUX &= 0xF8; // clear existing channel selection                
  ADMUX |=(channel & 0x07); // set channel/pin
  ADCSRA |= (1 << ADSC);  // Start A2D Conversions 
  loop_until_bit_is_set(ADCSRA, ADIF); /* Wait for ADIF, will happen soon */
  result=ADCH;
  return result;
}

void main(void)
{
  adc_init();

  // digital in for SW1 on right is PD4, SW2 on left is PD5 - these are modes
  // in for geiger and trigger = PD2,PD3
  // TESTING PD1 as output for trigger out now!

  //        serial_init(9600);
  //	stdout = &mystdout;

  DDRD=0x42; // PD6 as out and PD1 as out for serial // now PD0 is oiut was 0x42
  // 4 and 5 need pullups for switches
  PORTD=0x30; 

  // TIMERS and interrupts
  // TIMER 0 for PWM out

  TIMSK0=0; //????
  TCCR0A=(1<<COM0A1) | (1<<WGM01) | (1<<WGM00); 
  TCCR0B=(1<<CS00) | (1<<CS01);  // divide by 64 for 1KHz

  DIDR0=0x3f; // disable digital inputs on PC0 to PC5

  OCR0A=100;

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
    trigmode=mode>>1;
  }
}

