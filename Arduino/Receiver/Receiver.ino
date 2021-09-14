#include <GyverTimers.h>


/*
* Getting Started example sketch for nRF24L01+ radios
* This is a very basic example of how to send data from one node to another
* Updated: Dec 2014 by TMRh20
*/

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include <Servo.h>

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(7,8);
/**********************************************************/

byte addresses[][5] = {"Comp","Arm0"};

Servo armServo;
Servo forearmServo;

char pkt[32];
volatile long currStep = 0;
volatile long targStep = 0;

void setup() {
  armServo.attach(2);
  forearmServo.attach(3);
  Serial.begin(115200);
  printf_begin();
  
  radio.begin();
  radio.setAutoAck(false);

  // Get into standby mode
  radio.startListening();
  radio.stopListening();

  radio.printDetails();
  
  radio.openWritingPipe(addresses[1]);
  radio.openReadingPipe(1,addresses[0]);
  
  // Start the radio listening for data
  radio.startListening();

  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  
  Timer0.setPeriod(5000);
  Timer0.enableISR();

  delay(500);
  targStep = 500;
  delay(500);
  targStep = 0;
  delay(500);
}

ISR(TIMER0_A) {
  if(currStep == targStep) return;
  digitalWrite(A1, currStep < targStep);
  if(currStep < targStep) {
    currStep++;
    digitalWrite(A0, 1);
    for(int i = 0; i < 20; i++);
    digitalWrite(A0, 0);
  }
  if(currStep > targStep) {
    currStep--;
    digitalWrite(A0, 1);
    for(int i = 0; i < 20; i++);
    digitalWrite(A0, 0);
  }
}

void loop() {
    if( radio.available()){
                                                                    // Variable for the received timestamp
      while (radio.available()) {                                   // While there is data ready
        radio.read( pkt, 32 );             // Get the payload
      }
      armServo.write((unsigned char) pkt[0]);
      forearmServo.write((unsigned char) pkt[1]);
      targStep = ((long*)(pkt + 2))[0];
      Serial.print((int)((unsigned char)pkt[0]));
      Serial.print(" ");
      Serial.print((int)((unsigned char)pkt[1]));
      Serial.print(" ");
      Serial.println((int)targStep);
   }
}
