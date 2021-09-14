
/*
* Getting Started example sketch for nRF24L01+ radios
* This is a very basic example of how to send data from one node to another
* Updated: Dec 2014 by TMRh20
*/

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(7,8);
/**********************************************************/

byte addresses[][5] = {"Comp","Arm0"};

// Used to control whether this node is sending or receiving
bool role = 0;

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  Serial.println(F("RF24/examples/GettingStarted"));
  printf_begin();
  Serial.println(F("*** PRESS 'T' to begin transmitting to the other node"));
  
  radio.begin();
  radio.setAutoAck(false);

  // Get into standby mode
  radio.startListening();
  radio.stopListening();

  radio.printDetails();

  // Set the PA Level low to prevent power supply related issues since this is a
 // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  //radio.setPALevel(RF24_PA_LOW);
  
  // Open a writing and reading pipe on each radio, with opposite addresses
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1,addresses[1]);
  
  // Start the radio listening for data
  radio.startListening();
}

char pkt[32];

char line[128];
int lineSymbol = 0;

int64_t atoiCustom(char* str) {
  int64_t num = 0;
  int currPos = 0;
  if(str[0] == '-') currPos = 1;
  while(str[currPos] != '\0') {
    num = num * 10 + (str[currPos] - '0');
    currPos++;
  }
  if(str[0] == '-') return -num;
  else return num;
}

void loop() {
  if(Serial.available()) {
    char c = Serial.read();
    line[lineSymbol] = c;
    lineSymbol++;
    if(c == '\n') {
      line[lineSymbol] = '\0';
      lineSymbol = 0;
      char num1[16];
      char num2[16];
      char num3[16];
      int currNum = 0;
      int numSym = 0;
      int currLineSym = 0;
      while(line[currLineSym] != '\0') {
        if(currNum == 0) num1[numSym] = line[currLineSym];
        if(currNum == 1) num2[numSym] = line[currLineSym];
        if(currNum == 2) num3[numSym] = line[currLineSym];

        if(line[currLineSym] == ' ') {
          if(currNum == 0) num1[numSym] = '\0';
          if(currNum == 1) num2[numSym] = '\0';
          if(currNum == 2) num3[numSym] = '\0';
          currNum++;
          numSym = -1;
        }
        numSym++;
        currLineSym++;
      }
      pkt[0] = atoiCustom(num1);
      pkt[1] = atoiCustom(num2);
      ((long*)(pkt + 2))[0] = atoiCustom(num3);
      //SerialPrintf("1:%d 2:%d 3:%d\n", atoiCustom(num1), atoiCustom(num2), atoiCustom(num3));
    }
  }
  
    radio.stopListening();
    if (!radio.write( pkt, 32 )){
       Serial.println(F("failed"));
    }
        
    radio.startListening();
}
