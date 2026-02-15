#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#include <string>

using namespace std;


static volatile int keep_running = 1;

static void handle_sigint(int) {
    keep_running = 0;
}

class udp_sender {
public:
    string ip;
    int port = 50000;
    double freq_hz = 1.0;   // sine frequency
    double send_rate_hz = 100.0; // packet rate
    double dt;
    struct sockaddr_in addr;
    int sock;
    std::string buffer;
    iq_proto::plotData data_msg;

    udp_sender(const string& ip_addr, int port_num = 50000) : ip(ip_addr), port(port_num) {
        dt = 1.0 / send_rate_hz;
    
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            perror("socket");
            throw std::runtime_error("Failed to create socket");
        }

        
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)port);
        if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
            fprintf(stderr, "inet_pton failed for %s\n", ip.c_str());
            fprintf(stderr, "length: %zu\n", ip.length());
            for(size_t i = 0; i < ip.length(); i++) {
                fprintf(stderr, "ip[%zu] = %d ('%c')\n", i, (unsigned char)ip[i], 
                        isprint(ip[i]) ? ip[i] : '?');
            }

            close(sock);
            throw std::runtime_error("Failed to convert IP address");
        }

        // Handle kill/interrupt signals for clean exit
        signal(SIGINT, handle_sigint);
        signal(SIGTERM, handle_sigint);
    }

    void send_data(const vector<float>& x) {
        if(x.size() != (size_t)data_msg.values_size()) {
            data_msg.clear_values();
            for (unsigned i = 0; i < x.size(); ++i) {
                data_msg.add_values(x[i]);
            }
        }
        else {
            // If the size matches, just update the existing values
            for (unsigned i = 0; i < x.size(); ++i) {
                data_msg.set_values(i, x[i]);
            }
        }
        if (!data_msg.SerializeToString(&buffer)) {
            std::cerr << "Failed to serialize message.\n";
            return;
        }

        // Send
        ssize_t n = sendto(sock, buffer.data(), buffer.size(), 0,
                        (struct sockaddr*)&addr, sizeof(addr));
        if (n < 0) {
            perror("sendto");
            std::cerr << "Failed to send message. sock = " << sock << std::endl;
            throw std::runtime_error("Failed to send data message");
        } else {
            // Successfully sent
            // printf("Sent %zd bytes to %s:%d\n", n, ip.c_str(), port);
            // printf("%s\n", data_msg.DebugString().c_str());
        }
    }

    void send_channel_info(const iq_proto::plotData& info_msg) {
        if (!info_msg.SerializeToString(&buffer)) {
            std::cerr << "Failed to serialize message.\n";
            return;
        }

        // Send
        ssize_t n = sendto(sock, buffer.data(), buffer.size(), 0,
                        (struct sockaddr*)&addr, sizeof(addr));
        if (n < 0) {
            perror("sendto");
            throw std::runtime_error("Failed to send channel info message");
            // Successfully sent
            // printf("Sent %zd bytes to %s:%d\n", n, ip.c_str(), port);
            // printf("%s\n", data_msg.DebugString().c_str());
        }
    }
    ~udp_sender() {
        close(sock);
    }
};
