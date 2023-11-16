## simple monitor for plant health messages on mqtt
## warning get sent to the operator to deal with - Go water the plant!

import paho.mqtt.client as mqtt
from email.message import EmailMessage
import smtplib
import json



def get_value_from_json(json_file, key, sub_key):
   try:
       with open(json_file) as f:
           data = json.load(f)
           return data[key][sub_key]
   except Exception as e:
       print("Error: ", e)

## send email with alert
## email needs improving with gmail no longer available!!!!
def sendMail(alert):
    print("Sending email")
    sender = 'mark.foster@live.co.uk'
    recipient = 'mark.foster@live.co.uk'

    """   email = EmailMessage()
    email["From"] = sender
    email["To"] = recipient
    email["Subject"] = "Test Email"
    email.set_content(message)
    """

    pwd=get_value_from_json("secrets.json", "email", "pwd")
    
    smtp = smtplib.SMTP("smtp-mail.outlook.com", port=587)
    smtp.starttls()
    smtp.login(sender, pwd)
    smtp.sendmail(sender, recipient, alert)
    smtp.quit()


# Callback when the client connects to the broker
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    client.subscribe("student/CASA0014/plant/ucfnamm/1PL/F1/CEL/RHS5MF/health/#")  # Subscribe to the desired topic

# Callback when a message is received from the broker
def on_message(client, userdata, message):
    print("Received message '" + str(message.payload) + "' on topic '" + message.topic + "'")
    #### implement a check that the same topic hasn't been sent in n hours
    # send mail with health message 
    sendMail("Received message '" + str(message.payload) + "' on topic '" + message.topic + "'")


# Create an MQTT client
client = mqtt.Client()
# Set the username and password for authentication
client.username_pw_set("student", password="ce2021-mqtt-forget-whale")

# Set the callback functions
client.on_connect = on_connect
client.on_message = on_message

# Connect to the MQTT broker
client.connect("mqtt.cetools.org", 1884, 60)  # Replace "broker_address" with the address of your MQTT broker

# Start the MQTT client loop (this is a non-blocking loop that handles communication)
client.loop_forever()

#

