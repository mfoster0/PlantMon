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

//MQTT Topic Config 
const char* topicBase = TOPIC_BASE; //this is the main/higher part of topics that will be  the same for all subscriptions
const char* roomId = ROOM_ID; //CE Lab
const char* plantId = PLANT_ID; //ID of individual plant
const char* healthTopic = HEALTH_TOPIC_ID;
const char* healthTimeTopic = HEALTH_TIME_TOPIC_ID;
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

  // open serial connection for debug info
  Serial.begin(115200);
  delay(100);

  //print the script being executed
  Serial.println("Running script: MoistureTest");

  // start DHT sensor
  pinMode(DHTPin, INPUT);
  dht.begin();

  // run initialisation functions
  connectToWiFi();
  startWebserver();
  syncDate();

  // start MQTT server
  client.setServer(mqtt_server, 1884);
  client.setCallback(callback);
}

void loop() {
  // put your main code here, to run repeatedly:
  // handler for receiving requests to webserver
  server.handleClient();


//for testing 1 minute is too long, switching to a delay approach
/*
  if (minuteChanged()) {
    readMoisture();
    sendMQTT();
    Serial.println(GB.dateTime("H:i:s")); // UTC.dateTime("l, d-M-y H:i:s.v T")
  }
*/
  //get current readings
  updateReadings();

  sendMQTT();
  Serial.println(GB.dateTime("H:i:s")); // UTC.dateTime("l, d-M-y H:i:s.v T")
  delay(10000);

  client.loop();
}

void updateReadings(){
  //get soil moisture first as this takes the longest time to perform
  readMoisture();
  Temperature = dht.readTemperature(); // Gets the values of the temperature
  Humidity = dht.readHumidity(); // Gets the values of the humidity

  //check all values are in acceptable ranges
  String warning = checkValuesAreGood();
  
  if (warning.length() > 0 ) {
    client.publish((plantTopic + healthTopic).c_str(), warning.c_str());
    client.publish((plantTopic + healthTimeTopic).c_str(), UTC.dateTime("ymd His.v").c_str());
  }

}

String checkValuesAreGood(){
  String result = "";
  if (Moisture < danger_moist_low ) {
    result += "Moist Low (";
    result += Moisture;
    result += ") ";
  } 
  if (Moisture > danger_moist_high ) {
    result += "Moist High (";
    result += Moisture;
    result += ") ";
  }
  if (Temperature < danger_temp_low ) {
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


void readMoisture(){
  int soilReading = -1;
  int mappedSoilReading = -1;
  // power the sensor
  digitalWrite(sensorVCC, HIGH);
  digitalWrite(blueLED, LOW);
  delay(100);
  // read the value from the sensor:         
  // Mapped the range of the min and max values taken to 0-10. 
  //This intentionally removes much of the granularity of the reading due to this sensor have multiple factors that can influence its performance  
  //soilReading = analogRead(soilPin);
  
  Moisture = analogRead(soilPin);

  Serial.print("Soil: ");
  Serial.println(Moisture);
  
  //map to small range due to such inprecise readings. min value
  //min observed value: 8, max: 1024 ==> Min: 0, Max:10     
  //mappedSoilReading = mapVal(soilReading, 0,1025, 0, 10);  
/*  Moisture = map(soilReading, 0,1025, 1, 10); 
  Serial.print("Mapped Soil: ");
  Serial.println(Moisture);
*/
  //Moisture = analogRead(soilPin);
  //stop power
  digitalWrite(sensorVCC, LOW);  
  digitalWrite(blueLED, HIGH);
  delay(100);
 
  ////Serial.println(soilReading);   // read the value from the nails

}

int mapVal(int x, int in_min, int in_max, int out_min, int out_max) {
  Serial.println("mapVal in: ");
  Serial.println(x);
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
/*
void startWifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // check to see if connected and wait until you are
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("."); //show reattempt progress
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
*/

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

void syncDate() {
  // get real date and time
  waitForSync();
  Serial.println("UTC: " + UTC.dateTime());
  GB.setLocation("Europe/London");
  Serial.println("London time: " + GB.dateTime());
}

void startWebserver() {
  // when connected and IP address obtained start HTTP server
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void sendMQTT() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

 
  snprintf (msg, 50, "%.1f", Temperature);
  Serial.print("Publish message for temperature: ");
  Serial.println(msg);
  //client.publish("student/CASA0014/plant/ucfnamm/temperature", msg);
  client.publish((plantTopic + "temperature").c_str(), msg);


  snprintf (msg, 50, "%.0f", Humidity);
  Serial.print("Publish message for humidity: ");
  Serial.println(msg);
  //client.publish("student/CASA0014/plant/ucfnamm/humidity", msg);
  client.publish((plantTopic + "humidity").c_str(), msg);

  //Moisture = analogRead(soilPin);   // moisture read by readMoisture function
  snprintf (msg, 50, "%.0i", Moisture);
  Serial.print("Publish message for moisture: ");
  Serial.println(msg);
  //client.publish("student/CASA0014/plant/ucfnamm/moisture", msg);
  client.publish((plantTopic + "moisture").c_str(), msg);

  String sendTime = UTC.dateTime("ymd His.v");
  client.publish((plantTopic + "time").c_str(), sendTime.c_str());
  client.publish((plantTopic + "check").c_str(), codeString(sendTime).c_str());

  Serial.println(sendTime);
}
//#######################################################################################################
//##
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
      client.subscribe("student/CASA0014/plant/ucfnamm/#");
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
