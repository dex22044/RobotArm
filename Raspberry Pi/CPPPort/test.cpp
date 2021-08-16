#include <iostream>
#include <unistd.h>			//Used for UART
#include <wiringSerial.h>
#include <wiringPi.h>

using namespace std;

int main(int argc, char** argv)
{
    wiringPiSetup();
	int serialfd = serialOpen("/dev/ttyAMA0", 115200);

    while(true) {
        for(char i = 0; i < 180; i++) {
            serialPutchar(serialfd, i);
            usleep(15000);
        }
        for(char i = 180; i > 0; i--) {
            serialPutchar(serialfd, i);
            usleep(15000);
        }
    }
	
	serialClose(serialfd);
}