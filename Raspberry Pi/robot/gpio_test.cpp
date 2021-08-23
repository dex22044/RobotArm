#include <iostream>
#include <unistd.h>
#include <wiringPi.h>

using namespace std;

int main(int argc, char** argv)
{
    wiringPiSetupGpio();

    pinMode(17, OUTPUT);
    pinMode(18, OUTPUT);
    pinMode(19, OUTPUT);

    while(true) {
        digitalWrite(17, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        digitalWrite(18, 1);
        digitalWrite(19, 1);
        for(int i = 0; i < 10; i++);
        digitalWrite(17, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        digitalWrite(18, 0);
        digitalWrite(19, 0);
        for(int i = 0; i < 10; i++);
    }
}