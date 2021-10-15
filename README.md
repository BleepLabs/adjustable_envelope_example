### Envelope generator with adjustable shape and DC out for Teensy 4 audio library  

Now uses a LUT for much less processor usage.     
Envelope CV out on out 1.  
  
    
  TODO:  
  Gate input  
  Store LUT in flash. PROGMEM is not working inside the .h for some reason...  
  
  
  ![](https://raw.githubusercontent.com/BleepLabs/adjustable_envelope_example/main/envelope-examples.jpg)   
    
  `envelope1.shape(float)` controls the shape of attack, decay and release. -1.0 is a sharp exponential curve, 1.0 is a very chonky log. The shape of each stage can also be adjusted individually with `attackShape(float)`, `decayShape(float)` and `releaseShape(float)`. 
  
  Lengths of the envelope stages and amplitude of sustain is unaffected by shape change.   
      
  `envelope1.trigger()` works like a standard slope generator. Triggering it will produce the attack and decay stages and then end.  
     
  This example is self contained and requires no change to your existing audio library files.    
    


