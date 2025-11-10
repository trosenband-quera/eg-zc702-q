#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fstream>

#define XADC_BASE_ADDR  0x43C00000  // Replace with your actual base address
#define XADC_SPAN       0x1000

#include <sys/time.h>
#include <getopt.h>
#include <cstring>
#include <vector>
#include <tuple>
#include <string>
#include <sstream>

int main(int argc, char *argv[]) {
    int fd;
    void *map_base;
    volatile unsigned int *xadc_reg;

    // Default values
    unsigned nmax = 10;
    int showraw = 1;      // Show raw values by default
    int showscaled = 0;   // Show scaled values if requested
    int quiet = 0;        // Do not display data if set
    unsigned ms = 5;
    std::vector<unsigned> channels_to_read = {0,1,2,3}; // default: all channels

    // Channel names and offsets
    const char* channel_names[] = {"VP/VN", "NSAMP", "PHASE01", "PHASE2"};
    const unsigned channel_offsets[] = {8*4, 7*4, 9*4, 10*4};

    // Command line options
    int opt;
    std::string csv_filename;
    std::ofstream csvfile;
    int write_csv = 0;

    // Buffer for data: each entry is (time_us, vector of raw values)
    std::vector<std::pair<int, std::vector<unsigned>>> data_buffer;

    while ((opt = getopt(argc, argv, "n:m:rc:shf:q")) != -1) {
        switch (opt) {
            case 'n':
                nmax = std::strtoul(optarg, nullptr, 0);
                break;
            case 'r':
                showraw = 1;
                break;
            case 's':
                showscaled = 1;
                showraw = 0;
                break;
            case 'm':
                ms = std::strtoul(optarg, nullptr, 0);
                break;
            case 'c': {
                channels_to_read.clear();
                std::string arg(optarg);
                std::stringstream ss(arg);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    unsigned ch = std::strtoul(item.c_str(), nullptr, 0);
                    if (ch < 7) channels_to_read.push_back(ch);
                }
                break;
            }
            case 'f':
                csv_filename = optarg;
                write_csv = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'h':
            default:
                printf("Usage: %s [-n samples] [-r] [-s] [-m ms_delay] [-c channels] [-f csvfile] [-q] [-h]\n", argv[0]);
                printf("  -n N         Number of samples (default: 20)\n");
                printf("  -r           Show raw values (default)\n");
                printf("  -s           Show scaled values (disables raw)\n");
                printf("  -m MS        Delay in ms between samples (default: 5)\n");
                printf("  -c CHS       Comma-separated channel indices (0:TEMP, 1:VCCINT, ... 6:VCCBRAM)\n");
                printf("  -f FILE      Output CSV file\n");
                printf("  -q           Do not display data (quiet mode)\n");
                printf("  -h           Show this help message\n");
                return 0;
        }
    }

    // Prepare CSV header if needed
    if (write_csv) {
        csvfile.open(csv_filename);
        if (!csvfile.is_open()) {
            fprintf(stderr, "Failed to open CSV file: %s\n", csv_filename.c_str());
            return 1;
        }
        // Write CSV header: always raw, optionally scaled
        csvfile << "time_us";
        for (auto ch : channels_to_read) {
            csvfile << "," << channel_names[ch];
        }
        csvfile << std::endl;
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

    unsigned n = 0;

    const float scale[8] = {1/4096.0/4.0, 1, 1, 1, 1, 1, 3, 1}; // see UG480, p. 33
    const unsigned bipolar[8] = {1,     0,        0,       0,       0,        0,    0, 0};
    const unsigned offset = 0x00;
    int sleepval = ms * 1000;
    long us0 = 0;
	long us;
	
    struct timeval stop, start;
    gettimeofday(&start, NULL);
    
    unsigned nsamp0;
    std::vector<unsigned> raw_values(channels_to_read.size());
    while (n < nmax) {
        if(n % nmax == 0) {
            printf("  offset address:  ");
            for(auto ch : channels_to_read) {
                printf(" 0x%06X", offset + channel_offsets[ch]);
            }
            printf("\n   Channel names:");
            for(auto ch : channels_to_read) {
                printf(" %8s", channel_names[ch]);
            }
            printf("\n");
        }

        gettimeofday(&stop, NULL);
        us = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;

        for (auto ch : channels_to_read) {
            xadc_reg = (volatile unsigned int *)((char *)map_base + offset + channel_offsets[ch]);
            unsigned int raw = *xadc_reg;
            raw_values[ch] = raw;
        }
        data_buffer.emplace_back(us, raw_values);
        
        if(n == 0) {
			us0 = us;
			nsamp0 = raw_values[1];
		}
		
        if (!quiet && showraw) {
            printf("   RAW %6ld us:", us);
            for (size_t i = 0; i < raw_values.size(); ++i) {
                printf("   0x%04X", raw_values[i]);
            }
            printf("\n");
        }
        if (!quiet && showscaled) {
            printf("Scaled %6ld us:", us);
            for (size_t i = 0; i < raw_values.size(); ++i) {
				float val = raw_values[i];
				if(i==0) {
					long x = raw_values[i];
					if (bipolar[channels_to_read[i]] && x >= 0x8000)
						x = x - 0xffff - 1;
						
					val = x * scale[channels_to_read[i]];
				}
                if (channels_to_read[i] == 0)
                    printf(" %8.4f", val);
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
		printf("sample rate: %7.3f kHz\n", 1000.0*(raw_values[1]-nsamp0)/dt);

    if (write_csv) {
        // Write CSV header already done above

        // Write buffered data: always raw
        for (const auto& entry : data_buffer) {
            csvfile << entry.first;
            for (size_t i = 0; i < entry.second.size(); ++i) {
                unsigned val = entry.second[i];
                unsigned ch = channels_to_read[i];
                // If the channel is bipolar, convert to signed 12-bit integer
                int sval;
                if (bipolar[ch]) {
                    sval = ((int)(val >> 4));
                    if (sval & 0x800)
                        sval -= 0x1000;
                } else {
                    sval = val;
                }
                csvfile << "," << sval;
            }
            csvfile << std::endl;
        }
        csvfile.close();
    }

    munmap(map_base, XADC_SPAN);
    close(fd);
    return 0;
}
