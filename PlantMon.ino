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
#include <ezTime.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// Sensors - DHT22 and Nails
uint8_t DHTPin = 12;        // on Pin 2 of the Huzzah
uint8_t soilPin = 0;      // ADC or A0 pin on Huzzah
const int buzzer = 14; //buzzer pin

float Temperature;
float Humidity;
long Moisture = 1; // initial value just in case web page is loaded before readMoisture called
int sensorVCC = 13;
int blueLED = 2;
DHT dht(DHTPin, DHTTYPE);   // Initialize DHT sensor.

// Wifi and MQTT
#include "arduino_secrets.h" 
#include "hidden_config.h" 

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* ssid_bu = SECRET_SSID_BU;
const char* password_bu = SECRET_PASS_BU;
const char* mqttuser = SECRET_MQTTUSER;
const char* mqttpass = SECRET_MQTTPASS;
const char* cypher = SECRET_CYPHER;
bool local_notifications = LOCAL_NOTIFICATIONS; //toggle device notifications on/of
//MQTT Topic Config 
const char* topicBase = TOPIC_BASE; //this is the main/higher part of topics that will be  the same for all subscriptions
const char* roomId = ROOM_ID; //CE Lab
const char* plantId = PLANT_ID; //ID of individual plant
const char* healthTopic = HEALTH_TOPIC_ID;
const char* healthTimeTopic = HEALTH_TIME_TOPIC_ID;
const char* subscription_topic = SUB_TOPIC;

bool stopTemp = false;
bool stopHum = false;
bool stopMoist = false;

//Making code more config driven so changes only need to be made to the associated config file and not the main code
String plantTopic = String(topicBase) + roomId + plantId;

int min_moist = MIN_MOIST;
int max_moist = MAX_MOIST;

int danger_moist_low = DANGER_MOIST_LOW;
int danger_moist_high = DANGER_MOIST_HIGH;

float danger_temp_low = DANGER_TEMP_LOW;
float danger_temp_high = DANGER_TEMP_HIGH;

float danger_humidity_low =  DANGER_HUMIDITY_LOW;
float danger_humidity_high =  DANGER_HUMIDITY_HIGH;


ESP8266WebServer server(80);
const char* mqtt_server = MQTT_SERVER;
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// Date and time
Timezone GB;

void setup() {

  // Set up LED to be controllable via broker
  // Initialize the BUILTIN_LED pin as an output
  // Turn the LED off by making the voltage HIGH
  pinMode(BUILTIN_LED, OUTPUT);     
  digitalWrite(BUILTIN_LED, HIGH);  

  // Set up the outputs to control the soil sensor
  // switch and the blue LED for status indicator
  pinMode(sensorVCC, OUTPUT); 
  digitalWrite(sensorVCC, LOW);
  pinMode(blueLED, OUTPUT); 
  digitalWrite(blueLED, HIGH);
  delay(1000);
  digitalWrite(blueLED, LOW);
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output

  // open serial connection for debug info
  Serial.begin(115200);
  delay(100);

  //print the script being executed
  Serial.println("Running script: PlantMon");

  // start DHT sensor
  pinMode(DHTPin, INPUT);
  dht.begin();

  // run initialisation functions
  connectToWiFi();
  //currently disabled
  //startWebserver(); 
  syncDate();

  // start MQTT server
  client.setServer(mqtt_server, 1884);
  client.setCallback(callback);
}

void loop() {
  
  //check MQTT is connected
  connectMQTT();
  //give mqtt some time to establish connection after sleep
  for (int i = 1; i <= 3; ++i) {
    delay(2000); // *** use 10 secs to stay connected
    client.loop();
  }

  // handler for receiving requests to webserver
  server.handleClient();

  //get current readings
  updateReadings();

  Serial.println(GB.dateTime("H:i:s")); // UTC.dateTime("l, d-M-y H:i:s.v T")
  
  //sleep for 100 secs
  //wifi connection will be dropped so only use when sleeping for a longer period
  //ESP.deepSleep(6e7); //60 seconds
  //using loop to keep MQTT session alive to receive topic data
  //try with deep sleep and staying awake for n secs to receieve messages.
  //ESP.deepSleep(6e7); //60 seconds
  //ESP.deepSleep(4e7); //40 seconds
  ESP.deepSleep(3.6e8); //5 mins
  
  //delay(10);
  
  
}

void updateReadings(){
  //get soil moisture first as this takes the longest time to perform
  if (!stopMoist){
    readMoisture();
  }
  if (!stopTemp){
    Temperature = dht.readTemperature(); // Gets the values of the temperature
  }
  if (!stopMoist){
    Humidity = dht.readHumidity(); // Gets the values of the humidity
  }

  sendMQTT();

  //check all values are in acceptable ranges
  String warning = checkValuesAreGood();
  
  if (warning.length() > 0 ) {
    //produce physical alerts if setting is true
    if (local_notifications) {
      alertLocalDevice();
    }
    client.publish((plantTopic + healthTopic).c_str(), warning.c_str());
    client.publish((plantTopic + healthTimeTopic).c_str(), UTC.dateTime("ymd His.v").c_str());
  }

}

//alert someone locally with sound and LED
void alertLocalDevice(){
  tone(buzzer, 25); // Send 25KHz sounds like a bird!!
  delay(150);        
  noTone(buzzer);     // Stop buzzer
}

//if monitoring is on for a sensor, check values are good
String checkValuesAreGood(){
  String result = "";

  if (!stopMoist && Moisture < danger_moist_low ) {
    result += "Moist Low (";
    result += Moisture;
    result += ") ";
  } 
  if (!stopMoist && Moisture > danger_moist_high ) {
    result += "Moist High (";
    result += Moisture;
    result += ") ";
  }
  if (!stopTemp && Temperature < danger_temp_low ) {
    result += "Temp Low (";
    result += Temperature;
    result += ") ";
  } 
  if (Temperature > danger_temp_high ) {
    result += "Temp High (";
    result += Temperature;
    result += ") ";
  }
  if (Humidity < danger_humidity_low ) {
    result += "Humidity Low (";
    result += Humidity;
    result += ") ";
  } 
  if (Humidity > danger_humidity_high ) {
    result += "Humidity High (";
    result += Humidity;
    result += ") ";
  }
  return result;
}

//get moist reading the soil sensor
void readMoisture(){
  int soilReading = -1;
  int mappedSoilReading = -1;
  
  // power the sensor
  digitalWrite(sensorVCC, HIGH);
  digitalWrite(blueLED, LOW); //also used as a check to see the device is active
  delay(100);
  
  Moisture = analogRead(soilPin);

  Serial.print("Soil: ");
  Serial.println(Moisture);

  //Moisture = analogRead(soilPin);
  //stop power
  digitalWrite(sensorVCC, LOW);  
  digitalWrite(blueLED, HIGH);
 
  ////Serial.println(soilReading);   // read the value from the nails

}

//connection code to try backup if not found
void connectToWiFi(){
  bool usingBU = false;
  const char* usingSSID = ssid;
  const char* usingPwd = password;

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

//get current time
void syncDate() {
  // get real date and time
  waitForSync();
  Serial.println("UTC: " + UTC.dateTime());
  GB.setLocation("Europe/London");
  Serial.println("London time: " + GB.dateTime());
}

//start the oboard webserver
void startWebserver() {
  // when connected and IP address obtained start HTTP server
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");
}

//connect to data broker
void connectMQTT(){
  if (!client.connected()) {
    reconnect();
  }
}

//publish any new data to mqtt
void sendMQTT() {

  connectMQTT();

  client.loop();

  // If sensor is not disabled, get new value and send to mqtt
  if (!stopTemp){
    snprintf (msg, 50, "%.1f", Temperature);
    Serial.print("Publish message for temperature: ");
    Serial.println(msg);
    Serial.println((plantTopic + "temperature").c_str());
    //client.publish("student/CASA0014/plant/ucfnamm/temperature", msg);
    client.publish((plantTopic + "temperature").c_str(), msg);
   }

// If sensor is not disabled, get new value and send to mqtt
  if (!stopHum){
    snprintf (msg, 50, "%.0f", Humidity);
    Serial.print("Publish message for humidity: ");
    Serial.println(msg);
    //client.publish("student/CASA0014/plant/ucfnamm/humidity", msg);
    client.publish((plantTopic + "humidity").c_str(), msg);
  }

  // If sensor is not disabled, get new value and send to mqtt
  if (!stopMoist) {
    snprintf (msg, 50, "%.0i", Moisture);
    Serial.print("Publish message for moisture: ");
    Serial.println(msg);
    client.publish((plantTopic + "moisture").c_str(), msg);
  }

  // If any sensor is not disabled,send to mqtt
  if (!stopMoist || !stopTemp || !stopHum ){
    String sendTime = UTC.dateTime("ymd His.v");
    client.publish((plantTopic + "time").c_str(), sendTime.c_str());
    client.publish((plantTopic + "check").c_str(), codeString(sendTime).c_str());

    Serial.println(sendTime);
  }
}
//#######################################################################################################F
//## CHANGE CIPHER LOGIC TO ADD ALL READINGS AND TIME AND CREATE THE CHECK CHAR
//#######################################################################################################
//########################################################
// simple cipher function for checksum
//########################################################
String codeString(String str){
  //String last2Chars = str.substring(str.length() - 2);
  //str.substring(str.length() - 2) gets the last 2 chars of millisecs
  // the ".toInt()" casts them to int
  // this 0-99 values is used to grab the chardacter at that position in the cypher
  return String(cypher[str.substring(str.length() - 2).toInt()]);
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  //remotely switch on and off the sensors/readings and physical alerts
  if (strcmp(topic, "student/CASA0014/plant/ucfnamm/1PL/F1/CEL/RHS5MF/stoptemp/") == 0) {
      if ((char)payload[0] == '1'){
        Serial.println("Stopping temp");
        stopTemp = true;
      } else{
        Serial.println("Starting temp");
        stopTemp = false;
      }
   } else if (strcmp(topic, "student/CASA0014/plant/ucfnamm/1PL/F1/CEL/RHS5MF/stophumidity/") == 0) {
      if ((char)payload[0] == '1'){
        Serial.println("Stopping humidity");
        stopHum = true;
      } else{
        Serial.println("Starting humidity");
        stopHum = false;
      }
   } else if (strcmp(topic, "student/CASA0014/plant/ucfnamm/1PL/F1/CEL/RHS5MF/stopmoist/") == 0) {
      if ((char)payload[0] == '1'){
        Serial.println("Stopping moisture");
        stopMoist = true;
      } else{
        Serial.println("Starting moisture");
        stopMoist = false;
      }
   }
  
  if (strcmp(topic, "student/CASA0014/plant/ucfnamm/1PL/F1/CEL/RHS5MF/stopDeviceAlerts/") == 0) {
      if ((char)payload[0] == '1'){
        Serial.println("Stopping device alerts");
        local_notifications = false;
      } else{
        Serial.println("Starting temp");
        local_notifications = true;
      }
  }

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect with clientID, username and password
    if (client.connect(clientId.c_str(), mqttuser, mqttpass)) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe(subscription_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void handle_OnConnect() {
  Serial.println("handle_OnConnect: processing");
  Temperature = dht.readTemperature(); // Gets the values of the temperature
  Humidity = dht.readHumidity(); // Gets the values of the humidity
  server.send(200, "text/html", SendHTML(Temperature, Humidity, Moisture));
}

void handle_NotFound() {
  Serial.println("Handle not found");
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float Temperaturestat, float Humiditystat, int Moisturestat) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>ESP8266 DHT22 Report</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr += "p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div id=\"webpage\">\n";
  ptr += "<h1>ESP8266 Huzzah DHT22 Report</h1>\n";

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
  ptr += GB.dateTime("l,");
  ptr += "<br>";
  ptr += GB.dateTime("d-M-y H:i:s T");
  ptr += "</p>";

  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}


