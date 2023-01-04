/**
* James Shawn Carnley
* https://github.com/wrenchpilot
* Multiple effects for pedalShieldUno using debounced toggle switch to 
* cycle through defined effects. Double flick the toggle to change effects.
* BANK 1: Clean w/Volume Control
* BANK 2: Daft Punk Octaver
* BANK 3: Better Tremolo
*/

/**
* CC-by-www.Electrosmash.com
* Based on OpenMusicLabs previous works.
*/

/**
* Better Tremolo based on the stomp_tremolo.pde from openmusiclabs.com
* This program does a tremolo effect.  It uses a sinewave stored
* in program memory to modulate the signal.  The rate at which
* it goes through the sinewave is set by the push buttons,
* which is min/maxed by the speed value.
* https://www.electrosmash.com/forum/pedalshield-uno/453-7-new-effects-for-the-pedalshield-uno#1718
*/

/**
* Toggle switch idea based on post #1186 
* https://www.electrosmash.com/forum/pedalshield-uno/282-multi-effects-clean-boost-distortion-fuzz-crusher#1186
*/

// Uset Toggle library to debounce the toggle switch
#include <Toggle.h>

//defining hardware resources.
#define LED 13
#define FOOTSWITCH 12
#define TOGGLE 2
#define PUSHBUTTON_1 A5
#define PUSHBUTTON_2 A4

Toggle myToggle(TOGGLE);  // Oh, Toggle, My Toggle

//defining the output PWM parameters
#define PWM_FREQ 0x00FF  // pwm frequency - 31.3KHz
#define PWM_MODE 0       // Fast (1) or Phase Correct (0)
#define PWM_QTY 2        // 2 PWMs in parallel

// Setup sine wave for tremolo
const char* const sinewave[] PROGMEM = {
#include "mult16x16.h"
#include "sinetable.h"
};
unsigned int location = 0;  // incoming data buffer pointer

//effects variables:
int vol_variable = 32767;
int speed = 20;                  // tremolo speed
unsigned int fractional = 0x00;  // fractional sample position
int data_buffer;                 // temporary data storage to give a 1 sample buffer
int dist_variable = 10;          // octaver

//other variables
const int max_effects = 3;
int input;
int read_counter = 0;
int ocr_counter = 0;
int effect = 1;  // let's start at 1
byte ADC_low, ADC_high;

void setup() {
  //setup IO
  myToggle.begin(TOGGLE);
  pinMode(FOOTSWITCH, INPUT_PULLUP);
  pinMode(PUSHBUTTON_1, INPUT_PULLUP);
  pinMode(PUSHBUTTON_2, INPUT_PULLUP);
  pinMode(LED, OUTPUT);

  // setup ADC
  ADMUX = 0x60;   // left adjust, adc0, internal vcc
  ADCSRA = 0xe5;  // turn on adc, ck/32, auto trigger
  ADCSRB = 0x07;  // t1 capture for trigger
  DIDR0 = 0x01;   // turn off digital inputs for adc0

  // setup PWM
  TCCR1A = (((PWM_QTY - 1) << 5) | 0x80 | (PWM_MODE << 1));  //
  TCCR1B = ((PWM_MODE << 3) | 0x11);                         // ck/1
  TIMSK1 = 0x20;                                             // interrupt on capture interrupt
  ICR1H = (PWM_FREQ >> 8);
  ICR1L = (PWM_FREQ & 0xff);
  DDRB |= ((PWM_QTY << 1) | 0x02);  // turn on outputs
  sei();                            // turn on interrupts - not really necessary with arduino
}

void loop() {
  //Turn on the LED if the effect is ON.
  if (digitalRead(FOOTSWITCH)) {
    digitalWrite(LED, HIGH);
    myToggle.poll();
    if (myToggle.onPress()) {
      effect++;
      if (effect > max_effects) {
        effect = 1;
      }
    }
  } else {
    digitalWrite(LED, LOW);
  }
}

ISR(TIMER1_CAPT_vect) {

  switch (effect) {
    case (1):  //VOLUME EFFECT

      // get ADC data
      ADC_low = ADCL;  // you need to fetch the low byte first
      ADC_high = ADCH;

      input = ((ADC_high << 8) | ADC_low) + 0x8000;  // make a signed 16b value

      read_counter++;
      if (read_counter == 100) {
        read_counter = 0;
        if (!digitalRead(PUSHBUTTON_2)) {
          if (vol_variable < 32768) vol_variable = vol_variable + 100;  //increase the volume
          digitalWrite(LED, LOW);                                       //blinks the led
        }
        if (!digitalRead(PUSHBUTTON_1)) {
          if (vol_variable > 0) vol_variable = vol_variable - 100;  //decrease volume
          digitalWrite(LED, LOW);                                   //blinks the led
        }
      }

      input = map(input, -32768, +32768, -vol_variable, vol_variable);

      //write the PWM signal
      OCR1AL = ((input + 0x8000) >> 8);  // convert to unsigned, send out high byte
      OCR1BL = input;                    // send out low byte
      break;

    case (2):  //DAFT PUNK OCTAVER
      read_counter++;
      if (read_counter == 2000) {
        read_counter = 0;
        if (!digitalRead(PUSHBUTTON_2)) {  //increase the tremolo
          if (dist_variable < 500) dist_variable = dist_variable + 1;
          digitalWrite(LED, LOW);  //blinks the led
        }
        if (!digitalRead(PUSHBUTTON_1)) {
          if (dist_variable > 0) dist_variable = dist_variable - 1;
          digitalWrite(LED, LOW);  //blinks the led
        }
      }

      ocr_counter++;
      if (ocr_counter >= dist_variable) {
        ocr_counter = 0;
        // get ADC data
        ADC_low = ADCL;  // you need to fetch the low byte first
        ADC_high = ADCH;
        //construct the input sumple summing the ADC low and high byte.
        input = ((ADC_high << 8) | ADC_low) + 0x8000;  // make a signed 16b value

        //write the PWM signal
        OCR1AL = ((input + 0x8000) >> 8);  // convert to unsigned, send out high byte
        OCR1BL = input;                    // send out low byte
      }
      break;

    case (3):  //TREMOLO EFFECT

      // output the last value calculated
      OCR1AL = ((data_buffer + 0x8000) >> 8);  // convert to unsigned, send out high byte
      OCR1BL = data_buffer;                    // send out low byte

      // get ADC data
      byte temp1 = ADCL;                            // you need to fetch the low byte first
      byte temp2 = ADCH;                            // yes it needs to be done this way
      int input = ((temp2 << 8) | temp1) + 0x8000;  // make a signed 16b value

      read_counter++;
      if (read_counter == 1000) {
        read_counter = 0;
        if (!digitalRead(PUSHBUTTON_2)) {
          if (speed < 1024) speed = speed + 1;  //increase the tremolo
          digitalWrite(LED, LOW);               //blinks the led
        }
        if (!digitalRead(PUSHBUTTON_1)) {
          if (speed > 0) speed = speed - 1;  //decrease the tremelo
          digitalWrite(LED, LOW);            //blinks the led
        }
      }

      fractional += speed;         // increment sinewave lookup counter
      if (fractional >= 0x0100) {  // if its large enough to go to next sample
        fractional &= 0x00ff;      // round off
        location += 1;             // go to next location
        location &= 0x03ff;        // fast boundary wrap for 2^n boundaries
      }
      // fetch current sinewave value
      int amplitude = pgm_read_word_near(sinewave + location);
      amplitude += 0x8000;  // convert to unsigned
      int output;
      MultiSU16X16toH16(output, input, amplitude);
      // save value for playback next interrupt
      data_buffer = output;
      break;

    default:
      effect = 1;
      break;
  }
}
