### Envelope generator with adjustable shape and DC out for Teensy 4 audio library  

Now useses a LUT for much less processor usage.   
  
  TODO:  
  CV output  
  Gate input  
  
  
  ![](https://raw.githubusercontent.com/BleepLabs/adjustable_envelope_example/main/envelope-examples.jpg)   
    
  `envelope1.shape(float)` controls the shape of attack, decay and release. -1.0 is a sharp exponential curve, 1.0 is a very chonky log. The shape of each stage can also be adjusted individually with `attackShape(float)`, `decayShape(float)` and `releaseShape(float)`. 
  
  Lengths of the envelope stages and amplitude of sustain is unaffected by shape change.   
      
  `envelope1.trigger()` works like a standard slope generator. Triggering it will produce the attack and decay stages and then end.  
     
  This example is self contained and requires no change to your existing audio library files.    
    


