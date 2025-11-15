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

    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL); // shut off xon/xoff ctrl, no CR -> LF translation
    tty.c_iflag |= IGNCR; // ignore CR
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
    // Read response until 'OK\n' or maxlength or timeout
    int maxlength = 2000;
    int charcount = 0;
	bool showspecial = false;
	string ok("OK\n");
	unsigned iok = 0;
	printf("[BEGIN RESPONSE]\n");
    while (charcount++ < maxlength)
    { 
        char c;
        int n = read(fd, &c, 1);
        if (n > 0) {
			if(showspecial && c < 16)
				printf("<0x%02X>", c);

			putchar(c);
			if (c == '\n')
				fflush(stdout);
				
			if(c == ok[iok]) {
				iok++;
				if (iok == ok.length()) {
					printf("[END RESPONSE, length: %d]\n", charcount);
					break;
				}
			}
        } else {
			printf("[TIMEOUT]\n");
            break;
        }
    }
    close(fd);
    
    if(charcount >= maxlength)
		printf("[TOO LONG]\n");
		
    return 0;
}
