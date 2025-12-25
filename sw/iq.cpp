#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fstream>
#include <time.h>
#include <iostream>
#include <sstream>
#include <map>

#define GPIO_BASE_ADDR  0x41200000
#define XADC_BASE_ADDR  0x43C00000  
#define XADC_SPAN       0x10000

#include <sys/time.h>
#include <getopt.h>
#include <cstring>
#include <vector>
#include <tuple>
#include <string>
#include <cmath>
#include <iomanip>

#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#include "Novatech409c.h"
#include "message.pb.h"
#include <algorithm>

using namespace std;

static volatile int keep_running = 1;

static void handle_sigint(int) {
    keep_running = 0;
}


void set_register(volatile void *map_base, unsigned num, unsigned value) {
    *(volatile unsigned int *)((char *)map_base + num*4) = value;
}

unsigned get_register(volatile void *map_base, unsigned offset) {
    return *(volatile unsigned int *)((char *)map_base + offset);
}

// Function to read configuration from a file
map<string, string> read_config(const string& config_file) {
    map<string, string> config;
    if(config_file.empty()) {
        cout << "No config file specified, using default parameters." << endl;
        return config;
    }

    ifstream infile(config_file);
    if (!infile.is_open()) {
        cerr << "Could not open config file: " << config_file << endl;
        return config;
    }
    string line;
    while (getline(infile, line)) {
        // Remove comments and trim whitespace
        size_t comment_pos = line.find('#');
        if (comment_pos != string::npos) {
            line = line.substr(0, comment_pos);
        }
        // Trim whitespace from both ends
        size_t first = line.find_first_not_of(" \t");
        size_t last = line.find_last_not_of(" \t");
        if (first == string::npos || last == string::npos) {
            continue; // Skip empty or whitespace-only lines
        }
        line = line.substr(first, last - first + 1);

        size_t eq = line.find('=');
        if (eq != string::npos) {
            string key = line.substr(0, eq);
            string value = line.substr(eq + 1);
            config[key] = value;
        }
    }
    return config;
}

unsigned set_frequency(volatile void *map_base, unsigned channel, double freq_Hz, bool verbose=false) {
    const double SAMPLE_RATE_HZ = 1e6 * 25 / 26.0;
    double phase_inc_d = 65536.0 * 65536.0 * freq_Hz / SAMPLE_RATE_HZ;
    unsigned phase_inc = 0.5 + phase_inc_d;
    double freq_Hz_true = phase_inc * SAMPLE_RATE_HZ / 65536.0 / 65536.0;

    if (verbose) {
        printf("LO %d, target = %.3f Hz, true = %.6f Hz, phase delta = %.3f\n", channel, freq_Hz, freq_Hz_true, phase_inc_d);
    }

    set_register(map_base, 4 + channel, phase_inc);
    return phase_inc;
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
    iq_proto::plotData msg;

    udp_sender(string ip_addr) : ip(ip_addr) {
        dt = 1.0 / send_rate_hz;
    
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            perror("socket");
            return;
        }

        
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)port);
        if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
            fprintf(stderr, "inet_pton failed for %s\n", ip.c_str());
            close(sock);
            return;
        }

        // Handle kill/interrupt signals for clean exit
        signal(SIGINT, handle_sigint);
        signal(SIGTERM, handle_sigint);
    }

    void send_data(const vector<float>& x) {
        if(x.size() != (size_t)msg.values_size()) {
            msg.clear_values();
            for (unsigned i = 0; i < x.size(); ++i) {
                msg.add_values(x[i]);
            }
        }
        else {
            // If the size matches, just update the existing values
            for (unsigned i = 0; i < x.size(); ++i) {
                msg.set_values(i, x[i]);
            }
        }
        if (!msg.SerializeToString(&buffer)) {
            std::cerr << "Failed to serialize message.\n";
            return;
        }

        // Send
        ssize_t n = sendto(sock, buffer.data(), buffer.size(), 0,
                        (struct sockaddr*)&addr, sizeof(addr));
        if (n < 0) {
            perror("sendto");
        } else {
            // Successfully sent
            // printf("Sent %zd bytes to %s:%d\n", n, ip.c_str(), port);
            // printf("%s\n", msg.DebugString().c_str());
        }
    }
    ~udp_sender() {
        close(sock);
    }
};

int main(int argc, char *argv[]) {
    udp_sender* sender = 0;
    
    int fd;
    void *map_base;
    unsigned int reg;

    string config_file; // default config file
    if (argc > 1) {
        config_file = argv[1]; // use first argument as config file name
    }

    // Read configuration
    map<string, string> config = read_config(config_file);

    // Set default values
    string udp_ip = config.count("udp_ip") ? config["udp_ip"] : "";
    string udp_plot_channels_str = config.count("udp_plot_channels") ? config["udp_plot_channels"] : "";
    vector<int> udp_plot_channels;
    {
        stringstream ss(udp_plot_channels_str);
        string item;
        while (getline(ss, item, ',')) {
            udp_plot_channels.push_back(std::stoi(item));
        }
    }

    if(!udp_ip.empty()) {
        sender = new udp_sender(udp_ip);
        printf("UDP sender initialized to %s\n", udp_ip.c_str());
        // Example: send 4 floats (remove or move this to your data loop as needed)
        // sender->send_data4(1.1f, 2.2f, 3.3f, 4.4f);
    }
    int reference_ch = config.count("ref") ? std::stoi(config["ref"]) : -1;
    unsigned nmax = config.count("nmax") ? std::stoul(config["nmax"]) : 10;
    double t0_ms = config.count("t0_ms") ? std::stod(config["t0_ms"]) : 0.0;
    double tmax_ms = config.count("tmax_ms") ? std::stod(config["tmax_ms"]) : 1.0;
    int showraw = config.count("showraw") ? std::stoi(config["showraw"]) : 1;
    int showscaled = config.count("showscaled") ? std::stoi(config["showscaled"]) : 0;
    int quiet = config.count("quiet") ? std::stoi(config["quiet"]) : 0;
    double ms = config.count("ms") ? std::stod(config["ms"]) : 5;
    double ms0 = config.count("ms0") ? std::stod(config["ms0"]) : 0.0;
    std::string csv_filename = config.count("csv_filename") ? config["csv_filename"] : "";
    unsigned kp = config.count("kp") ? std::stoul(config["kp"]) : 0;
    unsigned ki = config.count("ki") ? std::stoul(config["ki"]) : 0;
    unsigned signal_good_threshold = config.count("signal_good_threshold") ? std::stoul(config["signal_good_threshold"]) : 0;
    std::string channel_list_str = config.count("channels") ? config["channels"] : "";
    std::string freqs_str = config.count("freqs") ? config["freqs"] : "100000,112500";
    std::string dds_device = config.count("dds_device") ? config["dds_device"] : "";
    double kp_dds = config.count("kp_dds") ? std::stod(config["kp_dds"]) : 0.0;
    double ki_dds = config.count("ki_dds") ? std::stod(config["ki_dds"]) : 0.0;
    double dds_update_interval_ms = config.count("dds_update_interval_ms") ? 
                std::stod(config["dds_update_interval_ms"]) : 10.0;
    unsigned num_files = config.count("num_files") ? std::stoul(config["num_files"]) : 1;
    Novatech409c* dds = nullptr;
    if(!dds_device.empty()) {
        dds = new Novatech409c(dds_device, 115200);
        dds->echo(false);
    }

    std::vector<double> freq_Hz;
    {
        stringstream ss(freqs_str);
        string item;
        while (getline(ss, item, ',')) {
            freq_Hz.push_back(std::stod(item));
            if(dds) {
                dds->setFrequencyHz(freq_Hz.size() - 1, freq_Hz.back());
                dds->setPhaseDeg(freq_Hz.size() - 1, 0.0);
            }
        }
    }
    std::vector<double> integrator_error(freq_Hz.size(), 0.0);
    std::vector<double> fLO = freq_Hz; // LO frequencies
    std::vector<double> fDDS = freq_Hz; // DDS frequencies
    std::vector<double> phaseDDS(freq_Hz.size(), 0.0); // phase accumulator for DDS updates, for higher frequency resolution

    if(dds) {
        dds->query();
    }

    // Channel names and offsets
    const unsigned num_initial_channels = 3; // NSAMP, VP-VN, GOOD
    const unsigned num_demod_channels = 4;
    const unsigned num_debug_channels = 11;
    unsigned num_channels = num_initial_channels + num_demod_channels + num_debug_channels; // total channels: NSAMP, VP-VN, GOOD, PHASE0..N, mixI, mixQ, avgmixI, avgmixQ, LO_I, LO_Q
    std::vector<unsigned> channels_to_read(num_channels);
    vector<string> channel_names = {"NSAMP", "VP-VN", "GOOD"}; 
    for(unsigned i = 0; i < num_demod_channels; i++) {
        channel_names.push_back("PHASE" + to_string(i));
    }

    // debug channels
    channel_names.push_back("mixI");
    channel_names.push_back("mixQ");
    channel_names.push_back("avgmixI");
    channel_names.push_back("avgmixQ");
    channel_names.push_back("LO_I");
    channel_names.push_back("LO_Q");
    channel_names.push_back("f0");
    channel_names.push_back("signal0");
    channel_names.push_back("signal1");
    channel_names.push_back("signal2");
    channel_names.push_back("signal3");

    vector<unsigned> channel_offsets(num_channels);
    vector<bool> plot_channels(num_channels, false);
    
	vector<unsigned>  width = {4, 2, 2}; // initial width for NSAMP and VP-VN
    const unsigned read_offset = 0x20; // Base offset for reading channels
    vector<double> scale = {1, 0, 1.0 / ((1 << num_demod_channels) - 1)}; // initial scale for NSAMP and VP-VN
    vector<unsigned> bipolar = {0, 1, 0}; // initial bipolar for NSAMP and VP-VN

    unsigned channel_offset = (read_offset)*4;
    for(size_t i=0; i<num_initial_channels+num_demod_channels; i++) {
        if(i >= num_initial_channels) {  // phase channels
            scale.push_back(2*M_PI/65536);
            bipolar.push_back(1);
            width.push_back(4);
        }
        channels_to_read[i] = i;
        channel_offsets[i] = channel_offset;
        channel_offset += width[i];
    }

    const double SAMPLE_RATE_HZ = 1e6 * 25 / 26.0;

    // debug channels in reverse order
    channel_offset = (32 + read_offset)*4;
    for(size_t j=0; j<num_debug_channels; j++) {
        size_t i = num_initial_channels + num_demod_channels + j;
        
        if(channel_names[i].find("f0") != string::npos) {
            width.push_back(4);
        } else {
            width.push_back(2);
        }
        channel_offset -= width.back();
        channel_offsets[i] = channel_offset;

        bipolar.push_back(1);
        scale.push_back(0);
        if(channel_names[i] == "f0") {
            scale[i] = SAMPLE_RATE_HZ / 65536.0 / 65536.0;
        }

        if(channel_names[i].find("signal") != string::npos) {
            scale[i] = 1;
        }

        channels_to_read[i] = i;      
    }
    
    // If a specific list of channels is provided in the config, parse it
    cout << "Channel list string: " << channel_list_str << endl;
    if (!channel_list_str.empty()) {
        channels_to_read.clear();
        stringstream ss(channel_list_str);
        string item;
        while (getline(ss, item, ',')) {
            unsigned ch = std::stoul(item);
            if (ch < num_channels) {
                channels_to_read.push_back(ch);
                cout << "Configured to read channel " << ch << " (" << channel_names[ch] << ")" << endl;
            } else {
                cerr << "Warning: Channel " << ch << " is out of range and will be ignored." << endl;
            }
        }
    }

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    map_base = mmap(NULL, XADC_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, XADC_BASE_ADDR);
    if (map_base == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

	// get scale from width if 0
    num_channels = channels_to_read.size();
    vector<int> phase_channels(4, -1);

    for (size_t i = 0; i < num_channels; ++i) {
        int ch = channels_to_read[i];
        if(std::find(udp_plot_channels.begin(), udp_plot_channels.end(), ch) != udp_plot_channels.end()) {
            plot_channels[i] = true;
        }

        if (0 == scale[ch]) {
            scale[ch] = 1.0;
            for(unsigned j=0; j<width[ch]; j++)
                scale[ch] /= 256.0;
        }
        printf("Channel[%2d] %s (index %d): scale = %g%s\n", 
            ch, channel_names[ch].c_str(), i, scale[ch], 
            plot_channels[i] ? " (UDP plot)" : "");

        for(int pc=0; pc<4; pc++) {
            if(channel_names[ch] == "PHASE" + to_string(pc)) {
                phase_channels[pc] = ch;
                printf("  PHASE%d channel found: %d\n", pc, ch);
            }
        }
    }
	
    int sleepval = ms * 1000;
    long us0 = 0;
	
    struct timespec start, stop;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("Start time: %ld.%09ld\n", start.tv_sec, start.tv_nsec);

// (((FREQ_HZ[((ch+1)*32-1):(ch*32)] * LUT_SIZE * 64) /
//                                       SAMPLE_RATE_HZ) << 18)

    vector<unsigned> phase_inc_values(freq_Hz.size(), 0);
    for(unsigned i=0; i<freq_Hz.size(); i++) {
        phase_inc_values[i] = set_frequency(map_base, i, freq_Hz[i], true);
    }
    
    //phase0 is reference
    //*(volatile unsigned int *)((char *)map_base + offset + 0*4) = 0xffffffff; // phase0_is_ref
    //printf("%s\n", (*(volatile unsigned int *)((char *)map_base + offset + 0*4) & 1) ? "phase0 is reference" : "phase0 is not reference");
    
    printf("Setting kp for phase lock to reference: 2^(%d)\n", -kp);
    printf("Setting ki for phase lock to reference: 2^(%d)\n", -ki);
    set_register(map_base, 1, (ki << 16) | kp); // set kp for phase lock to reference

    printf("Setting signal good threshold: %d\n", signal_good_threshold);
    set_register(map_base, 2, signal_good_threshold); // set ki for phase lock to reference

    // set LO phase inc registers
    for(unsigned i=0; i<phase_inc_values.size(); i++) {
        set_register(map_base, 4 + i, phase_inc_values[i]);
    }
	
    printf("  offset address:");
    for(auto ch : channels_to_read) {
        printf(width[ch] == 4 ? "     0x%04X" : "   0x%04X", channel_offsets[ch]);
    }
    printf("\n   Channel names:");
    for(auto ch : channels_to_read) {
        printf(width[ch] == 4 ? " %10s" : " %8s", channel_names[ch].c_str());
    }
    
    printf("\n");
    
    // reset IQ demodulator
    set_register(map_base, 0, 0x1); // assert reset
    usleep(1000);
    set_register(map_base, 0, 0x0); // deassert reset

    
    // Replace vector-based buffer with a one-dimensional dynamically allocated array
    unsigned max_samples = nmax;
    unsigned num_extra_channels = fLO.size(); // time_us, f1,...
    // Allocate a 1D array: data_buffer[sample * (num_channels + 1) + channel]
    unsigned *data_buffer = new unsigned[max_samples * (num_channels + num_extra_channels)];
    vector<float> udp_x(udp_plot_channels.size(), 0.0f);

    for(unsigned ifile_idx=0; ifile_idx<num_files; ifile_idx++) {
        unsigned n = 0;
        unsigned long us = 0;
        unsigned nsamp0 = 0;
        unsigned nsamp = 0;
        
        
        unsigned us_next_dds_update = 10000;

        while (n < nmax && us < tmax_ms * 1000) {
            clock_gettime(CLOCK_MONOTONIC, &stop);
            us = (stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_nsec - start.tv_nsec) / 1000;

            if(us < t0_ms * 1000) {
                usleep(10);
                continue;
            }   

            bool updated_dds = false;

            // Store time and raw values in the buffer
            unsigned udp_idx = 0;

            data_buffer[n * (num_channels + num_extra_channels) + 0] = us;
            for (size_t i = 0; i < num_channels; ++i) {
                int ch = channels_to_read[i];
                reg = get_register(map_base, (channel_offsets[ch] & 0xfffc));
                if(channel_offsets[ch] % 4 == 2)
                    reg = reg >> 16;

                unsigned int raw = reg;
                if(width[ch] == 4)
                    data_buffer[n * (num_channels + num_extra_channels) + (i + 1)] = raw;
                else
                    data_buffer[n * (num_channels + num_extra_channels) + (i + 1)] = raw & 0xffff;

                if(ch == reference_ch) {
                    double ratio = reg/(double)phase_inc_values[0];
                    
                    
                    // printf(" Set LO frequencies based on reference channel %d ratio %.6f\n", reference_ch, ratio);
                    // set frequencies for other channels
                    for(size_t j=1; j<freq_Hz.size(); j++) {
                        fLO[j] = freq_Hz[j] * ratio;
                        set_frequency(map_base, j, fLO[j]);
                    }
                } else if(dds && kp_dds != 0 && us > us_next_dds_update) {
                    updated_dds = true;
                    for(int pc=1; pc<4; pc++) {
                        if(ch == phase_channels[pc]) {
                            //printf(" DDS update channel %d at us=%ld\n", pc, us);
                            long x = raw;
                            if (x & 0x80000000)
                                x -= 0x100000000;
                            double error_rad = x * scale[ch];
                            integrator_error[pc] += error_rad;
                            double correction = kp_dds * error_rad + ki_dds * integrator_error[pc];
                            fDDS[pc] = freq_Hz[pc] - correction;
                            //dds->setFrequencyHz(pc, fDDS[pc]);
                            
                            phaseDDS[pc] -= 360.0 * correction * dds_update_interval_ms * 1e-3; // phase increment adjustment
                            phaseDDS[pc] = fmod(phaseDDS[pc], 360.0);
                            if(phaseDDS[pc] < 0)
                                phaseDDS[pc] += 360.0;

                            dds->setPhaseDeg(pc, phaseDDS[pc]);

                            //printf("[%9ld us] DDS update PC=%d, raw=%6ld, error=%.6f rad, f=%.3f Hz, phase=%7.3f deg\n", 
                            //       us, pc, x, error_rad, fLO[pc], phaseDDS[pc]);
                            // simple PI controller
                        }
                    }
                }
                if(dds && kp_dds != 0) {
                    for(size_t j=1; j<fLO.size(); j++)
                        data_buffer[n * (num_channels + num_extra_channels) + num_channels + j] = 
                            fDDS[j] * 1000; // store f in mHz
                }
                else {
                    for(size_t j=1; j<fLO.size(); j++)
                        data_buffer[n * (num_channels + num_extra_channels) + num_channels + j] = 
                            fLO[j] * 1000; // store f in mHz
                }
                if(plot_channels[i]) {
                    long x = data_buffer[n * (num_channels + num_extra_channels) + (i + 1)];
                    // If the channel is bipolar, convert to signed integer
                    if (bipolar[ch]) {
                        if (width[ch] == 2 && x & 0x8000)
                            x -= 0x10000;
                        if (width[ch] == 4 && x & 0x80000000)
                            x -= 0x100000000;
                    }
                    float value = x * scale[ch];
                    udp_x[udp_idx++] = value;
                    
                }
            }

            if(sender) {
                sender->send_data(udp_x);
            }
            if(updated_dds) {
                us_next_dds_update += dds_update_interval_ms * 1000;
            }

            nsamp = data_buffer[n * (num_channels + num_extra_channels) + 1];

            if(n == 0) {
                us0 = us;
                nsamp0 = nsamp;
            }
            
            if ((!quiet && showraw) || (n == nmax - 1)  ||     (n == 0) ) {
                printf(" RAW %8ld us:", us);
                for (size_t i = 0; i < num_channels; ++i) {
                    printf(width[channels_to_read[i]] == 4 ? " 0x%08X" : "   0x%04X", 
                        data_buffer[n * (num_channels + 1) + (i + 1)]);
                }
                printf("\n");
            }
            if (!quiet && showscaled) {
                printf("Scaled %6ld us:", us);
                for (size_t i = 0; i < num_channels; ++i) {
                    long x = data_buffer[n * (num_channels + 1) + (i + 1)];
                    int ch = channels_to_read[i];
                    if (bipolar[ch]) {
                        if (width[ch] == 2 && x & 0x8000)
                            x = (~x + 1) & 0xffff;
                            
                        if (width[ch] == 4 && x & 0x80000000)
                            x =  ~x + 1;
                    }
                    
                    float val = x * scale[ch];
                    if (ch == 0)
                        printf(" %8.4f", val);
                    else if (channels_to_read[i] == 1)
                        printf(" %8.0f", val - nsamp0);
                    else
                        printf(" %8.0f", val);
                }
                printf("\n");
            }
            //unsigned diff_us = us - us_prev;
            //sleepval -= (diff_us - 1000 * ms) / 10;
            if (sleepval > 0)
                usleep(sleepval);
            n++;
        }
        
        float dt = us-us0;
        if(dt>0)
            printf("sample rate: %7.3f kHz\n", 1000.0*(nsamp-nsamp0)/dt);

        if (csv_filename.length() > 0) {
            ofstream csvfile;
            csvfile.open(to_string(ifile_idx) + "_" + csv_filename);
            if (!csvfile.is_open()) {
                fprintf(stderr, "Failed to open CSV file: %s\n", csv_filename.c_str());
                return 1;
            } else {
                printf("Writing CSV output to %s\n", csv_filename.c_str());
            }

            // Set higher precision for floating-point output
            csvfile << std::fixed << std::setprecision(4);

            // Write CSV header
            csvfile << "time_us";
            for (auto ch : channels_to_read) {
                csvfile << "," << channel_names[ch];
            }
            for (size_t j = 1; j < fLO.size(); ++j) {
                csvfile << ",f" << j;
            }
            //
            csvfile << std::endl;

            // Write buffered data
            for (unsigned i = 0; i < n; ++i) {
                unsigned long time_us = data_buffer[i * (num_channels + num_extra_channels) + 0];
                if(time_us < ms0 * 1000)
                    continue;

                csvfile << time_us;
                for (size_t j = 0; j < num_channels; ++j) {
                    long x = data_buffer[i * (num_channels + num_extra_channels) + (j + 1)];
                    unsigned ch = channels_to_read[j];
                    // If the channel is bipolar, convert to signed integer
                    if (bipolar[ch]) {
                        if (width[ch] == 2 && x & 0x8000)
                            x -= 0x10000;
                        if (width[ch] == 4 && x & 0x80000000)
                            x -= 0x100000000;
                    }
                    double value = x * (showscaled ? scale[ch] : 1);
                    csvfile << "," << value;
                }
                // Write extra channels
                for (unsigned j = 1; j < num_extra_channels; ++j) {
                    csvfile << "," << data_buffer[i * (num_channels + num_extra_channels) + num_channels + j] * 0.001; // convert mHz back to Hz
                }
                csvfile << std::endl;
            }
            csvfile.close();
        }
    } // end of file loop

    // Free the buffer
    delete[] data_buffer;

    munmap(map_base, XADC_SPAN);
    close(fd);

    if(dds) {
        dds->close();
        delete dds;
    }

    if(sender) {
        delete sender;
    }

    
    return 0;
}
