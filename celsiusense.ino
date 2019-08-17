/* ESP8266 AWS IoT example by Evandro Luis Copercini
   Public Domain - 2017
   But you can pay me a beer if we meet someday :D
   This example needs https://github.com/esp8266/arduino-esp8266fs-plugin
  It connects to AWS IoT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
*/

#include "FS.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>

//Def
#define myPeriodic 2  //in sec | Thingspeak pub is 15sec
#define LED1 D1          // Led in NodeMCU at pin GPIO16 (D0).
#define LED2 D2          // Led in NodeMCU at pin GPIO16 (D0).
#define BUZZER D0

const char *host = "api.thingspeak.com";

const int httpsPort = 443;  //HTTPS= 443 and HTTP = 80
const char fingerprint[] PROGMEM = "F9 C2 65 6C F9 EF 7F 66 8B F7 35 FE 15 EA 82 9F 5F 55 54 3E";
// float prevTemp = 0;
String apiKey = "BQKR5IL71F10W3YR";

int sent = 0;
int val;
int tempPin = A0;

// Update these with values suitable for your network.

const char* ssid = "Cecilyfeng";
const char* password = "yiyi85876098yiyi";
const char* aws_iot_sub_topic = "topic/hello";
const char* aws_iot_pub_topic = "mySNStopic/CelsiuSense";
const char* aws_iot_pub_message = "Acknowledged.";
const char* client_id = "test";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const char* AWS_endpoint = "a3bne9n7ezednc-ats.iot.us-east-2.amazonaws.com"; //MQTT broker ip

WiFiClientSecure client;
PubSubClient mqtt(client);


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
 
  //mqtt.publish(aws_iot_pub_topic, Temp); //*****

}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  client.setBufferSizes(512, 512);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  while(!timeClient.update()){
    timeClient.forceUpdate();
  }

  client.setX509Time(timeClient.getEpochTime());

}

void setup() {


  pinMode(LED1, OUTPUT);    // LED pin as output.
  pinMode(LED2, OUTPUT);    // LED pin as output.
  pinMode(BUZZER, OUTPUT);
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  setup_wifi();
  delay(1000);
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

  // Load certificate file
  File cert = SPIFFS.open("/certificate.der", "r"); //replace cert.crt eith your uploaded file name
  if (!cert) {
    Serial.println("Failed to open cert file");
  }
  else
    Serial.println("Success to open cert file");

  delay(1000);

  if (client.loadCertificate(cert))
    Serial.println("cert loaded");
  else
    Serial.println("cert not loaded");

  // Load private key file
  File private_key = SPIFFS.open("/private.der", "r"); //replace private eith your uploaded file name
  if (!private_key) {
    Serial.println("Failed to open private cert file");
  }
  else
    Serial.println("Success to open private cert file");

  delay(1000);

  if (client.loadPrivateKey(private_key))
    Serial.println("private key loaded");
  else
    Serial.println("private key not loaded");



    // Load CA file
    File ca = SPIFFS.open("/ca.der", "r"); //replace ca eith your uploaded file name
    if (!ca) {
      Serial.println("Failed to open ca ");
    }
    else
    Serial.println("Success to open ca");

    delay(1000);

    if(client.loadCACert(ca))
    Serial.println("ca loaded");
    else
    Serial.println("ca failed");

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

  // AWS IoT MQTT uses port 8883
  mqtt.setServer(AWS_endpoint, 8883);
  mqtt.setCallback(callback);
  
}

void sendTemperatureTS(float cel, String state)
{
  WiFiClientSecure httpsClient;
  httpsClient.setFingerprint(fingerprint);
  Serial.printf("Using fingerprint '%s'\n", fingerprint);
  httpsClient.setTimeout(15000); // 15 Seconds
  delay(1000);
  if (httpsClient.connect(host, 443)) { // api.thingspeak.com on port 443
    Serial.println("httpsClient connected ");

    String postStr = apiKey;
    postStr += "&field1=";
    postStr += String(cel);
    postStr += "&field2=";
    postStr += String(state);
    postStr += "\r\n\r\n";

    httpsClient.print("POST /update HTTP/1.1\n");
    httpsClient.print("Host: api.thingspeak.com\n");
    httpsClient.print("Connection: close\n");
    httpsClient.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
    httpsClient.print("Content-Type: application/x-www-form-urlencoded\n");
    httpsClient.print("Content-Length: ");
    httpsClient.print(postStr.length());
    httpsClient.print("\n\n");
    httpsClient.print(postStr);
    delay(1000);

    String line;
    while (httpsClient.available()) {
      line = httpsClient.readStringUntil('\n');  //Read Line by Line
      Serial.println(line); //Print response
    }

  }//end if
  else {
    Serial.println("httpsClient failed");
  }
  sent++;
  httpsClient.stop();
}//end send

void loop() {

  // reconnect on disconnect
  while (!mqtt.connected()) {
    Serial.print("Now connecting to AWS IoT: ");
    if (mqtt.connect(client_id)) {
      Serial.println("connected!");

      const char message1[35]="{\"state\": \"MAINTENANCE\"}";
      const char message2[25]="{\"state\": \"OKAY\"}";

      while(1)
      {

        String state;
        //temperature sensing
        val = analogRead(tempPin);
        float mv = ( val/1024.0)*3300;
        float cel = mv/10;
        Serial.print("TEMPRATURE = ");
        Serial.print(cel);
        Serial.print("*C\n");
        delay(1000);

        // Converting the float data to char arrays
        //char sTemp[10];
        //dtostrf(cel, 6, 2, sTemp);
        //Temp= sTemp;
        
    
        if (cel <= 29) // too cold < 19, green on
        {
           digitalWrite(LED1, HIGH); // turn the LED off.
           digitalWrite(LED2, LOW); // turn the LED on.
           digitalWrite(BUZZER, LOW);
           mqtt.publish(aws_iot_pub_topic, message2);
           state = "too cold";
        }

        else if (cel > 29 && cel < 30) // normal
        {
           digitalWrite(LED2, LOW); // turn the LED off.
           digitalWrite(LED1, LOW); // turn the LED on.
           digitalWrite(BUZZER, LOW);
           mqtt.publish(aws_iot_pub_topic, message2);
           state = "good";
        }

        else if (cel >= 30 && cel < 32) // too hot > 25, red on
        {
           digitalWrite(LED2, HIGH); // turn the LED on.
           digitalWrite(LED1, LOW); // turn the LED off.
           digitalWrite(BUZZER, LOW);
           mqtt.publish(aws_iot_pub_topic, message2);
           state = "too hot";

        }
      
        else {
          digitalWrite(LED1, LOW); // turn the LED off.
          digitalWrite(LED2, LOW); // turn the LED off.
          digitalWrite(BUZZER, HIGH); //buzzer on
          delay(500);
          digitalWrite(BUZZER, LOW);
          mqtt.publish(aws_iot_pub_topic, message1);
          state = "need maintenance";
        }

        sendTemperatureTS(cel, state);
          int count = myPeriodic;
          while (count--)
          delay(1000);
       
      }
      //mqtt.subscribe(aws_iot_sub_topic); //subscribe to the topic
    } else {
      Serial.print("failed with status code ");
      Serial.print(mqtt.state());
      Serial.println(" trying again in 5 seconds...");
      delay(5000);
    }
  }

  

  mqtt.loop();
}
