#include "DHT.h"

#define tempSensor D7
#define sensorType DHT11

DHT dht(tempSensor, sensorType);

void setup() {
  // put your setup code here, to run once:
Serial.begin(115200);
Serial.println("ready!");
dht.begin();

analogWriteFreq(1000);
pinMode(D1, OUTPUT);
}

void loop() {

// reading for the temp sensor

/*
 float t = dht.readTemperature();
 
  if (isnan(t)) {
    Serial.println("haha fail");
    return;
  }
  
  Serial.print("Temp: ");
  Serial.print(t);
*/

// stepper driver test (used stepper driver pcb due to not having the parts for normal fan pwm from esp8266)

analogWrite(D1, 128);
delay(3000);
analogWrite(D1, 0);
delay(3000);


}

