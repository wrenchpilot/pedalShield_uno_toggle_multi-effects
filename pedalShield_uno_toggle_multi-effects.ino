/**
* James Shawn Carnley
* https://github.com/wrenchpilot
* Multiple effects for pedalShieldUno using debounced toggle switch to 
* cycle through defined effects. Double flick the toggle to change effects.
* BANK 1: Booster
* BANK 2: Bitcrusher
* BANK 3: Daft Punk Octaver
* BANK 4: Better Tremello

* CC-by-www.Electrosmash.com
* Based on OpenMusicLabs previous works.

* Better Tremolo based on the stomp_tremolo.pde from openmusiclabs.com
* This program does a tremolo effect.  It uses a sinewave stored
* in program memory to modulate the signal.  The rate at which
* it goes through the sinewave is set by the push buttons,
* which is min/maxed by the speed value.
* https://www.electrosmash.com/forum/pedalshield-uno/453-7-new-effects-for-the-pedalshield-uno#1718
*/

//defining hardware resources.
#define LED 13
#define FOOTSWITCH 12
#define EFFECTSBUTTON 2
#define PUSHBUTTON_1 A5
#define PUSHBUTTON_2 A4

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
int vol_variable = 10000;
unsigned int speed = 20;          // tremolo speed
unsigned int fractional = 0x00;   // fractional sample position
unsigned int dist_variable = 10;  // octaver

//other variables
const int max_effects = 4;
int data_buffer;  // temporary data storage to give a 1 sample buffer
int input, bit_crush_variable = 0;
unsigned int read_counter = 0;
unsigned int ocr_counter = 0;
unsigned int effect = 1;  // let's start at 1
byte ADC_low, ADC_high;

int buttonState;  //this variable tracks the state of the button, low if not pressed, high if pressed
int lastButtonState = LOW;

long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 50;    // the debounce time; increase if the output flickers

void setup() {
  //setup IO
  pinMode(FOOTSWITCH, INPUT_PULLUP);
  pinMode(EFFECTSBUTTON, INPUT_PULLUP);
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

void swapEffect() {
  static byte effects_btn_memory = 0;
  // Check for keypress
  if (!digitalRead(EFFECTSBUTTON)) {  // Pulled up so zero = pushed.

    delay(100);

    if (!digitalRead(EFFECTSBUTTON)) {  // if it is still pushed after a delay.
      effects_btn_memory = !effects_btn_memory;

      if (effects_btn_memory) {
        digitalWrite(LED, HIGH);
        effect = effect + 1;
        if (effect > max_effects) effect = 1;  // loop back to 1st effect
      } else digitalWrite(LED, LOW);
    }
    while (!digitalRead(EFFECTSBUTTON))
      ;  // wait for low
  }
}

void loop() {

  // Start Loop on Footswitch
  if (digitalRead(FOOTSWITCH)) {
    //Turn on the LED if the effect is ON.
    digitalWrite(LED, HIGH);

    swapEffect();
  }
}

ISR(TIMER1_CAPT_vect) {

  switch (effect) {

    case 1:  // BOOSTER
      // get ADC data
      ADC_low = ADCL;  // you need to fetch the low byte first
      ADC_high = ADCH;
      //construct the input sumple summing the ADC low and high byte.
      input = ((ADC_high << 8) | ADC_low) + 0x8000;  // make a signed 16b value

      //// All the Digital Signal Processing happens here: ////

      read_counter++;  //to save resources, the pushbuttons are checked every 100 times.
      if (read_counter == 100) {
        read_counter = 0;
        if (!digitalRead(PUSHBUTTON_2)) {
          if (vol_variable < 32768) vol_variable = vol_variable + 10;  //increase the vol
          digitalWrite(LED, LOW);                                      //blinks the led
        }

        if (!digitalRead(PUSHBUTTON_1)) {
          if (vol_variable > 0) vol_variable = vol_variable - 10;  //decrease vol
          digitalWrite(LED, LOW);                                  //blinks the led
        }
      }

      //the amplitude of the input signal is modified following the vol_variable value
      input = map(input, -32768, +32768, -vol_variable, vol_variable);

      //write the PWM signal
      OCR1AL = ((input + 0x8000) >> 8);  // convert to unsigned, send out high byte
      OCR1BL = input;                    // send out low byte
      break;

    case 2:  // BITCRUSHER

      read_counter++;  //to save resources, the pushbuttons are checked every 10000 times.
      if (read_counter == 10000) {
        read_counter = 0;
        if (!digitalRead(PUSHBUTTON_2)) {
          if (bit_crush_variable < 16) bit_crush_variable = bit_crush_variable + 1;  //increase the vol
          digitalWrite(LED, LOW);                                                    //blinks the led
        }

        if (!digitalRead(PUSHBUTTON_1)) {
          if (bit_crush_variable > 0) bit_crush_variable = bit_crush_variable - 1;  //decrease vol
          digitalWrite(LED, LOW);                                                   //blinks the led
        }
      }

      // get ADC data
      ADC_low = ADCL;  // you need to fetch the low byte first
      ADC_high = ADCH;
      //construct the input sumple summing the ADC low and high byte.
      input = ((ADC_high << 8) | ADC_low) + 0x8000;  // make a signed 16b value

      //// All the Digital Signal Processing happens here: ////
      //The bit_crush_variable goes from 0 to 16 and the input signal is crushed in the next instruction:
      input = input << bit_crush_variable;

      //write the PWM signal
      OCR1AL = ((input + 0x8000) >> 8);  // convert to unsigned, send out high byte
      OCR1BL = input;                    // send out low byte

      break;

    case 3:  //DAFT PUNK OCTAVER
      read_counter++;
      if (read_counter == 2000) {
        read_counter = 0;
        if (!digitalRead(PUSHBUTTON_1)) {  //increase the tremolo
          if (dist_variable < 500) dist_variable = dist_variable + 1;
          digitalWrite(LED, LOW);  //blinks the led
        }
        if (!digitalRead(PUSHBUTTON_2)) {
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

    case 4:  //BETTER TREMOLO EFFECT

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

      // output the last value calculated
      OCR1AL = ((data_buffer + 0x8000) >> 8);  // convert to unsigned, send out high byte
      OCR1BL = data_buffer;                    // send out low byte

      // get ADC data
      ADC_low = ADCL;  // you need to fetch the low byte first
      ADC_high = ADCH;
      //construct the input sumple summing the ADC low and high byte.
      input = ((ADC_high << 8) | ADC_low) + 0x8000;  // make a signed 16b value

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
  }
}
