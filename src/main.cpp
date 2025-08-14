#include <Arduino.h>

void setup() {
  Serial.begin(9600);
}

void loop() {
  Serial.printf("Hello World");
  delay(1000);
}
