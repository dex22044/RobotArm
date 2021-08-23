#include <iostream>
#include <unistd.h>
#include <wiringPi.h>

class Servo {
private:
    int pin;

public:
    Servo(int pin);
    void Write(int degrees);
};