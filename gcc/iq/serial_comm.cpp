#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string>
#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <serial_device> <message>\n", argv[0]);
        return 1;
    }

    const char *device = argv[1];
    string message = argv[2] + string("\r\n");

    int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        return 1;
    }

    // Set baud rate, 8N1, no flow control
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo, no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);        // ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag &= ~CSTOPB; // Ensure one stop bit is set
    //  tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(fd);
        return 1;
    }

    // Print the message being sent
    printf("Sending: %s", message.c_str());
    fflush(stdout);

    // Write message
    write(fd, message.c_str(), message.length());
    // Read response (optional)
    int maxlines = 20;
    int linecount = 0;
    int num_newline = 0;
    int timeout_ms = 200; // 0.2 seconds timeout
    int elapsed_ms = 0;
    while (linecount++ < maxlines)
    { 
        char buf[100];
        int n = read(fd, buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = 0;
            elapsed_ms = 0; // reset timeout if data received
            for (int i = 0; i < n; ++i) {
                if (buf[i] == '\n')
                {
                    num_newline++;
                    //printf("<0x%02X>", buf[i]);
                    if (0 == num_newline % 2)
                        cout << endl;        
                }
                else
                {
                    putchar(buf[i]);
                }
            }
            fflush(stdout);
        } else {
            usleep(100000); // sleep 100ms
            elapsed_ms += 100;
            if (elapsed_ms >= timeout_ms) {
                printf("\nResponse timeout reached (%d ms).\n", timeout_ms);
                break;
            }
        }
    }
    close(fd);
    return 0;
}