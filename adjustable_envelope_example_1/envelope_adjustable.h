#include "Arduino.h"
#include "AudioStream.h"
#include "utility/dspinst.h"

#define SAMPLES_PER_MSEC (AUDIO_SAMPLE_RATE_EXACT/1000.0)

class AudioEffectEnvelopeAdjustable : public AudioStream
{
  public:
    AudioEffectEnvelopeAdjustable() : AudioStream(1, inputQueueArray) {
      state = 0;
      delay(0);  // default values...
      attack(10.5f);
      hold(0);
      decay(35.0f);
      sustain(0.5f);
      release(300.0f);
      releaseNoteOn(5.0f);
      shape(0.5f);
    }
    void noteOn();
    void trigger();
    void noteOff();
    void delay(float milliseconds) {
      delay_count = milliseconds2count(milliseconds);
    }
    void attack(float milliseconds) {
      if (milliseconds < min_millis) {
        milliseconds = min_millis;
      }
      attack_count = milliseconds2count(milliseconds);
      if (attack_count == 0) attack_count = 1;
    }
    void hold(float milliseconds) {
      if (milliseconds < .1) {
        milliseconds = .1; //otherwise there are issues w it not doing decay
      }
      hold_count = milliseconds2count(milliseconds);
    }
    void decay(float milliseconds) {
      if (milliseconds < min_millis) {
        milliseconds = min_millis;
      }
      decay_count = milliseconds2count(milliseconds);
      if (decay_count == 0) decay_count = 1;
    }

    void sustain(float level) {
      if (level < 0.0) level = 0;
      else if (level > 1.0) level = 1.0;
      recalc_level = level;
      sus_level = level;

      //keep the sustain level constant and with a linear response across all the expo and log shapes
      //cant use fscale as this needs to output a float
      float curve = decay_shape;
      curve = powf(10, curve * curve_amount * -1.0);
      float flev =  (powf(level, curve));
      sustain_mult = level * 1073741824.0;

    }

    void release(float milliseconds) {
      release_count = milliseconds2count(milliseconds);
      if (release_count == 0) release_count = 1;
    }
    void releaseNoteOn(float milliseconds) {
      release_forced_count = milliseconds2count(milliseconds);
      if (release_count == 0) release_count = 1;
    }

    void shape(float s1) {
      attackShape(s1);
      decayShape(s1);
      releaseShape(s1);
    }

    void attackShape(float s1) {
      if (s1 < -1.0) {
        s1 = -1.0;
      }
      if (s1 > 1.0) {
        s1 = 1.0;
      }
      if (s1 > 0) {
        //the log shape does an odd fade out if it's up too high.
        //This is probably due to the curve_amount being >1.0
        //musically, I think having a sharp exp is may more useful than a bulbus log anyway.
        s1 *= 0.45;
      }
      attack_shape = s1;
    }
    void decayShape(float s1) {
      if (s1 < -1.0) {
        s1 = -1.0;
      }
      if (s1 > 1.0) {
        s1 = 1.0;
      }
      if (s1 > 0) {
        //the log shape does an odd fade out if it's up too high.
        //This is probably due to the curve_amount being >1.0
        //musically, I think having a sharp exp is may more useful than a bulbus log anyway.
        s1 *= 0.45;
      }
      decay_shape = s1;
    }
    void releaseShape(float s1) {
      if (s1 < -1.0) {
        s1 = -1.0;
      }
      if (s1 > 1.0) {
        s1 = 1.0;
      }
      if (s1 > 0) {
        //the log shape does an odd fade out if it's up too high.
        //This is probably due to the curve_amount being >1.0
        //musically, I think having a sharp exp is may more useful than a bulbus log anyway.
        s1 *= 0.45;
      }
      release_shape = s1;
    }
    // right now decay and release are the same due to the weird way I'm maintaining the sustain level across all shapes



    bool isActive();
    bool isSustain();
    using AudioStream::release;
    virtual void update(void);
  private:
    uint16_t milliseconds2count(float milliseconds) {
      if (milliseconds < min_millis) milliseconds = min_millis;
      uint32_t c = ((uint32_t)(milliseconds * SAMPLES_PER_MSEC) + 7) >> 3;
      if (c > 65535) c = 65535; // allow up to 11.88 seconds
      return c;
    }
    //powf is fast! http://openaudio.blogspot.com/2017/02/faster-log10-and-pow.html
    int32_t fscale(float inputValue, float curve) {
      curve = (curve * curve_amount) ;
      curve = powf(10, curve); // convert linear scale into logarithmic exponent for other pow function
      float normalizedCurVal  =  inputValue / 65535.0;   // normalize to 0 - 1 float.
      int32_t rangedValue =  (powf(normalizedCurVal, curve) * 65534.0);
      return (rangedValue);
    }

    audio_block_t *inputQueueArray[1];
    // state
    uint8_t  state, first_rel;     // idle, delay, attack, hold, decay, sustain, release, forced
    uint16_t count;      // how much time remains in this state, in 8 sample units
    int32_t  mult_hires; // attenuation, 0=off, 0x40000000=unity gain
    int32_t  inc_hires;  // amount to change mult_hires every 8 samples

    // settings
    float attack_shape, decay_shape, release_shape, shape_sel;
    uint16_t delay_count;
    uint16_t attack_count;
    uint16_t hold_count;
    uint16_t decay_count;
    int32_t  sustain_mult;
    uint16_t release_count;
    uint16_t release_forced_count;
    uint16_t print_tick;
    int32_t pcnt, rel_begin_curve, mt1;
    float recalc_level, sus_level, atten_level;
    float min_millis = .9;
    float curve_amount = -1.25; // increase this number to get steeper curves
    byte triggerMode;
    byte first_calc;
    int32_t curve, start_curve, end_curve, lerp_curve, lerp_curve2;

};
