#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <unistd.h>
#include <cmath>
#include "message.pb.h"

#define SERVER_IP "10.0.0.1"
#define SERVER_PORT 50000

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    // Serialize to string
    std::string buffer;
    float A=2500;
    float B=2000;
    for(unsigned i = 0; i < 1000; ++i) {
        // Create and populate Protobuf message
        iq_proto::plotData msg;
        msg.add_values(A+B*sin(i * 0.1f));
        msg.add_values(A+B*sin(i * 0.1f + 1.0f));
        msg.add_values(A+B*sin(i * 0.1f + 2.0f));
        msg.add_values(A+B*sin(i * 0.1f + 3.0f));

        
        if (!msg.SerializeToString(&buffer)) {
            std::cerr << "Failed to serialize message.\n";
            close(sock);
            return 1;
        }

        // Send over UDP
        ssize_t sent = sendto(sock, buffer.data(), buffer.size(), 0,
                            (sockaddr*)&serverAddr, sizeof(serverAddr));
        if (sent < 0) {
            perror("Send failed");
        }
        usleep(10000); // 10 ms delay
    }

    close(sock);
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}

