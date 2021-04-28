#include <Arduino.h>
#include "envelope_adjustable.h"

#define STATE_IDLE  0
#define STATE_DELAY 1
#define STATE_ATTACK  2
#define STATE_HOLD  3
#define STATE_DECAY 4
#define STATE_SUSTAIN 5
#define STATE_RELEASE 6
#define STATE_FORCED  7



void AudioEffectEnvelopeAdjustable::trigger(void)
{
  __disable_irq();
  triggerMode = 1;
  first_calc = 1;
  mult_hires = 0;
  count = delay_count;
  if (count > 0) {
    state = STATE_DELAY;
    inc_hires = 0;
  } else {
    state = STATE_ATTACK;
    count = attack_count;
    inc_hires = 0x40000000 / (int32_t)count;
  }
  __enable_irq();
}


void AudioEffectEnvelopeAdjustable::noteOn(void)
{
  __disable_irq();
  if (state == STATE_IDLE || state == STATE_DELAY || release_forced_count == 0) {
    mult_hires = 0;
    triggerMode = 0;
    count = delay_count;
    if (count > 0) {
      state = STATE_DELAY;
      inc_hires = 0;
    } else {
      state = STATE_ATTACK;
      count = attack_count;
      inc_hires = 0x40000000 / (int32_t)count;
    }
  }

  else {
    triggerMode = 0;
    int32_t tc;

    //this only works for the same attack and decay shapes meeting
    //you need to gifure out where end curve will meet start curve without messign with time?
    //well clip fixes that
    //well that's jsut for attack to decay? rel messes up too
    uint16_t match_mult, prev_match_mult, mtest, pmtest;
    //uint32_t cu = micros();
    for (int i; i < 128; i++) {
      pmtest = mtest;
      mtest = lerpLUT(i << 9, attack_shape);
      if ( pmtest <= end_curve && mtest >= end_curve) {
        match_mult = i << 9;
        break;
      }
    }
    //uint32_t du = micros() - cu; //takes 35 micros when its a fast retrig @ 127 which seems to work fine?
    mult_hires = match_mult << 14;
    state = STATE_ATTACK;
    count = attack_count;
    inc_hires = 0x40000000 / (int32_t)count;
  }

  __enable_irq();
}

void AudioEffectEnvelopeAdjustable::noteOff(void)
{
  __disable_irq();
  if (state != STATE_IDLE && state != STATE_FORCED) {
    triggerMode = 0;
    state = STATE_RELEASE;
    count = release_count;
    mult_hires = 0x40000000;
    first_rel = 1;
    inc_hires = (0 - 0x40000000) / (int32_t)count; //always do it the full range then put sus in later  }
    __enable_irq();
  }
  __enable_irq();
}

void AudioEffectEnvelopeAdjustable::update(void)
{
  audio_block_t *block;
  uint32_t *p, *end;
  uint32_t sample12, sample34, sample56, sample78, tmp1, tmp2;

  block = receiveWritable();
  if (!block) return;
  if (state == STATE_IDLE) {
    release(block);
    return;
  }
  p = (uint32_t *)(block->data);
  end = p + AUDIO_BLOCK_SAMPLES / 2;

  while (p < end) {
    // we only care about the state when completing a region
    if (count == 0) {
      if (state == STATE_ATTACK) {
        count = hold_count;
        if (count > 0) {
          state = STATE_HOLD;
          mult_hires = 0x40000000;
          inc_hires = 0;
        } else {
          state = STATE_DECAY;
          count = decay_count;
          inc_hires = (sustain_mult - 0x40000000) / (int32_t)count;
        }
        continue;
      } else if (state == STATE_HOLD) {
        state = STATE_DECAY;
        count = decay_count;
        inc_hires = (0 - 0x40000000) / (int32_t)count; //go though the whole range then put sus level in later

        continue;
      } else if (state == STATE_DECAY) {
        if (triggerMode == 1) {
          triggerMode = 0;
          state = STATE_IDLE;
          while (p < end) {
            *p++ = 0;
            *p++ = 0;
            *p++ = 0;
            *p++ = 0;
          }
          break;
        }
        else {
          state = STATE_SUSTAIN;
          count = 0xFFFF;
          mult_hires = sustain_mult;
          inc_hires = 0;
        }
      } else if (state == STATE_SUSTAIN) {
        count = 0xFFFF;
      } else if (state == STATE_RELEASE) {
        state = STATE_IDLE;
        while (p < end) {
          *p++ = 0;
          *p++ = 0;
          *p++ = 0;
          *p++ = 0;
        }
        break;
      } else if (state == STATE_DELAY) {
        state = STATE_ATTACK;
        count = attack_count;
        inc_hires = 0x40000000 / count;
        continue;
      }
    }

    mult = mult_hires >> 14;
    inc = inc_hires >> 17;

    int32_t end_mult = mult + (inc * 7);
    if (end_mult > 65534) {
      end_mult = 65534;
    }
    if (end_mult < 1) {
      end_mult = 0;
    }

    byte clip = 0;
    if (mult > 65534) {
      mult = 65534;
      clip = 1;
    }

    if (state == STATE_ATTACK) {
      if (clip == 1) {
        count = hold_count;
        if (count > 0) {
          state = STATE_HOLD;
          mult_hires = 0x40000000;
          inc_hires = 0;
        } else {
          state = STATE_DECAY;
          count = decay_count;
          inc_hires = (sustain_mult - 0x40000000) / (int32_t)count;
        }

        start_curve = mult;
        end_curve = mult;
      }

      else {
        shape_sel = attack_shape;

        if (calc_mode == 0) { //leftover from previous version. Still have it here for testing
          start_curve = fscale(mult, shape_sel);
          end_curve = fscale(end_mult, shape_sel);
        }
        if (calc_mode == 1) {
          start_curve = lerpLUT(mult, shape_sel);
          end_curve = lerpLUT(end_mult, shape_sel);
        }

      }
    }

    else if (state == STATE_DECAY) {
      shape_sel = decay_shape;
      if (triggerMode == 0) {
        if (calc_mode == 0) {
          start_curve = (fscale(mult, shape_sel) * (1.0 - sus_level)) + (65534.0 * sus_level);
          end_curve = (fscale(end_mult, shape_sel) * (1.0 - sus_level)) + (65534.0 * sus_level);
        }
        if (calc_mode == 1) {
          start_curve = (lerpLUT(mult, shape_sel) * (1.0 - sus_level)) + (65534.0 * sus_level);
          end_curve = (lerpLUT(end_mult, shape_sel) * (1.0 - sus_level)) + (65534.0 * sus_level);
        }
      }
      else {
        if (calc_mode == 0) {
          start_curve = fscale(mult, shape_sel);
          end_curve = fscale(end_mult, shape_sel);
        }
        if (calc_mode == 1) {
          start_curve = lerpLUT(mult, shape_sel);
          end_curve = lerpLUT(end_mult, shape_sel);
          retrig_mult = end_mult;
        }
      }
    }

    else if (state == STATE_RELEASE) {
      if (first_rel == 1) {
        first_rel = 0;
        atten_level = float(start_curve) / 65534.0;
      }
      shape_sel = release_shape;
      if (calc_mode == 0) {
        start_curve = fscale(mult, shape_sel) * atten_level;
        end_curve = fscale(end_mult, shape_sel) * atten_level;
      }
      if (calc_mode == 1) {
        start_curve = lerpLUT(mult, shape_sel) * atten_level;
        end_curve = lerpLUT(end_mult, shape_sel) * atten_level;
      }
    }
    else {
      //maintain the last curve
    }

    prev_shape_sel = shape_sel;

    if (start_curve > 65535) {
      start_curve = 65535;
    }
    if (end_curve > 65535) {
      end_curve = 65535;
    }

    lerpsteps[0] = start_curve;
    lerpsteps[7] = end_curve;

    for (int i = 1; i < 7; i++) {
      if (state == STATE_ATTACK) {
        step_size = (end_curve - start_curve) / 8.0;
        lerpsteps[i] = start_curve + (step_size * i);
      }

      else if (state == STATE_HOLD) {
        lerpsteps[0] = mult;
        lerpsteps[7] = mult;
        lerpsteps[i] = mult;
      }

      else if (state == STATE_SUSTAIN) {
        lerpsteps[0] = start_curve;
        lerpsteps[7] = start_curve;
        lerpsteps[i] = start_curve;
      }

      else {
        step_size = (start_curve - end_curve) / 8.0;
        lerpsteps[i] = start_curve - (step_size * i);
      }

    }

    print_tick++;
    if (print_tick > 25 && 0) {
      print_tick = 0;
      //Serial.print(start_curve);
      //Serial.print(" ");
      Serial.print(end_curve);
      Serial.print(" ");
      Serial.println(mult);
    }

    // process 8 samples, using only mult and inc (16 bit resolution)
    sample12 = *p++;
    sample34 = *p++;
    sample56 = *p++;
    sample78 = *p++;
    p -= 4;
    mult += (inc * 8);

    tmp1 = signed_multiply_32x16b(lerpsteps[0], sample12);
    tmp2 = signed_multiply_32x16t(lerpsteps[1], sample12);
    sample12 = pack_16b_16b(tmp2, tmp1);
    tmp1 = signed_multiply_32x16b(lerpsteps[2], sample34);
    tmp2 = signed_multiply_32x16t(lerpsteps[3], sample34);
    sample34 = pack_16b_16b(tmp2, tmp1);
    tmp1 = signed_multiply_32x16b(lerpsteps[4], sample56);
    tmp2 = signed_multiply_32x16t(lerpsteps[5], sample56);
    sample56 = pack_16b_16b(tmp2, tmp1);
    tmp1 = signed_multiply_32x16b(lerpsteps[6], sample78);
    tmp2 = signed_multiply_32x16t(lerpsteps[7], sample78);
    sample78 = pack_16b_16b(tmp2, tmp1);
    *p++ = sample12;
    *p++ = sample34;
    *p++ = sample56;
    *p++ = sample78;

    // adjust the long-term gain using 30 bit resolution (fix #102)
    // https://github.com/PaulStoffregen/Audio/issues/102
    mult_hires += inc_hires;
    count--;
  }
  transmit(block);
  release(block);
}

bool AudioEffectEnvelopeAdjustable::isActive()
{
  uint8_t current_state = *(volatile uint8_t *)&state;
  if (current_state == STATE_IDLE) return false;
  return true;
}

bool AudioEffectEnvelopeAdjustable::isSustain()
{
  uint8_t current_state = *(volatile uint8_t *)&state;
  if (current_state == STATE_SUSTAIN) return true;
  return false;
}
