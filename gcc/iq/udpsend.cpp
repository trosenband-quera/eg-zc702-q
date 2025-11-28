// udp_sender.c
// Sends a 32-bit little-endian float over UDP at 100 Hz (1 Hz sine wave)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

// Helper: sleep for N milliseconds (coarse but sufficient for demo)
static void msleep(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

// Convert host float to little-endian byte array (portable)
static void float_to_le_bytes(float f, unsigned char out[4]) {
    // Reinterpret bytes
    union {
        float f;
        unsigned char b[4];
    } u;
    u.f = f;

    // Detect endianness at runtime
    int x = 1;
    int is_little = *(char*)&x == 1;

    if (is_little) {
        // Host is little-endian: copy as-is
        out[0] = u.b[0];
        out[1] = u.b[1];
        out[2] = u.b[2];
        out[3] = u.b[3];
    } else {
        // Host is big-endian: reverse
        out[0] = u.b[3];
        out[1] = u.b[2];
        out[2] = u.b[1];
        out[3] = u.b[0];
    }
}

static volatile int keep_running = 1;

static void handle_sigint(int sig) {
    keep_running = 0;
}

int main(int argc, char** argv) {
    const char* ip = "127.0.0.1";
    int port = 50000;
    double freq_hz = 1.0;   // sine frequency
    double send_rate_hz = 100.0; // packet rate
    double dt = 1.0 / send_rate_hz;

    if (argc >= 2) ip = argv[1];     // optional: target IP
    if (argc >= 3) port = atoi(argv[2]); // optional: target port

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "inet_pton failed for %s\n", ip);
        close(sock);
        return 1;
    }

    // Handle kill/interrupt signals for clean exit
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    // Timebase: monotonic clock
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    printf("Sending UDP floats to %s:%d at %.1f Hz (sine %.1f Hz)\n",
           ip, port, send_rate_hz, freq_hz);

    for (; keep_running;) {
        // Current time
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double t = (now.tv_sec - t0.tv_sec) + (now.tv_nsec - t0.tv_nsec) / 1e9;

        // 1 Hz sine
        float x = (float)sin(2.0 * M_PI * freq_hz * t);

        // Pack as little-endian float
        unsigned char payload[4];
        float_to_le_bytes(x, payload);

        // Send
        ssize_t n = sendto(sock, payload, sizeof(payload), 0,
                           (struct sockaddr*)&addr, sizeof(addr));
        if (n < 0) {
            perror("sendto");
            break;
        }

        // Sleep to maintain ~100 Hz
        msleep((long)(dt * 1000.0));
    }

    close(sock);
    return 0;
}
