#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <getopt.h>
#include <sys/time.h>

#include <string>

using namespace std;

int open_serial(const char* device, int baudrate) {
    printf("Opening serial port: %s at baudrate %d\n", device, baudrate);

    int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    // Set baud rate, 8N1, no flow control
    speed_t speed = B115200;
    switch (baudrate) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
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
        return -1;
    }

    return fd;
}

void send_command(int fd, const string& command, bool verbose = false) {
    string cmd = command + "\r\n";
    if (verbose) {
        timeval tv;
        gettimeofday(&tv, NULL);
        printf("[%ld.%06ld s] Sending command: %s", tv.tv_sec, tv.tv_usec, cmd.c_str());
    }
    write(fd, cmd.c_str(), cmd.length());
}

int get_response(int fd, char* response, int maxlength, bool verbose = false) {
    int total_read = 0;
    while (total_read+1 < maxlength) {
        int n = read(fd, response + total_read, maxlength - total_read - 1);
        if (n > 0) {
            response[total_read + n] = '\0';
            total_read += n;
            if (verbose) {
                timeval tv;
                gettimeofday(&tv, NULL);
                printf("[%ld.%06ld s] Received chunk: %s", tv.tv_sec, tv.tv_usec, 
                        response + total_read - n);
            }
            // Check for termination condition (e.g., "OK\n")
            if (strstr(response, "OK\n") != NULL) {
                return 0; // Successfully received complete response
            }
        } else {
            return 1; // Timeout or error
        }
    }

    return 2; // Response too long
}

int make_table_row(int fd, bool verbose, int row, int ch,double freq, float phase=0, 
                   float A=1, int dwell=100) {
    char msg[128];
    snprintf(msg, sizeof(msg), "T %d %d %d %.7lf %.2f %.3f", 
             row, dwell, ch, freq, phase, A);
    send_command(fd, msg, verbose);
    return get_response(fd, msg, 63, verbose);
}

int main(int argc, char *argv[]) {
    char *device = NULL;
    char *message0 = NULL;
    int baudrate = 115200;
    int benchmark = 0;
    int phase = 0;
    int freq = 0;
    int channel = 0;
    int table_rows = 0;
    int test_table = 0;
    int opt;

    while ((opt = getopt(argc, argv, "d:m:b:BP:F:T:t:c:")) != -1) {
        switch (opt) {
            case 'd':
                device = optarg;
                break;
            case 'm':
                message0 = optarg;
                break;
            case 'b':
                baudrate = atoi(optarg);
                break;
            case 'B':
                benchmark = 1;
                break;
            case 'P':
                phase = atoi(optarg);
                break;
            case 'F':
                freq = atoi(optarg);
                break;
            case 'T':
                table_rows = atoi(optarg);
                break;
            case 't':
                test_table = atoi(optarg);
                break;
            case 'c':
                channel = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -d <device> [-m <message>] [-b baudrate] [-B]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!device) {
        fprintf(stderr, "Usage: %s -d <device> [-m <message>] [-b baudrate] [-B]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int fd = open_serial(device, baudrate);
    if (fd < 0) {
        return 1;
    }

    // Benchmarking
    struct timeval t0, t1;
    char response[1024];
    if(test_table) {
        // test table performance
        send_command(fd, "E d"); // disable echo
        get_response(fd, response, 10);
        
        if (benchmark) {
            gettimeofday(&t0, NULL);
        }
        const int num_rows = 100;
        for(int i=0; i<test_table; i++) {
            int r = rand() % num_rows;
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "TS %d", r);
            send_command(fd, cmd);
            int ret = get_response(fd, cmd, sizeof(cmd));
            if(ret != 0) {
                ///printf("Error setting table row %d, ch %d\n", r, ch);
            }
        }
        if (benchmark) {
            gettimeofday(&t1, NULL);
            double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6;
            printf("Benchmark: %.3f ms for %d table rows, %.3f row/ms\n", elapsed * 1000, 
                   test_table, test_table / elapsed / 1000);
        }
    } else
    if(table_rows) {
        send_command(fd, "E d"); // disable echo
        get_response(fd, response, 10);
        
        if (benchmark) {
            gettimeofday(&t0, NULL);
        }
        for(int r=0; r<table_rows; r++) {
            int ch = 1 + r % 3; // channels 1,2,3
            double f = (r / 3) * 1e-7 + 1.0; // frequencies starting from 1.0 MHz
            int ret = make_table_row(fd, true, r, ch, f);
            if(ret != 0) {
                ///printf("Error setting table row %d, ch %d\n", r, ch);
            }
            if(r % 10 == 0) {
                printf("Set table row %d/%d\n", r, table_rows);
            }
        }
        if (benchmark) {
            gettimeofday(&t1, NULL);
            double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6;
            printf("Benchmark: %.3f ms for %d table rows, %.3f row/ms\n", elapsed * 1000, table_rows, table_rows / elapsed / 1000);
        }
    } else if(phase) {
        send_command(fd, "E d"); // disable echo
        get_response(fd, response, 10);
        
        if (benchmark) {
            gettimeofday(&t0, NULL);
        }
        for(int i=0; i<phase; i++) {
            char cmd[32];
            int ch = i % 4;
            float phi = ((i / 4) % 8) * 45.0;
            snprintf(cmd, sizeof(cmd), "P%d %.2f", ch, phi);
            if(i % 1 == 0) {
                printf("Setting phase %6d: %s\n", i, cmd);
            }
            //printf("Setting phase: %s\n", cmd);
            send_command(fd, cmd);
            get_response(fd, response, 10);
            //usleep(1000); // 1 ms delay
        }
        if (benchmark) {
            gettimeofday(&t1, NULL);
            double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6;
            printf("Benchmark: %.3f ms for %d phase commands, %.3f cmd/ms\n", elapsed * 1000, phase, phase / elapsed / 1000);
        }
    } else if(freq) {
        send_command(fd, "E d"); // disable echo
        get_response(fd, response, 10);
        gettimeofday(&t0, NULL);
        char cmd[32];
        double f = 1.0 + freq * 1e-6;
        snprintf(cmd, sizeof(cmd), "F%d %.7lf", channel, f);
        printf("Setting frequency %6d: %s MHz\n", freq, cmd);
        //printf("Setting frequency: %s\n", cmd);
        send_command(fd, cmd);
        get_response(fd, response, 10);
        //usleep(1000); // 1 ms delay
        gettimeofday(&t1, NULL);
        double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6;
        printf("Benchmark: %.3f ms for %d frequency commands, %.3f cmd/ms\n", elapsed * 1000, freq, freq / elapsed / 1000);
    } else {
        // Print the message being sent
        string message = message0 ? string(message0) : "q";
        printf("Sending: %s\n", message.c_str());
        fflush(stdout);   

        if (benchmark) {
            gettimeofday(&t0, NULL);
        }
        // Write message
        send_command(fd, message);
        int r = get_response(fd, response, 1024);
        printf("[BEGIN RESPONSE]\n");
        printf(response);
        if(r == 0) {
            printf("[END RESPONSE, length: %zu]\n", strlen(response));
        } else if (r == 1) {
            printf("[TIMEOUT]\n");
        } else if (r == 2) {
            printf("[TOO LONG]\n");
        }
        if (benchmark) {
            gettimeofday(&t1, NULL);
            double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6;
            printf("Benchmark: %.3f ms, %.1f char/ms\n", elapsed * 1000, 
                   strlen(response) / elapsed / 1000);
        }
    }

    close(fd);
		
    return 0;
}
