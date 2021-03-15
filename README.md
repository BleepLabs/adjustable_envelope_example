### Envelope generator with adjustable shape and DC out for Teensy 4 audio library  
  
  This example is self contained and requires no change to your existing audio library files.   
  
  envelope1.shape(float) controls the shape of attack, decay and release. -1.0 is a sharp exponential curve, 1.0 is a very chonky log.  
  Lengths of the envelope stages and amplitude of sustain is unaffected by shape change.  
    
  envelope1.trigger() works like a standard slope generator. Triggering it will produce just the attack and decay stages. 
  
This works well on the teensy 4.x and only takes about 2% processor on the 4.0 but on the 3.2 it’s over 100% due to all the powf use. Each update of envelope has two calls to fscale and then interpolates between them for the other values in the 8 output batch.   
I attempted to do this on the teensy 3.2 with LUTs that would be interpolated between but it got very complicated and was still taking 10% processor.  
