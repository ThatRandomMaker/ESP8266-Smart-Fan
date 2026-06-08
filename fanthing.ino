#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"

#define tempSensor D7
#define sensorType DHT11

ESP8266WebServer server(80);
DHT dht(tempSensor, sensorType);

void setup() {
  Serial.begin(115200);
  Serial.println("ready!");

  dht.begin();

  analogWriteFreq(1000);
  pinMode(D1, OUTPUT);

  WiFi.begin("wifiname", "wifipassword");
  while (WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print("wait..");
  }
  Serial.println("Connected!");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send(200, "text/html", "<h1>Hello World!</h1><h2>Hello World, but smaller!</h2>");
  });

  server.begin();
  Serial.println("well, that was a success.");
}

void loop() {

//reading for the temp sensor
//  float t = dht.readTemperature();
 
//   if (isnan(t)) {
//     Serial.println("haha fail");
//     return;
//   }
  
//   Serial.print("The temperature is... ");
//   Serial.print(t);


// // Placeholder for motor
// analogWrite(D1, 128);
// delay(3000);
// analogWrite(D1, 0);
// delay(3000);

//temporary thing for webpage
  server.handleClient();


}

