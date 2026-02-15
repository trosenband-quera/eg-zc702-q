#include <iostream>
#include <string>
#include <unistd.h>
#include <cmath>
#include "message.pb.h"
#include "udp_sender.h"


#define SERVER_IP "10.0.0.1"
#define SERVER_PORT 50000

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    udp_sender sender(SERVER_IP, SERVER_PORT);

    // Serialize to string
    std::string buffer;
    float A=2500;
    float B=2000;
    vector<float> udp_x(4);

    for(unsigned i = 0; i < 1000; ++i) {
        udp_x[0] = A+B*sin(i * 0.1f);
        udp_x[1] = A+B*sin(i * 0.1f + 1.0f);
        udp_x[2] = A+B*sin(i * 0.1f + 2.0f);
        udp_x[3] = A+B*sin(i * 0.1f + 3.0f);
        sender.send_data(udp_x);

        usleep(10000); // 10 ms delay
    }

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}

