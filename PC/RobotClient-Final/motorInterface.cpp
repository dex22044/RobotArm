#include "motorInterface.h"

int serialfd;
char line[256];

void initInterface() {
    serialfd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY | O_SYNC);

    {
        struct termios tty;
        memset(&tty, 0, sizeof tty);
        if (tcgetattr (serialfd, &tty) != 0)
        {
            std::cout << "Error " << errno << " from tcgetattr: " << strerror(errno) << "\n";
        }

        cfsetospeed (&tty, B115200);
        cfsetispeed (&tty, B115200);
        tty.c_cflag     &=  ~PARENB;
        tty.c_cflag     &=  ~CSTOPB;
        tty.c_cflag     &=  ~CSIZE;
        tty.c_cflag     |=  CS8;
        tty.c_cflag     &=  ~CRTSCTS;
        tty.c_lflag     =   0;
        tty.c_oflag     =   0;
        tty.c_cc[VMIN]  =   0;
        tty.c_cc[VTIME] =   5;

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

void writeMotors(int servo1, int servo2, int stepper) {
    while(true) {
        int s1, s2;
        std::cin >> s1 >> s2;
        if(s1 < 0) exit(0);
        int lineLen = snprintf(line, 256, "%d %d 0 \n", s1, s2);
        write(serialfd, line, lineLen);
    }
}