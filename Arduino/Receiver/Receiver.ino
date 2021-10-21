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
RF24 radio(10, 9);
/**********************************************************/

byte addresses[][5] = {"Comp","Arm0"};

Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;
Servo servo5;

char pkt[32];
volatile int64_t currStep = 0;
volatile int64_t targStep = 0;

enum MICROSTEP_MODE {
  MS_1 = 0, MS_2 = 1, MS_4 = 2, MS_8 = 3, MS_16 = 7
};

char microstep = MS_1;

void setup() {
  digitalWrite(A5, 1);
  servo1.attach(2);
  servo2.attach(3);
  servo3.attach(4);
  servo4.attach(5);
  servo5.attach(6);
  Serial.begin(230400);
  Serial.println("shiiiiiiiiiiiiiit");
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
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  pinMode(A4, OUTPUT);
  pinMode(A5, OUTPUT);
  
  digitalWrite(A4, microstep & 0b1);
  digitalWrite(A3, microstep & 0b10);
  digitalWrite(A2, microstep & 0b100);
  
  Timer0.setPeriod(5000);
  Timer0.enableISR();
  
  digitalWrite(A5, 0);
}

ISR(TIMER0_A) {
  if(currStep == targStep) return;
  digitalWrite(A0, currStep < targStep);
  if(currStep < targStep) {
    currStep++;
    digitalWrite(A1, 1);
    for(int i = 0; i < 20; i++);
    digitalWrite(A1, 0);
  }
  if(currStep > targStep) {
    currStep--;
    digitalWrite(A1, 1);
    for(int i = 0; i < 20; i++);
    digitalWrite(A1, 0);
  }
}

void loop() {
    if( radio.available()){
      radio.read( pkt, 32 );
      servo1.write((unsigned char) pkt[0]);
      servo2.write((unsigned char) pkt[1]);
      servo3.write((unsigned char) pkt[2]);
      servo4.write((unsigned char) pkt[3]);
      servo5.write((unsigned char) pkt[3]);
      targStep = ((long*)(pkt + 5))[0];
   }
}
