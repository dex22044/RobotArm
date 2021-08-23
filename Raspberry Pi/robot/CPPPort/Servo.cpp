#include "Servo.h"

Servo::Servo(int pin) {
    this->pin = pin;

    pinMode(pin, PWM_OUTPUT);
}

void Servo::Write(int degrees) {
    if(degrees < 0) degrees = 0;
    if(degrees > 180) degrees = 180;
    pwmWrite(pin, 55 + (degrees / 180.0 * 175.0));
}