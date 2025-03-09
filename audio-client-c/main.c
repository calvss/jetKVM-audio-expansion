#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#define SERIAL_PORT_FILE_NAME "/dev/ttyS3"

int main(int, char**){
    int serialFile = open(SERIAL_PORT_FILE_NAME, O_RDWR);

    if(serialFile < 0)
    {
        fprintf(stderr, "Error %d opening tty file %s: %s", errno, SERIAL_PORT_FILE_NAME, strerror(errno));
        return errno;
    }

    struct termios tty;
    memset (&tty, 0, sizeof tty);

    if(tcgetattr(serialFile, &tty) != 0)
    {
        fprintf(stderr, "Error %d from tcgetattr: %s", errno, strerror(errno));
        return errno;
    }

    //=================================================
        // set UART baud rate to 3,000,000
        // 8 data bits, no parity, 1 stop bit
    //=================================================
    {

        cfsetospeed(&tty, B3000000);
        cfsetispeed(&tty, B3000000);

        // disable parity
        tty.c_cflag &= ~PARENB;

        // signle stop bit
        tty.c_cflag &= ~CSTOPB;

        // 8 data bits
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        
        // CLOCAL disables SIGHUP when modem disconnected
        // CREAD allows reading from the serial device
        tty.c_cflag |= CREAD | CLOCAL;

        // Disable canonical mode (don't buffer data line-by-line, we want bytes to come in immediately)
        tty.c_lflag &= ~ICANON;

        // These bits might not do anything anyway if canonical is disabled
        // It's here just to be safe
        tty.c_lflag &= ~ECHO; // Disable echo
        tty.c_lflag &= ~ECHOE; // Disable erasure
        tty.c_lflag &= ~ECHONL; // Disable new-line echo

        // Don't interpret special signal characters (INTR, QUIT, SUSP)
        // We just want the raw bytes
        tty.c_lflag &= ~ISIG;

        // Disable software flow control
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);

        // Disable any special handling of received bytes, we just want the raw data
        tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);

        tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
        tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

        // Set to block reads until some data is available, or timeout occurs
        // Set timeout to 1s
        // Thes variables are defined as cc_t type (unsigned char for the jetKVM buildroot)
        tty.c_cc[VTIME] = 10; // timeout = 10 deciseconds = 1 second
        tty.c_cc[VMIN] = 100; // read() will return on timeout even if no bytes are read

        // Save tty settings, also checking for error
        // TCSANOW means write changes to settings immediately
        if (tcsetattr(serialFile, TCSANOW, &tty) != 0) {
            fprintf(stderr, "Error %d from tcsetattr: %s\n", errno, strerror(errno));
            return errno;
        }

        //=================================================
        // end UART settings
        //=================================================
    }

    // flush serial buffer before working so we're not constantly 10s behind
    tcflush(serialFile, TCIOFLUSH);

    // discard bytes until we hit 0xffff
    while(1)
    {
        // discard bytes until we reach the first 0xff
        char charbuf;
        do
        {
            if(read(serialFile, &charbuf, 1) <=0)
            {
                fprintf(stderr, "EOF before reaching packet boundary");
                charbuf = 0;
            }
        } while(charbuf != 0xff);

        if(read(serialFile, &charbuf, 1) <=0)
        {
            fprintf(stderr, "EOF before reaching packet boundary");
            charbuf = 0;
        }

        if(charbuf == 0xff)
        {
            // 0xffff was reached, stop discarding bytes
            break;
        }
        else
        {
            // just a random isolated 0xff, keep discarding bytes
            ;
        }
    }

    // now that we reached the start of a packet, start decoding bytes
    // keep track of which channel we're working on, since samples are interleaved L-R-L-R...
    bool leftChannel = false;
    // each audio frame is two int16s
    int16_t frame[2];
    const uint8_t packetBoundary[2] = {0xff, 0xff};
    while(1)
    {
        // read 2 bytes at a time (each ADC sample is two bytes)
        uint8_t buffer[2];

        int n = read(serialFile, buffer, 2);
        if(n != 2)
        {
            fprintf(stderr, "Error %d: %s\n", errno, strerror(errno));
            continue;
        }
        else if(memcmp(buffer, packetBoundary,2) == 0)
        {
            // if we hit a packet boundary, skip decoding its value
            continue;
        }
        else
        {
            // else convert into an int16
            // shift the upper byte 8 bits to the left
            // combine with lower bytes to create a uint16
            // cast uint16_t to int32_t so we don't lose data converting from unsigned to signed
            // shift it by 2048 so we remove the DC bias from the waveform
            // cast back into int16_t for PCM standard data type

            // note: technically we don't need to convert to int16 here
            // Just dump the bytes straight back out into stdout and it should work since the Pico board already dumps a pseudo-PCM16 stream
            // But converting it to int16 here will make it easier to add Vorbis encoding someday
            // Perhaps even hosting a RTSP stream from the jetKVM directly so any media client can receive it, like VLC or mpv or something
            if(leftChannel)
            {
                frame[0] = (int16_t)((int32_t)(( (uint16_t)buffer[1] << 8 ) | buffer[0]) - 2048) << 4;
                leftChannel = false;
            }
            else
            {
                frame[1] = (int16_t)((int32_t)(( (uint16_t)buffer[1] << 8 ) | buffer[0]) - 2048) << 4;
                leftChannel = true;

                // once we have the right channel, the frame is complete.

                fwrite(frame, sizeof(int16_t), 2, stdout);
            }
        }
    }
}
