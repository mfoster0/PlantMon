//ESP.deepSleep() requires RST to Pin 16 connection

const int buzzer = 14; //buzzer pin

void setup(){
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output
}

void loop(){
  tone(buzzer, 25); // Send 25KHz sounds like a bird!!
  delay(150);        
  noTone(buzzer);     // Stop buzzer
  //LowPower.sleep(10000);        //
  ESP.deepSleep(10e6);

}
