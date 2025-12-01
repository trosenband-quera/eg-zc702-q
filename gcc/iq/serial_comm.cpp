#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <getopt.h>
#include <sys/time.h>

#include <string>

#include "Novatech409c.h"

using namespace std;
Novatech409c* dds;

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

    dds = new Novatech409c(device, baudrate);

    // Benchmarking
    struct timeval t0, t1;

    if(test_table) {
        // test table performance
        dds->echo(false);
        
        
        if (benchmark) {
            gettimeofday(&t0, NULL);
        }
        const int num_rows = 100;
        for(int i=0; i<test_table; i++) {
            int r = rand() % num_rows;
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "TS %d", r);
            int ret = dds->writeCommand(cmd, 10);
            
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
        dds->echo(false);
        
        
        if (benchmark) {
            gettimeofday(&t0, NULL);
        }
        for(int r=0; r<table_rows; r++) {
            int ch = 1 + r % 3; // channels 1,2,3
            double f = (r / 30.0) + 1.0e6; // frequencies starting from 1.0 MHz
            dds->makeTableRow(r, ch, f);

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
        dds->echo(false);
    
        if (benchmark) {
            gettimeofday(&t0, NULL);
        }
        for(int i=0; i<phase; i++) {
            int ch = i % 4;
            float phi = ((i / 4) % 8) * 45.0;
            dds->setPhaseDeg(ch, phi);
            //usleep(1000); // 1 ms delay
        }
        if (benchmark) {
            gettimeofday(&t1, NULL);
            double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6;
            printf("Benchmark: %.3f ms for %d phase commands, %.3f cmd/ms\n", elapsed * 1000, phase, phase / elapsed / 1000);
        }
    } else if(freq) {
        dds->echo(false);
        gettimeofday(&t0, NULL);
        dds->setFrequencyHz(channel, freq);
        gettimeofday(&t1, NULL);
        double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) ;
        printf("Benchmark: %.3f ms\n", elapsed * 1e-3);
    } else {
        // Print the message being sent
        string message = message0 ? string(message0) : "q";
        printf("Sending: %s\n", message.c_str());
        fflush(stdout);   

        if (benchmark) {
            gettimeofday(&t0, NULL);
        }
        // Write message
        dds->writeCommand(message, 1000, true);
        if (benchmark) {
            gettimeofday(&t1, NULL);
            double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6;
            printf("Benchmark: %.3f ms\n", elapsed * 1000);
        }
    }
    dds->close();
    return 0;
}
