
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
  Serial.begin(57600);
  printf_begin();
  
  radio.begin();
  radio.setAutoAck(false);

  // Get into standby mode
  radio.startListening();
  radio.stopListening();

  radio.printDetails();

  // Set the PA Level low to prevent power supply related issues since this is a
 // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio.setPALevel(RF24_PA_LOW);
  
  // Open a writing and reading pipe on each radio, with opposite addresses
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1,addresses[1]);
  radio.stopListening();
  
  // Start the radio listening for data
  //radio.startListening();
}

char pkt[32];

char line[128];
int lineSym = 0;
int64_t data[16];

int64_t atoiCustom(char* str) {
  int64_t num = 0;
  int currPos = 0;
  if(str[0] == '-') currPos = 1;
  while(str[currPos] != ',' && str[currPos] != '\0') {
    num = num * 10 + (str[currPos] - '0');
    currPos++;
  }
  if(str[0] == '-') return -num;
  else return num;
}

void loop() {
  if(Serial.available()) {
    char c = Serial.read();
    line[lineSym] = c;
    lineSym++;
    if(c == '\n') {
      line[lineSym - 1] = '\0';
      lineSym = 0;
      //Serial.println(line);
  
      int count = 0;
      char* offset = line;
      while(true) {
        data[count] = atoiCustom(offset);
        count++;
        offset = strchr(offset, ',');
        if(offset) offset++;
        else break;
      }
      pkt[0] = data[0];
      pkt[1] = data[1];
      ((long*)(pkt + 2))[0] = (long)data[2];
    
      if (!radio.write( pkt, 32 )){
         Serial.println(F("failed"));
      }
          
      //radio.startListening();
    }
  }
}
