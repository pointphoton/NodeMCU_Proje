#include <Arduino.h>
#include <Wire.h>
#include <BH1750.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>  
#include "Creds.h"  
#include "Setup.h" 
#include <string>
#include <sstream>



BH1750 lightMeter(0x23);
WiFiClient client;
PubSubClient mqttClient(client);

struct light
{
  float lux; // the light intensity as a lux.
} light;

static const String field_check = "field2"; //the topic-check of the callback
static const char alphanum[] = "0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz";
unsigned long lastConnectionTime = 0; 
const unsigned long postingInterval = 90L * 1000L; // Post data every one and half minute.
int dim ;  
int mode = AUTO; //0 is AUTO  & 1 is MANUEL mode 

void connectToWiFi(char const *ssid, char const *password);
//void callback(char* topic, byte* payload, unsigned int length);
void reconnectAndSubscribe(char const *username, char const *password,unsigned long subChannelID,char const *readAPIKey);
int mqttSubscribeLevel(long subChannelID,char const *readAPIKey);
int mqttSubscribeMode(long subChannelID,char const *readAPIKey);
void readLightData(struct light *l);
void mqttPublishLight();
void adjustLEDLight();
void adjustLEDLightWithLevel(int level);

//Callback from ThingSpeak MQTT Topic
void callback(char* topic, byte* payload, unsigned int length) {
Serial.print("Message arrived [");
Serial.print(topic);
Serial.print("]");
Serial.println();
String received = "";
String check=String(topic); 
String s1=check.substring(34,40);
Serial.println("field= "+s1);
char ch[length];
for (uint i=0;i<length;i++) {
//Serial.print((char)payload[i]);
received = received + (char)payload[i];
ch[i]= (char)payload[i];
}
Serial.println("received= "+received);
if (field_check==s1){ // the data received from the field2
if(mode==MANUEL){ //manuel mode
 Serial.println("mode 62 = "+mode); 
std::istringstream iss1 (ch);
  int number;
  iss1 >> number;
  delay(10);
  adjustLEDLightWithLevel(number);
}

}else{ //the data received from the field3 
 Serial.println("mode 71 = "+mode); 
std::istringstream iss2 (ch);
int number;
iss2 >> number;
delay(10);
Serial.println("mode= "+number); 
mode=number;
}

 
}
 


void setup() {
  //set baudrate
   Serial.begin(9600);
   //set NodeMCU output as the pin D7
   pinMode(S_LED, OUTPUT); 
   //set starting value of the strip led. 0 is not emitting any light.
   analogWrite(S_LED,dim);
   //start I2C connection with the BH1750 light sensor
   Wire.begin();
   //start BH1750 to collect the light data 
   if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  }
  else {
    Serial.println(F("Error initialising BH1750"));
  }
  //connect to wireless network
  connectToWiFi(SSID,PASSWORD);
  // set Thingspeak's MQTT server over the unencrypted port
  mqttClient.setServer(MQTT_SERVER, 1883); 
 //set callback of the MQTT service in order to listen android app data
  mqttClient.setCallback(callback);
  dim= KAPALI;
}

void loop() {
  // put your main code here, to run repeatedly:   
  
  // reconnect if MQTT client is not connected.
  if (!mqttClient.connected()) 
  {
  reconnectAndSubscribe(MQTT_USERNAME,MQTT_PASSWORD,MQTT_CHANNEL_ID,MQTT_READ_API_KEY);
  }
  // Call the loop continuously to establish connection to the server.
  mqttClient.loop();  
  //calculate time difference & read sensor data and publish it. 
  if (millis() - lastConnectionTime > postingInterval) 
  {
     readLightData(&light);
     delay(10);
     if(mode==AUTO){ //adjust brightness of the light if the selection mode is the AUTO
        adjustLEDLight();
     }    
     mqttPublishLight();
     

  }
}

//Establish a wifi connection
void connectToWiFi(char const *ssid, char const *password)
{
  delay(10);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);
  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
  would try to act as both a client and an access-point and could cause
  network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  //start connecting to WiFi
  WiFi.begin(ssid, password);
  //while client is not connected to WiFi keep loading
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
}



//reconnect to ThingSpeak MQTT Service
void reconnectAndSubscribe(char const *username, char const *password,unsigned long subChannelID,char const *readAPIKey) 
{
  char clientID[9];

  // Loop until reconnected.
  while (!mqttClient.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Generate ClientID
    for (int i = 0; i < 8; i++) {
        clientID[i] = alphanum[random(51)];
    }
    clientID[8]='\0';

    // Connect to the MQTT broker.
    if (mqttClient.connect(clientID,username,password)) 
    {
      Serial.println("connected");
     mqttSubscribeLevel(subChannelID,readAPIKey);
     mqttSubscribeMode(subChannelID,readAPIKey);
     
    } else 
    {
      Serial.print("failed, rc=");
      // Print reason the connection failed.
      // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

//Subscribe to ThinkSpeak MQTT Topic
int mqttSubscribeLevel(long subChannelID,char const *readAPIKey){
String myTopic="channels/"+String(subChannelID)+"/subscribe/fields/field"+String(MQTT_SUB_FIELD_MOBILE_LEVEL)+"/"+String(readAPIKey);
Serial.println("Subscribed to " +myTopic);
//Serial.print("State= " + String(mqttClient.state()));
char charBuf[myTopic.length()+1];
myTopic.toCharArray(charBuf,myTopic.length()+1);
return mqttClient.subscribe(charBuf);
}

int mqttSubscribeMode(long subChannelID,char const *readAPIKey){
String myTopic= "channels/"+String(subChannelID)+"/subscribe/fields/field"+String(MQTT_SUB_FIELD_MOBILE_MODE)+"/"+String(readAPIKey);
Serial.println("Subscribed to " +myTopic);
//Serial.print("State= " + String(mqttClient.state()));
char charBuf[myTopic.length()+1];
myTopic.toCharArray(charBuf,myTopic.length()+1);
return mqttClient.subscribe(charBuf);
}


  void readLightData(struct light *l){  
    //red value of the light sensor
   float lux = lightMeter.readLightLevel();
   Serial.print("Light: ");
   Serial.print(lux);
   Serial.println(" lx");
   //set value to struct-light
   l->lux = lux;   
}

void adjustLEDLight(){
  Serial.println("AUTO adjusting");
  //get last read value from struct-light
   float val = light.lux;

   if(val<AUTO_NIGHT){ //night
      dim = MAKS_PARLAKLIK; //maximum brightness
   }else if(val<AUTO_DARK_INDOOR){ //dark indoor
      dim = YUKSEK_PARLAKLIK;
   }else if(val<AUTO_INDOOR){ //indor & cloudy outdoor
      dim = ORTA_PARLAKLIK;
   }else if(val<AUTO_OUTDOOR){ //outdoor
      dim = DUSUK_PARLAKLIK;
   }else{ //very bright light
      dim = KAPALI; //minumum brightness
   }
   //set brightness of the LED
    analogWrite(S_LED,dim);
}
void adjustLEDLightWithLevel(int level){
   Serial.println("MANUEL adjusting");
  switch (level)
  {
  case GECE://level 5 is night mode
    dim = MAKS_PARLAKLIK;
    break;
   case KOYU_KAPALI_ALAN://level 4 is dark-indoor
    dim = YUKSEK_PARLAKLIK;
    break;
   case KAPALI_ALAN://level 3 is indoor
    dim = ORTA_PARLAKLIK;
    break; 
   case ACIK_ALAN://level 2 is outdoor
    dim = DUSUK_PARLAKLIK;
    break;
   case AYDINLIK://level 1 is bright
    dim = KAPALI;
    break;  
  default: 
 dim = KAPALI;
    break;
  }
  //set brightness of the LED
    analogWrite(S_LED,dim);
}
 
//publish data to ThingSpeak MQTT Service
void mqttPublishLight(){

// Create data string to send to ThingSpeak.
  String data = String(light.lux, DEC);
  //int length = data.length();
  const char *msgBuffer;
  msgBuffer = data.c_str();
  Serial.println(msgBuffer);

  // Create a topic string and publish data to ThingSpeak channel feed. 
  String topicString ="channels/" + String( MQTT_CHANNEL_ID ) + "/publish/fields/field" + String(MQTT_PUB_FIELD_LIGHT) + "/" + String(MQTT_WRITE_API_KEY);
  //length = topicString.length();
  const char *topicBuffer;
  topicBuffer = topicString.c_str();
  mqttClient.publish( topicBuffer, msgBuffer );

 lastConnectionTime = millis();
}