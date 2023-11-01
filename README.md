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

TODO:

Debug soil sensor readings - the soil moisture script is working
Add error handlers
Warnings / errors - publish to mqtt
Add checksum - use simple cipher so that consumers of data know this is genuine
Add tamper checks - look at ways to analyse readings to identify potential tamper events
Enable the remote control for switching device on/off:
  - via webpage
  - via mqtt
  - add on/off for each function
Use LED's for physical monitoring and heartbeat
Add sound alert to support non-visual alert
Set up active remote monitoring
Automatically attempt to connect to back up WiFi connection if primary fails
Move from delay to millis
Update pulse logic to show a less frequent heartbeat

Webpage:
  Add colour highlighting for values out of accepted ranges
  Add start/stop functionality (possibly dropdown or radio buttons)
  Clean up room and device ids
  Possibly make realtime




