/*
  Adjustable envelope generator for teensy 4.x
  No controls required in current config. Just upload to hear some nice popcorn

  Now uses a LUT for much less processor usage.
  More info at https://github.com/BleepLabs/adjustable_envelope_example

*/


#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include "envelope_adjustable.h"


// GUItool: begin automatically generated code
AudioSynthWaveform       waveform1;      //xy=430,354
AudioEffectEnvelopeAdjustable      envelope1;      //xy=593,349
AudioOutputI2S           i2s1;           //xy=754,348
AudioConnection          patchCord1(waveform1, envelope1);
AudioConnection          patchCord2(envelope1, 0, i2s1, 0);
AudioConnection          patchCord3(envelope1, 0, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=550,270
// GUItool: end automatically generated code


#include <Bounce2.h>
#define GATE_BUTTON_PIN 37
#define TRIG_BUTTON_PIN 36
Bounce gate_bounce = Bounce();
Bounce trig_bounce = Bounce();
#define BOUNCE_LOCK_OUT

unsigned long current_time, note_timer;
unsigned long prev_time[8];
float attack_time, decay_time, release_time, sustain_level, shape_adj;
int high_note = 75;
int low_note = 35;
byte note_state;

//starts at midi note 12, C0
const float chromatic[121] = {16.3516, 17.32391673, 18.35405043, 19.44543906, 20.60172504, 21.82676736, 23.12465449, 24.499718, 25.95654704, 27.50000365, 29.13523896, 30.86771042, 32.7032, 34.64783346, 36.70810085, 38.89087812, 41.20345007, 43.65353471, 46.24930897, 48.99943599, 51.91309407, 55.00000728, 58.27047791, 61.73542083, 65.40639999, 69.29566692, 73.4162017, 77.78175623, 82.40690014, 87.30706942, 92.49861792, 97.99887197, 103.8261881, 110.0000146, 116.5409558, 123.4708417, 130.8128, 138.5913338, 146.8324034, 155.5635124, 164.8138003, 174.6141388, 184.9972358, 195.9977439, 207.6523763, 220.0000291, 233.0819116, 246.9416833, 261.6255999, 277.1826676, 293.6648067, 311.1270248, 329.6276005, 349.2282776, 369.9944716, 391.9954878, 415.3047525, 440.0000581, 466.1638231, 493.8833665, 523.2511997, 554.3653352, 587.3296134, 622.2540496, 659.2552009, 698.4565551, 739.9889431, 783.9909755, 830.6095048, 880.0001162, 932.3276461, 987.7667329, 1046.502399, 1108.73067, 1174.659227, 1244.508099, 1318.510402, 1396.91311, 1479.977886, 1567.981951, 1661.219009, 1760.000232, 1864.655292, 1975.533466, 2093.004798, 2217.46134, 2349.318453, 2489.016198, 2637.020803, 2793.82622, 2959.955772, 3135.963901, 3322.438019, 3520.000464, 3729.310584, 3951.066931, 4186.009596, 4434.92268, 4698.636906, 4978.032395, 5274.041605, 5587.652439, 5919.911543, 6271.927802, 6644.876037, 7040.000927, 7458.621167, 7902.133861, 8372.019192, 8869.845359, 9397.273811, 9956.06479, 10548.08321, 11175.30488, 11839.82309, 12543.8556, 13289.75207, 14080.00185, 14917.24233, 15804.26772, 16744.03838};


void setup() {
  AudioNoInterrupts();

  gate_bounce.attach(GATE_BUTTON_PIN);
  gate_bounce.interval(5); // interval in ms

  trig_bounce.attach(TRIG_BUTTON_PIN);
  trig_bounce.interval(5); // interval in ms

  pinMode(GATE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIG_BUTTON_PIN, INPUT_PULLUP);
  pinMode(35, INPUT_PULLUP);

  sgtl5000_1.enable();
  AudioMemory(50);
  sgtl5000_1.volume(0.25);

  waveform1.begin(1.0, 220.0, WAVEFORM_SINE);


  envelope1.attack(5);
  envelope1.decay(50);
  envelope1.sustain(.5);
  envelope1.release(1500);
  envelope1.shape(-.9); //change shape of all stages. -1.0 very exponential, 1.0 very log
  // envelope1.attackShape(-.9); //change each one
  // envelope1.decayShape(.3);
  // envelope1.releaseShape(-.3);

  randomSeed(analogRead(A8));
  analogReadResolution(10); //Teensy 4 is 10b
  analogReadAveraging(64);

  AudioInterrupts();

}



void loop() {
  current_time = millis();

  gate_bounce.update();
  trig_bounce.update();

  if (gate_bounce.fell()) {
    waveform1.frequency(chromatic[random(low_note,high_note)]);
    envelope1.noteOn();
  }

  if (gate_bounce.rose()) {
    envelope1.noteOff();
  }

  if (trig_bounce.fell()) {
    waveform1.frequency(chromatic[random(low_note,high_note)]);
    envelope1.trigger();
  }

  //enable to adjust shapes with pots
  if (current_time - prev_time[1] > 10 && 0) {
    prev_time[1] = current_time;
    attack_time = (analogRead(A10) / 1024.0) * 500.0;
    decay_time = (analogRead(A11) / 1024.0) * 2000.0;
    sustain_level = (analogRead(A12) / 1024.0);
    release_time = (analogRead(A13) / 1024.0) * 500.0;

    envelope1.attack(attack_time);
    envelope1.decay(decay_time);
    envelope1.sustain(sustain_level);
    envelope1.release(release_time);

    shape_adj = (analogRead(A14) / 512.0) - 1.0;
    envelope1.shape(shape_adj);

    //envelope1.attackShape((analogRead(A14) / 512.0) - 1.0);
    //envelope1.decayShape((analogRead(A15) / 512.0) - 1.0);
    //envelope1.releaseShape((analogRead(A16) / 512.0) - 1.0);

  }

  //enable for repeating slopes
  if (0) {
    if (current_time - prev_time[5] > (attack_time + decay_time) * 1.1) {
      prev_time[5] = current_time;
      waveform1.frequency(chromatic[random(low_note,high_note)]);
      envelope1.trigger();
    }
  }

  //enable for standard ADSRs gated at a set rate
  if (0) {
    if (current_time - prev_time[4] > attack_time + decay_time + 500 + release_time) {
      prev_time[4] = current_time;
      note_state = 1;
      note_timer = current_time;
      waveform1.frequency(chromatic[random(low_note,high_note)]);
      envelope1.noteOn();
    }

    if (note_state == 1 && current_time - note_timer > attack_time + decay_time + 250) {
      envelope1.noteOff();
      note_state = 0;
    }
  }

  //enable for popcorn
  if (1) {
    if (current_time - prev_time[3] > 150) {
      prev_time[3] = current_time;

      byte r1 = random(15);
      if (r1  > 8) {
        float ra = random(1000) / 100.0;
        float rd = random(1000) / 10.0;
        envelope1.attack(1 + ra);
        envelope1.decay(5 + rd);
        waveform1.frequency(chromatic[random(low_note,high_note)]);
        envelope1.trigger();
      }
    }
  }


  if (current_time - prev_time[0] > 1000 && 1) {
    prev_time[0] = current_time;

    Serial.print(AudioProcessorUsageMax());
    Serial.print("%  ");
    Serial.print(AudioMemoryUsageMax());
    Serial.println();
    Serial.println();
    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();

  }


}
