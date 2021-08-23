#include <iostream>
#include <unistd.h>
#include <wiringPi.h>
#include "StepperMotor.h"

int main() {
    wiringPiSetupGpio();
    StepperMotor* motor1 = new StepperMotor(27, 22);

    while(true) {
        motor1->neededPosition++;
        while(motor1->Step());
    }
}