### Envelope generator with adjustable shape and DC out for Teensy 4 audio library  
  
  ![](https://raw.githubusercontent.com/BleepLabs/adjustable_envelope_example/main/envelope-examples.jpg)   
    
  `envelope1.shape(float)` controls the shape of attack, decay and release. -1.0 is a sharp exponential curve, 1.0 is a very chonky log. The shape of each stage can also be adjusted individually with `attackShape(float)`, `decayShape(float)` and `releaseShape(float)`. 
  
  Lengths of the envelope stages and amplitude of sustain is unaffected by shape change.   
      
  `envelope1.trigger()` works like a standard slope generator. Triggering it will produce the attack and decay stages and then end.  
     
  This example is self contained and requires no change to your existing audio library files.    
    
This object has the same in and out as the standard envelope but adds a DC output that is jsut the envelope level. Right now something must be goign into the input for it to work, though.  
  
This works well on the teensy 4.x and only takes about 2% processor on the 4.0 but on the 3.2 it’s over 100% due to all the powf use. Each update of envelope has two calls to fscale and then interpolates between them for the other values in the 8 output batch.   
If there's demand I'll mke a LUT version of it for the 3.x



