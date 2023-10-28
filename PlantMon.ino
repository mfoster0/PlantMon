/*
Arduino Plant Monitor

This monitor is a physical device that is located next to the plant and monitoring the conditions the plant is currently exposed to.
It does this through various sensors, initially this is through:
 - soil moisture detector, via steel nails registering the resistance of the soil, the less moisture in the soil, the higher the insulation (resistance).
 - air humidity and temperature through a DHT22

Readings are posted to an MQTT server for remote monitoring
For local/physical monitoring light and sound are used so that in the event of a connection issue with the server the monitor can still function
and health can be highlighted locally.

Files stored in the hidden directory are ignored
Arduino published messages are sent to the server and topics defined in the ./hidden/hidden_config.h file
Connection logins are stored in the ./hidden/arduino_secrets.h file

TODO:
Incorporate the working DHT script
Incorporate the working soil moisture script
Use plant ids
Add error handler
Warnings / errors
Add checksum
Add tamper checks
Check the remote on/off
Use LED's for physical monitoring
Set up remote monitoring


*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <string>
#include <iostream>
#include <cstring>

//locally stored logins and config
#include "./arduino_secrets.h" //#include "./hidden/arduino_secrets.h"
#include "./hidden_config.h" //#include "./hidden/hidden_config.h"

//pull in the logins and config
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* mqttuser = SECRET_MQTTUSER;
const char* mqttpass = SECRET_MQTTPASS;
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const char* healthTopic = HEALTH_TOPIC_ID; //used to send system health separate to readings
const char* errTopic = ERROR_TOPIC_ID; //used to send errors separate to readings
const char* topicBase = TOPIC_BASE;
const char* plantId = PLANT_ID;
const int interval = MINIMUM_INTERVAL;
const int pubIntervals = PUB_INTERVALS;
const char* remoteControl = REMOTE_CONTROL_TOPIC;

const char* topTemp = "temp";
const char* topHum = "hum";
const char* topMoist = "moist";  

bool ledOn = false;

int elapsedIntervals = 0;

//const char* statusTopic = topicBase + plantId;
String plantTopic = String(topicBase) + plantId;

//**declare the clients - wifi & the pubsub
WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  //
  Serial.begin(115200);
  delay(100);

  //set the built in LED as an output
  pinMode(LED_BUILTIN,OUTPUT);
  ledOn = false;
  digitalWrite(LED_BUILTIN, HIGH); //switch off LED

  client.setServer(mqtt_server,mqtt_port);
  client.setCallback(callback);

  //connect
  Serial.println();
  Serial.println("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wifi connected"),
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

void loop() {

  // increment timer
  elapsedIntervals++;
  
  //check if any actions need performing
  processDueActions();

  //wait for defined interval
  //Serial.println("Interval:")
  delay(interval);
}

void processDueActions() {
  snprintf (msg, 30, "Elapsed: #%ld - Trigger value: #%ld", elapsedIntervals, pubIntervals);
  Serial.println(msg);

  //process actions if defined intervals have passed
  if (elapsedIntervals >= pubIntervals  ){
    sendMQTT();
    elapsedIntervals = 0;
  }  

  //show pulse
  if (ledOn){
    digitalWrite(LED_BUILTIN, HIGH); //off
  } else {
    digitalWrite(LED_BUILTIN, LOW); //on
  }
  ledOn = !ledOn;
}

void sendMQTT(){
  //check if connection to MQTT is active
  if (!client.connected()){
    reconnect();
  }
  //**
  client.loop();
  ++value;

  snprintf (msg, 50, "#%ld", value);
  Serial.print("Publish message: ");
  Serial.println(msg);
  Serial.println(plantTopic.c_str());
  //client.publish(statusTopic.c_str(),msg);
  client.publish(plantTopic.c_str(),msg);
    // Convert the byte array to a char* using type casting

  
}

void reconnect(){
  //loop til reconnected
  while (!client.connected()){
    Serial.print("Attempting connection...");

    //**create random client id
    String clientid = "ESP8266Client-";
    clientid += String(random(0xffff,HEX));

    //attempt to connect
    if (client.connect(clientid.c_str(), mqttuser, mqttpass)) {
      Serial.println("connected");
      
      //subscribe to messages on broker for remote control
      //client.subscribe((plantTopic + "/" + remoteControl + ).c_str());
      client.subscribe((plantTopic + "/#").c_str());

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("Try again in 5 secs");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.println("]");
  
    //REMOVE HARDCODED TOPIC PATH AFTER TESTING
  //check if switch off required
  if (strcmp(topic, "student/CASA0014/plant/ucfnamm/rc") == 0){
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
