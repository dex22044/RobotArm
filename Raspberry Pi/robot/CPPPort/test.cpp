#include <iostream>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringSerial.h>

#include "Servo.h"

using namespace std;

int main(int argc, char** argv)
{
    wiringPiSetupGpio();

    Servo* servo = new Servo(13);
    
    pwmSetMode(PWM_MODE_MS);
    pwmSetClock(192);
    pwmSetRange(2000);

    while(true) {
        int val;
        cin >> val;
        servo->Write(val);
    }
}