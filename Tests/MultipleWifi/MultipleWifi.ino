
#include <Arduino_MultiWiFi.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "./secrets.h";

MultiWiFi multiClient;

void setup() {
    // Wait until the serial port is connected
    Serial.begin(115200);
    while (!Serial) {}

    // Configure the known networks (first one gets higher priority)
    multiClient.add(SECRET_SSID, SECRET_PASS);
    multiClient.add(SECRET_SSID_BU, SECRET_PASS_BU);

    // Connect to the first available network
    Serial.println("Looking for a network...");
    if (multiClient.run() == WL_CONNECTED) {
        Serial.print("Successfully connected to network: ");
        Serial.println(WiFi.SSID());
    } else {
        Serial.println("Failed to connect to a WiFi network");
    }

    
    PubSubClient client(espClient); 

    //set up mqtt connection
    client.setServer(MQTT_SERVER,MQTT_PORT);
    //set up callback function to be trigger on mqtt update for a subcribed topic
    client.setCallback(callback);

    client.subscribe(("student/CASA0014/plant/ucfnamm/#").c_str());
}

void loop() {
    delay(1000);
    
    PubSubClient client(espClient); 
}

void callback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.println("]");
}