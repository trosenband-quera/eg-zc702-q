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

using namespace std;

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
        size_t eq = line.find('=');
        if (eq != string::npos) {
            string key = line.substr(0, eq);
            string value = line.substr(eq + 1);
            config[key] = value;
        }
    }
    return config;
}

int main(int argc, char *argv[]) {
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
    unsigned nmax = config.count("nmax") ? std::stoul(config["nmax"]) : 10;
    int showraw = config.count("showraw") ? std::stoi(config["showraw"]) : 1;
    int showscaled = config.count("showscaled") ? std::stoi(config["showscaled"]) : 0;
    int quiet = config.count("quiet") ? std::stoi(config["quiet"]) : 0;
    unsigned ms = config.count("ms") ? std::stoul(config["ms"]) : 5;
    std::string csv_filename = config.count("csv_filename") ? config["csv_filename"] : "";
    unsigned kp = config.count("kp") ? std::stoul(config["kp"]) : 0;
    unsigned ki = config.count("ki") ? std::stoul(config["ki"]) : 0;
    unsigned signal_good_threshold = config.count("signal_good_threshold") ? std::stoul(config["signal_good_threshold"]) : 0;
    std::string channel_list_str = config.count("channel_list") ? config["channel_list"] : "";

    // Channel names and offsets
    const unsigned num_initial_channels = 3; // NSAMP, VP-VN, GOOD
    const unsigned num_demod_channels = 3;
    const unsigned num_debug_channels = 8;
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
    channel_names.push_back("signal");

    vector<unsigned> channel_offsets(num_channels);
	vector<unsigned>  width = {4, 2, 2}; // initial width for NSAMP and VP-VN
    const unsigned read_offset = 0x20; // Base offset for reading channels
    vector<double> scale = {1, 0, 1}; // initial scale for NSAMP and VP-VN
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
    for (size_t i = 0; i < num_channels; ++i) {
        int ch = channels_to_read[i];
        if (0 == scale[ch]) {
            scale[ch] = 1.0;
            for(unsigned j=0; j<width[ch]; j++)
                scale[ch] /= 256.0;
        }
        printf("Channel[%2d] %s (index %d): scale = %g\n", ch, channel_names[ch].c_str(), ch, scale[ch]);
    }
	
    unsigned n = 0;

    int sleepval = ms * 1000;
    long us0 = 0;
	long us;
	
    struct timespec start, stop;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("Start time: %ld.%09ld\n", start.tv_sec, start.tv_nsec);

// (((FREQ_HZ[((ch+1)*32-1):(ch*32)] * LUT_SIZE * 64) /
//                                       SAMPLE_RATE_HZ) << 18)
    
	const vector<double> freq_Hz = {100.0e3, 112.5e3};
    vector<unsigned> phase_inc_values(1+freq_Hz.size()/2, 0);
    for(unsigned i=0; i<freq_Hz.size(); i++) {
        double phase_inc_d = 65536.0 * 65536.0 * freq_Hz[i] / SAMPLE_RATE_HZ;
        unsigned phase_inc = 0.5 + phase_inc_d;
        double freq_Hz_true = phase_inc * SAMPLE_RATE_HZ / 65536.0 / 65536.0;
        
        printf("LO %d, target = %.3f Hz, true = %.6f Hz, phase delta = %.3f\n", i, freq_Hz[i], freq_Hz_true, phase_inc_d);
        phase_inc_values[i] = phase_inc;
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
	
    // reset IQ demodulator
    set_register(map_base, 0, 0x1); // assert reset
    usleep(1000);
    set_register(map_base, 0, 0x0); // deassert reset

    unsigned nsamp0, nsamp;
    
    // Replace vector-based buffer with a one-dimensional dynamically allocated array
    unsigned max_samples = nmax;
    
    // Allocate a 1D array: data_buffer[sample * (num_channels + 1) + channel]
    unsigned *data_buffer = new unsigned[max_samples * (num_channels + 1)];
    while (n < nmax) {
        if(n % nmax == 0) {
            printf("  offset address:");
            for(auto ch : channels_to_read) {
                printf(width[ch] == 4 ? "     0x%04X" : "   0x%04X", channel_offsets[ch]);
            }
            printf("\n   Channel names:");
            for(auto ch : channels_to_read) {
                printf(width[ch] == 4 ? " %10s" : " %8s", channel_names[ch].c_str());
            }
            
            printf("\n");
        }

        clock_gettime(CLOCK_MONOTONIC, &stop);
        us = (stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_nsec - start.tv_nsec) / 1000;

        // Store time and raw values in the buffer
        data_buffer[n * (num_channels + 1) + 0] = us;
        for (size_t i = 0; i < num_channels; ++i) {
            unsigned ch = channels_to_read[i];
            reg = get_register(map_base, (channel_offsets[ch] & 0xfffc));
            if(channel_offsets[ch] % 4 == 2)
                reg = reg >> 16;

            unsigned int raw = reg;
            if(width[ch] == 4)
                data_buffer[n * (num_channels + 1) + (i + 1)] = raw;
            else
                data_buffer[n * (num_channels + 1) + (i + 1)] = raw & 0xffff;
        }

        nsamp = data_buffer[n * (num_channels + 1) + 1];

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
        csvfile.open(csv_filename);
        if (!csvfile.is_open()) {
            fprintf(stderr, "Failed to open CSV file: %s\n", csv_filename.c_str());
            return 1;
        } else {
            printf("Writing CSV output to %s\n", csv_filename.c_str());
        }

        // Write CSV header
        csvfile << "time_us";
        for (auto ch : channels_to_read) {
            csvfile << "," << channel_names[ch];
        }
        csvfile << std::endl;

        // Write buffered data
        for (unsigned i = 0; i < n; ++i) {
            csvfile << data_buffer[i * (num_channels + 1) + 0];
            for (size_t j = 0; j < num_channels; ++j) {
                long x = data_buffer[i * (num_channels + 1) + (j + 1)];
                unsigned ch = channels_to_read[j];
                // If the channel is bipolar, convert to signed integer
                if (bipolar[ch]) {
                    if (width[ch] == 2 && x & 0x8000)
                        x -= 0x10000;
                    if (width[ch] == 4 && x & 0x80000000)
                        x -= 0x100000000;
                }
                csvfile << "," << x * (showscaled ? scale[ch] : 1) + (showscaled && channel_names[ch] == "f0" ? -1e5 : 0);
            }
            csvfile << std::endl;
        }
        csvfile.close();
    }

    // Free the buffer
    delete[] data_buffer;

    munmap(map_base, XADC_SPAN);
    close(fd);
    return 0;
}
