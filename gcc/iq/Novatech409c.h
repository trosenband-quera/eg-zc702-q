#pragma once
#include <string>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>

class Novatech409c {
public:
    Novatech409c(const std::string& device, int baudrate);
    ~Novatech409c();

    void setPhaseDeg(unsigned channel, double phase);
    void setFrequencyHz(unsigned channel, double frequency);
    void echo(bool enable);
    void makeTableRow(int row, int ch, double freq, float phase=0,
                      float A=1, int dwell=100);
                      
    int writeCommand(const std::string& cmd, size_t length=0, bool verbose=false);
    int readResponse(size_t maxlength, bool verbose=false);
    void close();

private:
    std::string devicePath;
    const unsigned channelCount = 4;
    int fd;
    struct termios tty;
    void configurePort(int baudrate);
};
