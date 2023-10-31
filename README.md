# Plant Health Monitor
## Overview
 Arduino Plant Monitor - this plant monitor is set up to take reading of soil moisture, air temperature and# humidty levels. It does this through dedicated hardware (ESP8266 with a DHT and a homemade basic steel nail resistance monitor). The ESP's onboard WiFi adapter is used to connect to a network. Readings from the sensors are posted to a remote MQTT server. The core of the project is based on content from UCL's Connected Environments module within the Connected Environments MSc Programme.
 .... 
 ## Details of the circuit used to connect to the sensors

 The code in the "PlantMon.ino" is loaded into the ESP device that is the microcontroller in this circuit.
 .....Add desc and diagrams

 ## The ESP code
The code is structure to perform x core tasks split between 3 main areas:
### On Start up

### Continual Execution

### On Callback Event





