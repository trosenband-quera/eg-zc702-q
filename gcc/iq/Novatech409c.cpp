#include "Novatech409c.h"
#include <iostream>
#include <cstring>
#include <sys/time.h>
#include <assert.h>

Novatech409c::Novatech409c(const std::string& device, int baudrate)
    : devicePath(device), fd(-1) {
    fd = open(device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        throw std::runtime_error("Failed to open serial port: " + device);
    }
    configurePort(baudrate);
}

Novatech409c::~Novatech409c() {
    close();
}

void Novatech409c::configurePort(int baudrate) {
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        throw std::runtime_error("Error from tcgetattr");
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

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        throw std::runtime_error("Error from tcsetattr");
    }
}

void Novatech409c::echo(bool enable) {
    char buf[16];
    snprintf(buf, sizeof(buf), "E %c", enable ? 'e' : 'd');
    writeCommand(buf);
    readResponse(10);
}

void Novatech409c::setPhaseDeg(unsigned channel, double phase) {
    if (channel >= channelCount) {
        throw std::out_of_range("Channel number out of range");
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "P%d %.2f", channel, phase);
    writeCommand(buf, 10);
}

void Novatech409c::setFrequencyHz(unsigned channel, double frequency) {
    if (channel >= channelCount) {
        throw std::out_of_range("Channel number out of range");
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "F%d %.7f", channel, frequency*1e-6);
    writeCommand(buf, 10);
}

void Novatech409c::makeTableRow(int row, int ch, double freq, float phase,
                                 float A, int dwell) {
    char buf[128];
    snprintf(buf, sizeof(buf), "T %d %d %d %.7lf %.2f %.3f",
             row, dwell, ch, freq, phase, A);
    writeCommand(buf, 63);
}
//    snprintf(msg, sizeof(msg), "T %d %d %d %.7lf %.2f %.3f", 
//             row, dwell, ch, freq, phase, A);
int Novatech409c::writeCommand(const std::string &cmd, size_t length, bool verbose)
{
    std::string cmd_with_newline = cmd + "\r\n";
    if (fd < 0)
        throw std::runtime_error("Serial port not open");
    int n = ::write(fd, cmd_with_newline.c_str(), cmd_with_newline.size());
    if (n < 0)
        throw std::runtime_error("Failed to write to serial port");
    if (length == 0)
    {
        return 0;
    }
    else
        return readResponse(length, verbose);
}

int Novatech409c::readResponse(size_t maxlength, bool verbose)
{
    if (fd < 0)
        throw std::runtime_error("Serial port not open");
    char response[1024];
    assert(sizeof(response) > maxlength); // "Response buffer too small");
    size_t total_read = 0;

    if (verbose)
        printf("[BEGIN RESPONSE]\n");
    while (total_read + 1 < maxlength)
    {
        int n = read(fd, response + total_read, maxlength - total_read - 1);
        if (n > 0)
        {
            response[total_read + n] = '\0';
            total_read += n;
            if (verbose)
                printf("%s", response + total_read - n);
            // Check for termination condition (e.g., "OK\n")
            if (strstr(response, "OK\n") != NULL)
            {
                if (verbose)
                    printf("[END RESPONSE]\n");
                return 0; // Successfully received complete response
            }
        }
        else
        {
            if (verbose)
                printf("[TIMEOUT]\n");
            return 1; // Timeout or error
        }
    }
    if (verbose)
        printf("[TOO LONG]\n");

    return 2; // Response too long
}

void Novatech409c::close()
{
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }
}
