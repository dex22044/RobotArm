#pragma once

#include <wiringPi.h>
#include <chrono>
#include <unistd.h>
#include <cstdio>

using namespace std;

class StepperMotor {
public:
    int portStep;
    int portDir;

    int64_t currentPosition = 0;
    int64_t neededPosition = 0;
    int stepDelay = 500;

    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;

    StepperMotor(int stp, int dir);
    bool Step();
};