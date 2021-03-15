/*
  adjustable envelope generator for teensy 4.x
  no controls required in current config. Just uplaod to hear some nice popcorn

  Only uses about 2% processor on 4.0 but is over 100% on Teensy 3.2 due to lots of powf
  More info at

*/


#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include "envelope_adjustable.h"

// GUItool: begin automatically generated code
AudioSynthWaveform       waveform1;      //xy=172,262
AudioSynthNoiseWhite     noise1;         //xy=187,465
AudioSynthWaveform       waveform2;      //xy=212,538
AudioSynthWaveform       waveform3;      //xy=265,579
AudioEffectEnvelopeAdjustable      envelope1;      //xy=335,311
AudioFilterStateVariable filter1;        //xy=345,430
AudioAnalyzePeak         peak2;          //xy=390,531
AudioAnalyzePeak         peak3;          //xy=407,579
AudioMixer4              mixer1;         //xy=505,373
AudioAnalyzeRMS          peak1;          //xy=508,196
AudioOutputI2S           i2s1;           //xy=708,312
AudioConnection          patchCord1(waveform1, envelope1);
AudioConnection          patchCord2(noise1, 0, filter1, 0);
AudioConnection          patchCord3(waveform2, peak2);
AudioConnection          patchCord4(waveform3, peak3);
AudioConnection          patchCord5(envelope1, 0, mixer1, 0);
AudioConnection          patchCord6(filter1, 0, mixer1, 1);
AudioConnection          patchCord7(mixer1, 0, i2s1, 0);
AudioConnection          patchCord8(mixer1, 0, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=686,193
// GUItool: end automatically generated code

//DC out from envelope
AudioConnection          patchCord31(envelope1, 1, peak1, 0);
AudioConnection          patchCord32(envelope1, 1, filter1, 1);

#include <Bounce2.h>
#define GATE_BUTTON_PIN 2
#define TRIG_BUTTON_PIN 3
Bounce gate_bounce = Bounce();
Bounce trig_bounce = Bounce();
#define BOUNCE_LOCK_OUT

unsigned long current_time, note_timer;
unsigned long prev_time[8];
uint16_t lfo, lfo_count;

float attack_time, decay_time, release_time, sustain_level, shape_adj;

byte note_state;
const float chromatic[88] = {55.00000728, 58.27047791, 61.73542083, 65.40639999, 69.29566692, 73.4162017, 77.78175623, 82.40690014, 87.30706942, 92.49861792, 97.99887197, 103.8261881, 110.0000146, 116.5409558, 123.4708417, 130.8128, 138.5913338, 146.8324034, 155.5635124, 164.8138003, 174.6141388, 184.9972358, 195.9977439, 207.6523763, 220.0000291, 233.0819116, 246.9416833, 261.6255999, 277.1826676, 293.6648067, 311.1270248, 329.6276005, 349.2282776, 369.9944716, 391.9954878, 415.3047525, 440.0000581, 466.1638231, 493.8833665, 523.2511997, 554.3653352, 587.3296134, 622.2540496, 659.2552009, 698.4565551, 739.9889431, 783.9909755, 830.6095048, 880.0001162, 932.3276461, 987.7667329, 1046.502399, 1108.73067, 1174.659227, 1244.508099, 1318.510402, 1396.91311, 1479.977886, 1567.981951, 1661.219009, 1760.000232, 1864.655292, 1975.533466, 2093.004798, 2217.46134, 2349.318453, 2489.016198, 2637.020803, 2793.82622, 2959.955772, 3135.963901, 3322.438019, 3520.000464, 3729.310584, 3951.066931, 4186.009596, 4434.92268, 4698.636906, 4978.032395, 5274.041605, 5587.652439, 5919.911543, 6271.927802, 6644.876037, 7040.000927, 7458.621167, 7902.133861, 8372.019192};

void setup() {

  gate_bounce.attach(GATE_BUTTON_PIN);
  gate_bounce.interval(5); // interval in ms

  trig_bounce.attach(TRIG_BUTTON_PIN);
  trig_bounce.interval(5); // interval in ms

  sgtl5000_1.enable();
  AudioMemory(50);
  sgtl5000_1.volume(0.75);

  waveform1.begin(1.0, 440.0, WAVEFORM_SINE);
  waveform2.begin(1.0, .20, WAVEFORM_SINE);
  waveform3.begin(1.0, .06, WAVEFORM_SINE);

  mixer1.gain(0, .7); //sine wave
  mixer1.gain(1, .3); //noise info filter

  noise1.amplitude(1);

  envelope1.attack(10);
  envelope1.decay(100);
  envelope1.sustain(.4);
  envelope1.release(750);
  envelope1.shape(-.9); //change shape of all stages. -1.0 very expoential, 1.0 very log
  //envelope1.attackShape(-.9);
  //envelope1.decayShape(.3); //decay and release. Release isn't seperate right now

  filter1.frequency(0);
  filter1.octaveControl(8);

  analogReadResolution(10); //Teensy 4 is 10b
  analogReadAveraging(64);

}


void loop() {

  current_time = millis();

  gate_bounce.update();
  trig_bounce.update();

  if (gate_bounce.fell()) {
    waveform1.frequency(chromatic[random(20, 50)]);
    envelope1.noteOn();
  }

  if (gate_bounce.rose()) {
    envelope1.noteOff();
  }

  if (trig_bounce.fell()) {
    waveform1.frequency(chromatic[random(20, 50)]);
    envelope1.trigger();
  }

  //enable to adjust shapes with pots
  if (current_time - prev_time[1] > 10 && 0) {
    prev_time[1] = current_time;
    attack_time = (analogRead(A0) / 1024.0) * 500.0;
    decay_time = (analogRead(A1) / 1024.0) * 500.0;
    sustain_level = (analogRead(A2) / 1024.0);
    release_time = (analogRead(A3) / 1024.0) * 1000.0;
    shape_adj = (analogRead(A8) / 1024.0);
    envelope1.attack(attack_time);
    envelope1.decay(decay_time);
    envelope1.sustain(sustain_level);
    envelope1.release(release_time);
    envelope1.shape(shape_adj);
  }

  //enable for standard ADSRs gated at a set rate
  if (0) {
    if (current_time - prev_time[4] > 2000) {
      prev_time[4] = current_time;
      note_state = 1;
      note_timer = current_time;
      waveform1.frequency(chromatic[random(20, 50)]);
      envelope1.noteOn();
    }

    if (note_state == 1 && current_time - note_timer > 1000) {
      envelope1.noteOff();
      note_state = 0;
    }
  }

  //enable for slopes, just the attack and decay, triggered randomly
  if (current_time - prev_time[3] > 150 && 1) {
    prev_time[3] = current_time;
    byte r1 = random(15);

    if (r1  > 8) {
      waveform1.frequency(chromatic[random(20, 50)]);
      envelope1.trigger();
    }

  }

  //modulate the decay and attack times
  if (1) {
    prev_time[5] = current_time;
    if (peak2.available()) {
      float lfo1 = (peak2.read() * 10.0) + 5;
      envelope1.attack(lfo1);
    }
    if (peak3.available()) {
      float lfo2 = (peak3.read() * 1000.0);
      envelope1.decay(lfo2);
    }
  }

  //print audio usages
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
