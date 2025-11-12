#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fstream>
#include <time.h>

#define XADC_BASE_ADDR  0x43C00000  // Replace with your actual base address
#define XADC_SPAN       0x1000

#include <sys/time.h>
#include <getopt.h>
#include <cstring>
#include <vector>
#include <tuple>
#include <string>
#include <sstream>
#include <cstring>
#include <cmath>
int main(int argc, char *argv[]) {
    int fd;
    void *map_base;
    unsigned int reg;

    // Default values
    unsigned nmax = 10;
    int showraw = 1;      // Show raw values by default
    int showscaled = 0;   // Show scaled values if requested
    int quiet = 0;        // Do not display data if set
    unsigned ms = 5;
    std::vector<unsigned> channels_to_read = {0,9}; // default: all channels
    double scale[] = {0, 1, 2*M_PI/65536, 2*M_PI/65536, 2*M_PI/65536, 
						0, 0, 
						0, 0, 
						0, 0};
    const unsigned bipolar[] = {1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
 
    // Channel names and offsets
    const char* channel_names[] = {"VP-VN", "NSAMP", "PHASE0", "PHASE1", "PHASE2", 
		                           "mixI", "mixQ", "avgmixI", "avgmixQ", "LO_I", "LO_Q"};
    const unsigned channel_offsets[] = {8*4, 7*4, 9*4, 9*4 + 2, 10*4, 11*4, 12*4, 13*4, 14*4, 15*4, 15*4+2};
	const unsigned width[] = {2, 4, 2, 2, 2, 4, 4, 4, 4, 2, 2};

    // Command line options
    int opt;
    std::string csv_filename;
    std::ofstream csvfile;
    int write_csv = 0;

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
                    if (ch < 11) channels_to_read.push_back(ch);
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

	
	unsigned num_channels = channels_to_read.size();
	
	// get scale from width if 0
	for (size_t i = 0; i < num_channels; ++i) {
		int ch = channels_to_read[i];
		if (0 == scale[ch]) {
			scale[ch] = 1.0;
			for(unsigned j=0; j<width[ch]; j++)
				scale[ch] /= 256.0;
		}
	}
	
    unsigned n = 0;

    const unsigned offset = 0x00;
    int sleepval = ms * 1000;
    long us0 = 0;
	long us;
	
    struct timespec start, stop;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("Start time: %ld.%09ld\n", start.tv_sec, start.tv_nsec);

// (((FREQ_HZ[((ch+1)*32-1):(ch*32)] * LUT_SIZE * 64) /
//                                       SAMPLE_RATE_HZ) << 18);
	const double SAMPLE_RATE_HZ = 1e6 * 25 / 26.0;
	const double freq_Hz = 100.0e3;
	double phase_inc_d = 65536.0 * freq_Hz / SAMPLE_RATE_HZ;
	unsigned phase_inc = 0.5 + phase_inc_d;
	double freq_Hz_true = phase_inc * SAMPLE_RATE_HZ / 65536.0;
	
	printf("target = %.3f Hz, true = %.3f Hz, phase delta = %.3f\n", freq_Hz, freq_Hz_true, phase_inc_d);
	
	// set LO freq, reg2
	*(volatile unsigned int *)((char *)map_base + offset + 2*4) = phase_inc;
	
    unsigned nsamp0, nsamp;
    
    // Replace vector-based buffer with a one-dimensional dynamically allocated array
    unsigned max_samples = nmax;
    
    // Allocate a 1D array: data_buffer[sample * (num_channels + 1) + channel]
    unsigned *data_buffer = new unsigned[max_samples * (num_channels + 1)];
    while (n < nmax) {
        if(n % nmax == 0) {
            printf("  offset address:  ");
            for(auto ch : channels_to_read) {
                printf(width[ch] == 4 ? "     0x%04X" : "   0x%04X", offset + channel_offsets[ch]);
            }
            printf("\n   Channel names:");
            for(auto ch : channels_to_read) {
                printf(width[ch] == 4 ? " %10s" : " %8s", channel_names[ch]);
            }
            printf("\n");
        }

        clock_gettime(CLOCK_MONOTONIC, &stop);
        us = (stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_nsec - start.tv_nsec) / 1000;

        // Store time and raw values in the buffer
        data_buffer[n * (num_channels + 1) + 0] = us;
        for (size_t i = 0; i < num_channels; ++i) {
            unsigned ch = channels_to_read[i];
            if(channel_offsets[ch] % 4 == 0)
                reg = *(volatile unsigned int *)((char *)map_base + offset + (channel_offsets[ch] & 0xfffc));
            else
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
		
        if (!quiet && showraw) {
            printf("   RAW %6ld us:", us);
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

    if (write_csv) {
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
                csvfile << "," << x * (showscaled ? scale[ch] : 1);
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
