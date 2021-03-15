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

void AudioEffectEnvelopeAdjustable::noteOn(void)
{
  __disable_irq();
  triggerMode = 0;
  first_calc = 1;
  if (state == STATE_IDLE || state == STATE_DELAY || release_forced_count == 0) {
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
  } else if (state != STATE_FORCED) {
    state = STATE_FORCED;
    count = release_forced_count;
    inc_hires = (-mult_hires) / (int32_t)count;
  }
  __enable_irq();
}

void AudioEffectEnvelopeAdjustable::trigger(void)
{
  __disable_irq();
  triggerMode = 1;
  first_calc = 1;
  if (state == STATE_IDLE || state == STATE_DELAY || release_forced_count == 0) {
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
  } else if (state != STATE_FORCED) {
    state = STATE_FORCED;
    count = release_forced_count;
    inc_hires = (-mult_hires) / (int32_t)count;
  }

  __enable_irq();
}


void AudioEffectEnvelopeAdjustable::noteOff(void)
{
  __disable_irq();
  if (state != STATE_IDLE && state != STATE_FORCED) {
    state = STATE_RELEASE;
    count = release_count;
    inc_hires = (-mult_hires) / (int32_t)count;
  }
  __enable_irq();
}

void AudioEffectEnvelopeAdjustable::update(void)
{
  audio_block_t *block, *env_block;
  uint32_t *p, *end, *e;
  uint32_t sample12, sample34, sample56, sample78, tmp1, tmp2;
  block = receiveWritable();
  if (!block) return;
  env_block = allocate();

  if (state == STATE_IDLE) {
    release(block);
    release(env_block);

    return;
  }
  p = (uint32_t *)(block->data);
  e = (uint32_t *)(env_block->data);
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
          sustain(recalc_level);
          inc_hires = (sustain_mult - 0x40000000) / (int32_t)count;
        }
        continue;
      } else if (state == STATE_HOLD) {
        state = STATE_DECAY;
        count = decay_count;
        if (triggerMode == 0) {
          sustain(recalc_level);
          inc_hires = (sustain_mult - 0x40000000) / (int32_t)count;
        }
        if (triggerMode == 1) {
          inc_hires = (0 - 0x40000000) / (int32_t)count;
        }
        continue;
      } else if (state == STATE_DECAY) {
        if (triggerMode == 0) {
          sustain(recalc_level);
          state = STATE_SUSTAIN;
          count = 0xFFFF;
          mult_hires = sustain_mult;
          inc_hires = 0;
        }
        if (triggerMode == 1) {
          state = STATE_IDLE;
          while (p < end) {
            *p++ = 0;
            *p++ = 0;
            *p++ = 0;
            *p++ = 0;
          }
          break;
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
      } else if (state == STATE_FORCED) {
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
      } else if (state == STATE_DELAY) {
        state = STATE_ATTACK;
        count = attack_count;
        inc_hires = 0x40000000 / count;
        continue;
      }
    }
    const int32_t top = 65534;
    byte lerpstep = 0;

    int32_t mult = mult_hires >> 14;
    int32_t inc = inc_hires >> 17;
    // process 8 samples, using only mult and inc (16 bit resolution)
    sample12 = *p++;
    sample34 = *p++;
    sample56 = *p++;
    sample78 = *p++;
    p -= 4;

    if (state == STATE_ATTACK) { //it's just easier to do this here rather than in count==0
      shape_sel = attack_shape;
    }
    if (state == STATE_DECAY) {
      shape_sel = decay_shape;
    }
    if (state == STATE_RELEASE) {
      shape_sel = release_shape;
    }

    if (first_calc == 1) {
      //you could only calculate one per update but it doesn't save much and you still need to calc both on the first go around
      //start_curve = fscale(mult, shape_sel);
      first_calc = 0;
    }
    else {
      //start_curve = end_curve;
    }

    start_curve = fscale(mult, shape_sel);
    end_curve = fscale(mult + (inc * 7), shape_sel);

    tmp1 = signed_multiply_32x16b(start_curve, sample12);
    lerpstep++;
    lerp_curve2 = start_curve + (((end_curve - start_curve) * lerpstep) >> 3);
    tmp2 = signed_multiply_32x16t(lerp_curve2, sample12);
    sample12 = pack_16b_16b(tmp2, tmp1);
    *e++ = pack_16b_16b(lerp_curve2 >> 1, start_curve >> 1);

    lerp_curve = start_curve + (((end_curve - start_curve) * lerpstep) >> 3);
    lerpstep++;
    tmp1 = signed_multiply_32x16b(lerp_curve, sample34);
    lerp_curve2 = start_curve + (((end_curve - start_curve) * lerpstep) >> 3);
    lerpstep++;
    tmp2 = signed_multiply_32x16t(lerp_curve2, sample34);
    sample34 = pack_16b_16b(tmp2, tmp1);
    *e++ = pack_16b_16b(lerp_curve2 >> 1, lerp_curve >> 1);

    lerp_curve = start_curve + (((end_curve - start_curve) * lerpstep) >> 3);
    lerpstep++;
    tmp1 = signed_multiply_32x16b(lerp_curve, sample56);
    lerp_curve2 = start_curve + (((end_curve - start_curve) * lerpstep) >> 3);
    lerpstep++;
    tmp2 = signed_multiply_32x16t(lerp_curve2, sample56);
    sample56 = pack_16b_16b(tmp2, tmp1);
    *e++ = pack_16b_16b(lerp_curve2 >> 1, lerp_curve >> 1);

    lerp_curve = start_curve + (((end_curve - start_curve) * lerpstep) >> 3);
    lerpstep++;
    tmp1 = signed_multiply_32x16b(lerp_curve, sample78);
    tmp2 = signed_multiply_32x16t(end_curve, sample78);
    sample78 = pack_16b_16b(tmp2, tmp1);
    *e++ = pack_16b_16b(end_curve >> 1, lerp_curve >> 1);

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
  if (env_block) {
    transmit(env_block, 1);
    release(env_block);
  }
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
