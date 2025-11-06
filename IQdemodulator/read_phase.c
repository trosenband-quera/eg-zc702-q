#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

int main() {
    int fd = open("/dev/axi_iq_demodulator", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    uint32_t phase_data[3];
    ssize_t bytes = read(fd, phase_data, sizeof(phase_data));
    if (bytes != sizeof(phase_data)) {
        perror("Failed to read phase data");
        close(fd);
        return 1;
    }

    printf("Phase 100kHz: 0x%04x\n", phase_data[0] & 0xFFFF);
    printf("Phase 110kHz: 0x%04x\n", phase_data[1] & 0xFFFF);
    printf("Phase 120kHz: 0x%04x\n", phase_data[2] & 0xFFFF);

    close(fd);
    return 0;
}
