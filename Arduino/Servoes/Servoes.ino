#include <GyverTimers.h>
#include <Servo.h>

size_t SerialPrintf(const char *format, ...) {
    const uint8_t MAX_STRING_SIZE = 64;      
    char buf[MAX_STRING_SIZE];

    va_list args;
    va_start(args, format);
    vsnprintf(buf, MAX_STRING_SIZE, format, args);
    va_end(args);
    return Serial.print(buf);
}

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

volatile int64_t currStep = 0;
volatile int64_t targStep = 0;

ISR(TIMER0_A) {
  if(currStep == targStep) return;
  digitalWrite(5, currStep < targStep);
  if(currStep < targStep) {
    currStep++;
    digitalWrite(2, 1);
    for(int i = 0; i < 20; i++);
    digitalWrite(2, 0);
  }
  if(currStep > targStep) {
    currStep--;
    digitalWrite(2, 1);
    for(int i = 0; i < 20; i++);
    digitalWrite(2, 0);
  }
}

Servo servo1;
Servo servo2;

typedef struct ServoData {
  byte angle1;
  byte angle2;
  byte angle3;
  byte angle4;
} ServoData;

volatile ServoData sdata;
char line[128];
int lineSymbol = 0;

void setup() {
  servo1.attach(22);
  servo2.attach(23);
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(8, OUTPUT);
  digitalWrite(8, 0);
  Timer0.setPeriod(5000);
  Timer0.enableISR();
  sdata = {0};
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
      servo1.write(atoiCustom(num1));
      servo2.write(atoiCustom(num2));
      targStep = atoiCustom(num3) / 16;
      //SerialPrintf("1:%d 2:%d 3:%d\n", atoiCustom(num1), atoiCustom(num2), atoiCustom(num3));
    }
  }
}
