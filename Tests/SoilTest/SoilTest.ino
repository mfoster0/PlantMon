uint8_t soilPin = 0;   //**one nail goes to +5V, the other nail goes to this analogue pin
int moisture_val;

int soilSenVCC = 13; //use digital pin 13
int counter = 0;

void setup() {
  Serial.begin(115200);     //open serial port
  pinMode(soilSenVCC, OUTPUT);
  digitalWrite(soilSenVCC, LOW); //initialising in off mode
}

void loop() {
  counter++;
  if(counter> 10){      // change this value to set "not powered" time. higher number bigger gap
    // power the sensor
    digitalWrite(soilSenVCC, HIGH);
    //delay here probably to allow the circuit to fully power and stablise
    delay(1000); 

    // read the value from the sensor:
    moisture_val = analogRead(soilPin);   // read the resistance      
    //stop power
    digitalWrite(soilSenVCC, LOW);  
    delay(100);
    counter=0;    
  }  
  //wait
  Serial.print("sensor = " );                       
  Serial.println(moisture_val);     
  delay(500);
}