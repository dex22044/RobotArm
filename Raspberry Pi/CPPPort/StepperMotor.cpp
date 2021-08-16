#include "StepperMotor.h"

StepperMotor::StepperMotor(int stp, int dir) {
    portStep = stp;
    portDir = dir;
    pinMode(stp, OUTPUT);
    pinMode(dir, OUTPUT);
}

bool StepperMotor::Step() {
    if(currentPosition < neededPosition) {
        auto duration = std::chrono::high_resolution_clock::now() - lastTime;
        long long micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        if(micros > stepDelay) {
            digitalWrite(portDir, 1);
            lastTime = std::chrono::high_resolution_clock::now();
            digitalWrite(portStep, 1);
            usleep(50);
            digitalWrite(portStep, 0);
            currentPosition++;
        }
        return true;
    }
    if(currentPosition > neededPosition) {
        auto duration = std::chrono::high_resolution_clock::now() - lastTime;
        long long micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        if(micros > stepDelay) {
            digitalWrite(portDir, 0);
            lastTime = std::chrono::high_resolution_clock::now();
            digitalWrite(portStep, 1);
            usleep(50);
            digitalWrite(portStep, 0);
            currentPosition--;
        }
        return true;
    }
    return false;
}