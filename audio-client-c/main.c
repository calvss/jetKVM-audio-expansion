#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>

#define SERIAL_PORT_FILE "/dev/ttyS3"

int main(int, char**){
    int serialFile = open(SERIAL_PORT_FILE, O_RDWR | O_NONBLOCK);

    if(serialFile < 0)
    {
        fprintf(stderr, "Error %d opening tty file %s: %s", errno, SERIAL_PORT_FILE, strerror(errno));
        return errno;
    }

    struct termios tty;
    memset (&tty, 0, sizeof tty);

    if(tcgetattr(serialFile, &tty) != 0)
    {
        fprintf(stderr, "Error %d getting serial settings: %s", errno, strerror(errno));
        return errno;
    }

    cfsetospeed(&tty, B3000000);
    cfsetispeed(&tty, B3000000);
}
