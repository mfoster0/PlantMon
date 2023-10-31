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
Add error handlers
Warnings / errors
Add checksum
Add tamper checks
Check the remote on/off:
  - via webpage
  - via mqtt
  - add on/off for each function
Use LED's for physical monitoring
Add sound alert to support non-visual alert
Set up remote monitoring
Automatically attempt to connect to back up WiFi connection if primary fails
Move from delay to millis
Update pulse logic to show a less frequent heartbeat
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <string>
#include <iostream>
#include <cstring>

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
const char* healthTopic = HEALTH_TOPIC_ID; //used to send system health separate to readings
const char* errTopic = ERROR_TOPIC_ID; //used to send errors separate to readings
const char* topicBase = TOPIC_BASE; //this is the main/higher part of topics that will be  the same for all subscriptions
const char* roomId = ROOM_ID; //CE Lab
const char* plantId = PLANT_ID; //ID of individual plant
const int interval = MINIMUM_INTERVAL; //millis to wait between events
const int pubIntervals = PUBLISH_INTERVALS; //number of intervals before publishing update
const char* remoteControl = REMOTE_CONTROL_TOPIC; //topic used to communicate back to the arduino through MQTT

bool ledOn = false;

int elapsedIntervals = 0;

//const char* statusTopic = topicBase + plantId;

//#################################
//Making code more config driven so changes only need to be made to the associated config file and not the main code
String plantTopic = String(topicBase) + roomId + plantId;

//declare the clients - wifi & the pubsub
WiFiClient espClient;
PubSubClient client(espClient); //used for mqtt communication

long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  //### ensure baud rate is correct
  Serial.begin(115200);
  delay(100);

  //set the built in LED as an output
  pinMode(LED_BUILTIN,OUTPUT);
  ledOn = false;
  digitalWrite(LED_BUILTIN, LOW); //switch off LED

  //set up mqtt connection
  client.setServer(mqtt_server,mqtt_port);
  //set up callback function to be trigger on mqtt update for a subcribed topic
  client.setCallback(callback);

  //connect to wifi
  Serial.println();
  Serial.println("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid_bu, password_bu);
  
  //wait for wifi connection
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  //print IP of device
  Serial.println("");
  Serial.println("Wifi connected"),
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

void loop() {

  //currently using delay as a timer
  //this will be switched to millis and sync with ezTime
  // increment timer
  elapsedIntervals++;
  
  //check if any actions need performing
  processDueActions();

  //wait for defined interval
  delay(interval);
}

void processDueActions() {
  //##########################################################
  //so that the device is not in a "delay" state for too long, a counted is used to allow loop cycles
  //process actions if defined intervals have passed
  if (elapsedIntervals >= pubIntervals  ){
    sendMQTT();
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


}

void sendMQTT(){
  //check if connection to MQTT is active
  if (!client.connected()){
    reconnect();
  }
  
  //####################
  //keep connection to mqtt alive
  client.loop();
  ++value;

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
