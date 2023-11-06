/*
Arduino Plant Monitor

This monitor is a physical device that is located next to the plant and monitoring the conditions the plant is currently exposed to.
It does this through various sensors, initially this is through:
 - soil moisture detector, via steel nails registering the resistance of the soil, the less moisture in the soil, the higher the insulation (resistance).
 - air humidity and temperature through a DHT22

Readings are posted to an MQTT server for remote monitoring
For local/physical monitoring light and sound are used so that in the event of a connection issue with the server the monitor can still function
and health can be highlighted locally.

Arduino published messages are sent to the mqtt server and topics defined in the ./hidden/hidden_config.h file
Connection logins are stored in the ./hidden/arduino_secrets.h file
Remote control of the device is performed through mqtt and the webpage

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
- Debug soil sensor readings - the soil moisture script is working
- Add error handlers
- Warnings / errors - publish to mqtt
- Add checksum - use simple cipher so that consumers of data know this is genuine
- Add tamper checks - look at ways to analyse readings to identify potential tamper events
- Enable the remote control for switching device on/off:
  -- via webpage
  -- via mqtt
  -- add on/off for each function
- Use LED's for physical monitoring and heartbeat
- Add sound alert to support non-visual alert
- Set up active remote monitoring
- Automatically attempt to connect to back up WiFi connection if primary fails
- Move from delay to millis
- Update pulse logic to show a less frequent heartbeat

Webpage:
  Add colour highlighting for values out of accepted ranges
  Add start/stop functionality (possibly dropdown or radio buttons)
  Clean up room and device ids
  Possibly make realtime
-   Add colour highlighting for values out of accepted ranges
-   Add start/stop functionality (possibly dropdown or radio buttons)
-   Clean up room and device ids
-   Possibly make realtime
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ezTime.h>
#include <string>
#include <iostream>
#include <cstring>
#include <DHT.h>
#include <DHT_U.h>

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

//locally stored logins and config
#include "./arduino_secrets.h" 
#include "./hidden_config.h" //conatains paths for topics, 

//pull in the logins and config
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* ssid_bu = SECRET_SSID_BU;
const char* password_bu = SECRET_PASS_BU;
const char* mqttuser = SECRET_MQTTUSER;
const char* mqttpass = SECRET_MQTTPASS;

//#######################################
//the below are pulled from the hidden_config.h
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const int web_server_port = WEB_SERV_PORT;
const char* healthTopic = HEALTH_TOPIC_ID; //used to send system health separate to readings
const char* errTopic = ERROR_TOPIC_ID; //used to send errors separate to readings
const char* topicBase = TOPIC_BASE; //this is the main/higher part of topics that will be  the same for all subscriptions
const char* roomId = ROOM_ID; //CE Lab
const char* plantId = PLANT_ID; //ID of individual plant
const int interval = MINIMUM_INTERVAL; //millis to wait between events
const int pubIntervals = PUBLISH_INTERVALS; //number of intervals before publishing update
const char* remoteControl = REMOTE_CONTROL_TOPIC; //topic used to communicate back to the arduino through MQTT

// Set up for Sensors - DHT22 and Nails
uint8_t DHTPin = 12;        // on Pin 12 of the Huzzah
uint8_t soilPin = 0;      // ADC or A0 pin on Huzzah
float Temperature;
float Humidity;
long Moisture = 1; // initial value just in case web page is loaded before readMoisture called
int sensorVCC = 13; //VCC pin 13
int blueLED = 2; 
int minMoist = MIN_MOIST; //use config for range bounds
int maxMoist = MAX_MOIST; //use config for range bounds
int minMoistRng = MIN_MOIST_RNG; //use config for range bounds
int maxMoistRng = MAX_MOIST_RNG; //use config for range bounds

DHT dht(DHTPin, DHTTYPE);   // Initialize DHT sensor.

bool ledOn = false;

int elapsedIntervals = 0;

//#####################################################################
//Making code more config driven so changes only need to be made to the associated config file and not the main code
String plantTopic = String(topicBase) + roomId + plantId;

//declare the clients - wifi & the pubsub
WiFiClient espClient;
PubSubClient client(espClient); //used for mqtt communication
ESP8266WebServer server(web_server_port);

long lastMsg = 0;
char msg[50];
int value = 0;

// Date and time
//Timezone GB; //##############understand GB usage before switching from UTC

void setup() {
  //### ensure baud rate is correct
  Serial.begin(115200);
  delay(100);

  //#############################################################
  //Yet to configure the built in LED's for production operation. These will reflect the status of the device and sensors
  //set the built in LED as an output
  pinMode(LED_BUILTIN,OUTPUT);
  ledOn = false;
  digitalWrite(LED_BUILTIN, LOW); //switch off LED

  // start DHT sensor
  pinMode(DHTPin, INPUT);
  dht.begin();

  //set up mqtt connection
  client.setServer(mqtt_server,mqtt_port);
  //set up callback function to be trigger on mqtt update for a subcribed topic
  client.setCallback(callback);

  connectToWiFi();

  //print IP of device
  Serial.println("");
  Serial.println("Wifi connected"),
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  startWebserver();
  syncDate();

}
void connectToWiFi(){
  bool usingBU = false;
  char* usingSSID = ssid;
  char* usingPwd = password

  //connect to wifi
  Serial.println();
  Serial.println("Connecting to ");
  Serial.print(usingSSID);
  
  WiFi.begin(usingSSID, usingPwd);
  
  int count = 0;
  //wait for wifi connection - use back up if primary not available
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
    if (count > 30){
      usingBU != usingBU;
      
      //get the correct ssid and pwd
      if (usingBU){
        usingSSID = ssid_bu;
        usingPwd = password_bu;  
      } else {
        usingSSID = ssid;
        usingPwd = password;     
      }

      //connect to wifi
      Serial.println();
      Serial.println("Connecting to ");
      Serial.print(usingSSID);
      WiFi.begin(usingSSID, usingPwd);   
      
      count = 0;
    }
  
    count++;
  }
}

//#####################################################################
// continuous execution loop for device
//#####################################################################
void loop() {
  // handler for receiving requests to webserver
  server.handleClient();

  //currently using delay as a timer
  //this will be switched to millis and sync with ezTime
  // increment timer
  elapsedIntervals++;
  
  //check if any actions need performing
  processDueActions();

  //wait for defined interval
  delay(interval);
}

//#####################################################################
// check if if it is time to send data to mqtt, if so process current readings
// so that the device is not in a "delay" state for too long, a counter is used to allow loop cycles continue
// consider switching to millis as a method of waiting
//#####################################################################
void processDueActions() {
 
  //exit if interval count threshold has not been reached
  if (elapsedIntervals < pubIntervals  ){
    return;
  }

  //process actions 
  //use UTC until read up on any issues with GB
  Serial.println(UTC.dateTime("l, d-M-y H:i:s.v T")); // GB.dateTime("H:i:s")

  //#######################################################
  //reading moisture first because this reading takes longer
  //get soil reading
  readMoisture();

  Temperature = dht.readTemperature(); // Gets the values of the temperature
  Humidity = dht.readHumidity(); // Gets the values of the humidity
  
  //send readings to broker
  sendMQTT();

  //reset the interval count
  elapsedIntervals = 0;
  
  //#################################################################
  //Pulse to be used in finished version to show that the device is still active
  //show pulse
  if (ledOn){
      digitalWrite(LED_BUILTIN, HIGH); //off
    } else {
      digitalWrite(LED_BUILTIN, LOW); //on
    }
    ledOn = !ledOn;  
}

//#######################################################
// read data from nail sensors
//#######################################################
void readMoisture(){
  int soilReading = -1;
  int mappedSoilReading = -1;
  // power the sensor
  digitalWrite(sensorVCC, HIGH);
  digitalWrite(blueLED, LOW);
  delay(100);
  // read the value from the sensor:          
  Moisture = analogRead(soilPin);

  Serial.print("Soil: ");
  Serial.println(Moisture);
  /*
  //map to small range due to such inprecise readings. min value
  //min observed value: 8, max: 1024 ==> Min: 0, Max:10      
  Moisture = map(soilReading, minMoist, maxMoist, minMoistRng, maxMoistRng); 
  //Moisture = map(soilReading, 0,1025, 1, 10); 
  Serial.print("Mapped Soil: ");
  Serial.println(Moisture);
  //Moisture = analogRead(soilPin);
  */
  //stop power
  digitalWrite(sensorVCC, LOW);  
  digitalWrite(blueLED, HIGH);
  delay(100);
 
  ////Serial.println(soilReading);   // read the value from the nails

}

//#######################################################
// start webserver used for processing http requests received
//#######################################################
void startWebserver() {
  // when connected and IP address obtained start HTTP server
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");
}

//#######################################################
// establish current date and time from ezTime
void syncDate() {
  // get real date and time from ezTime
  waitForSync();
  Serial.println("UTC: " + UTC.dateTime());
  //GB.setLocation("Europe/London");
  //Serial.println("London time: " + GB.dateTime());
}

void sendMQTT(){
  //check if connection to MQTT is active
  if (!client.connected()){
    reconnect();
  }
  
  //#######################################################
  //keep connection to mqtt alive
  client.loop();
  ++value;



  //#######################################################
  // publish current readings through mqtt pubsub client
  //Serial.print("Publishing moisture val: ");
  //Serial.println(Moisture);
  snprintf (msg, 50, "%.0i", Moisture);
  Serial.print("Publish message for m: ");
  Serial.println(msg);
  client.publish((plantTopic + "moisture").c_str(), msg);

  //Temperature
  snprintf (msg, 50, "%.1f", Temperature);
  Serial.print("Publish message for t: ");
  Serial.println(msg);
  client.publish((plantTopic + "temperature").c_str(), msg);

  //Humidity
  snprintf (msg, 50, "%.0f", Humidity);
  Serial.print("Publish message for h: ");
  Serial.println(msg);
  client.publish((plantTopic + "humidity").c_str(), msg);

  //Publish device time at send. opting for iso8601 as the format is clear
  String sendTime = UTC.dateTime(ISO8601);
  Serial.print("Publish message for time: ");
  Serial.println(sendTime);
  client.publish((plantTopic + "time").c_str(), sendTime.c_str());

  //Publish check value (cipher) for identify values not from this device
  String codedText = codeString(sendTime);
  Serial.print("Publish message for check: ");
  Serial.println(msg);
  client.publish((plantTopic + "check").c_str(), codedText.c_str());
}

//########################################################
// placeholder for simple cipher function for checksum
//########################################################
String codeString(String str){
  return "foo";
}

//#####################################################################
// establish mqtt connection and subscribe for topics as required
//#####################################################################
void reconnect(){
  //loop til reconnected
  while (!client.connected()){
    Serial.print("Attempting connection...");

    //create random client id
    // this must be unique for the mqtt broker to identify this client
    String clientid = "ESP8266Client-";
    clientid += String(random(0xffff,HEX));

    //attempt to connect
    if (client.connect(clientid.c_str(), mqttuser, mqttpass)) {
      Serial.println("connected");
      
      //subscribe to messages on broker for remote control
      Serial.println(("Subscribing for: " + plantTopic + "#" ).c_str());
      client.subscribe((plantTopic + "#").c_str());

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("Try again in 5 secs");
      delay(5000);
    }
  }
}

//#####################################################################
// this callback is being used to track topic updates arriving through 
// mqtt whilst in development
//for prod use, this should be removed if not used
//#####################################################################
void callback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.println("]");

/*  ######################################################  
  //##############REMOVE HARDCODED TOPIC PATH AFTER TESTING
  //check if switch off required
  if (strcmp(topic, "student/CASA0014/plant/ucfnamm/1PLF1CEL/RHS5MF/rc/") == 0){
    Serial.println("Recieved remote instrcution:");
    if ((char)payload[0] == '0') {
      //switch off
      Serial.println("Switch Off");
    }
    if ((char)payload[0] == '1') {
      //switch on
      Serial.println("Switch On");
    }
  } 
*/
  char pload[length];

  for (int i = 0; i < length; i++){
    Serial.print((char)payload[i]);
    pload[i] = (char)payload[i];
  }

  Serial.print("Payload: ");
  for (int i = 0; i < length; i++){
    Serial.print((char)pload[i]);
  }
  Serial.println();
  
}

//########################################################
// request from a web client recieved
// grab current values and publish via SendHTML function
// intentionally not refreshing here to keep values in sync with last MQTT publish
// revisit logic if there extended intervals between reads
//########################################################
void handle_OnConnect() {
  Serial.println("handle_OnConnect: processing");
  //Temperature = dht.readTemperature(); // Gets the values of the temperature
  //Humidity = dht.readHumidity(); // Gets the values of the humidity
  server.send(200, "text/html", SendHTML(Temperature, Humidity, Moisture));
}

//########################################################
// generate 404 error response for a request for no matching function
//########################################################
void handle_NotFound() {
  Serial.println("Handle not found");
  server.send(404, "text/plain", "Not found");
}
//####################################################################
// Added basic accessibility info
// Added simple formatting and device info
// Added placeholder buttons for starting and stopping device
String SendHTML(float Temperaturestat, float Humiditystat, int Moisturestat) {
  String ptr = "<!DOCTYPE html lang=""en""> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>ESP8266 DHT22/Soil Moisture Report and Controls</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr += "p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div id=\"webpage\">\n";
  ptr += "<h1>ESP8266 Huzzah DHT22 Report</h1>\n";
  ptr += "<p>Device ID: ";
  ptr += plantId;
  ptr += "<p>Room ID: ";
  ptr += roomId;
  ptr += "<hr>";
  ptr += "<p>Temperature: ";
  ptr += (int)Temperaturestat;
  ptr += " C</p>";
  ptr += "<p>Humidity: ";
  ptr += (int)Humiditystat;
  ptr += "%</p>";
  ptr += "<p>Moisture: ";
  ptr += Moisturestat;
  ptr += "</p>";
  ptr += "<p>Sampled on: ";
  ptr += UTC.dateTime("l,"); //GB.dateTime("l,");
  ptr += "<br>";
  ptr += UTC.dateTime("d-M-y H:i:s T"); //GB.dateTime("d-M-y H:i:s T");
  ptr += "</p>";
  ptr += "<hr>";
  ptr += "<br>";
  ptr += "<p>Control Device On/Off:";
  ptr += "<br>";
  ptr += "<button type=""button"" style=""margin:10px;"" disabled>Start</button>";
  ptr += "<button type=""button"" style=""margin:10px;"" disabled>Stop</button>";
  ptr += "</p>";
  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}

/*
void publishToRC(){
  snprintf (msg, 50, "#%ld", value);
  Serial.print("Publish message: ");
  Serial.println(msg);
  Serial.println(plantTopic.c_str());
  
  //###############################
  //publish to mqtt
  //client.publish(statusTopic.c_str(),msg);
  //client.publish(plantTopic.c_str(),msg);
  client.publish((plantTopic + remoteControl).c_str(),msg);
}
*/
