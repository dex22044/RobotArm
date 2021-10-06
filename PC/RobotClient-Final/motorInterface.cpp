#include "motorInterface.h"

int serialfd;
char line[64];

void initInterface() {
    serialfd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY);

    {
        struct termios tty;
        memset(&tty, 0, sizeof tty);
        if (tcgetattr (serialfd, &tty) != 0)
        {
            std::cout << "Error " << errno << " from tcgetattr: " << strerror(errno) << "\n";
        }

        cfsetospeed (&tty, B57600);
        cfsetispeed (&tty, B57600);
        tty.c_cflag     &=  ~PARENB;
        tty.c_cflag     &=  ~CSTOPB;
        tty.c_cflag     &=  ~CSIZE;
        tty.c_cflag     |=  CS8;
        tty.c_cflag     &=  ~CRTSCTS;
        tty.c_lflag     =   0;
        tty.c_oflag     =   0;
        tty.c_cc[VMIN]  =   0;
        tty.c_cc[VTIME] =   1;

        tty.c_cflag     |=  CREAD | CLOCAL;
        tty.c_iflag     &=  ~(IXON | IXOFF | IXANY);
        tty.c_lflag     &=  ~(ICANON | ECHO | ECHOE | ISIG);
        tty.c_oflag     &=  ~OPOST;

        tcflush(serialfd, TCIFLUSH);

        if (tcsetattr(serialfd, TCSANOW, &tty) != 0)
        {
            std::cout << "Error " << errno << " from tcsetattr\n";
        }
    }
}

void writeMotors(int servo1, int servo2, int servo3, int servo4, int stepper) {
    int lineLen = snprintf(line, 63, "%d,%d,%d,%d,%d\n", servo1, servo2, servo3, servo4, stepper);
    write(serialfd, line, lineLen);
}