#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define XADC_BASE_ADDR  0x43C00000  // Replace with your actual base address
#define XADC_SPAN       0x1000
#define XADC_VPVN_REG   0x04  // see UG480 for register offsets

#include <sys/time.h>

int main() {
    int fd;
    void *map_base;
    volatile unsigned int *xadc_reg;

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
	unsigned nmax = 20;
	int showscaled = 0;
	unsigned ms = 5;
	
	const float Toff = -273.15;
	const float scale[8] = {503.975, 3, 3, 1, 1, 1, 3, 1}; // see UG480, p. 33
	const unsigned bipolar[8] = {0, 0, 0, 0, 0, 1, 0, 0};
	const unsigned offset = 0x200;  // see XADC Wizard v3.3 Product Guide (PG091), p. 17
	const char channels[] = "   TEMP   VCCINT   VCCAUX    VP/VN    VREFP    VREFN  VCCBRAM";
	
	struct timeval stop, start;
    gettimeofday(&start, NULL);
    while (n < nmax) {
		if(n % nmax == 0) {
			printf(" offset address: ");
			for(unsigned i=0; i<0x1c; i+=4) {
				printf(" 0x%06X", offset+i);
			}
			printf("\n                  %s\n", channels);
		}
		
		gettimeofday(&stop, NULL);
		int us = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
		
		printf("XADC %6d us: ", us);
		for(unsigned i=0; i<0x1c; i+=4) {
			xadc_reg = (volatile unsigned int *)((char *)map_base + offset + i);
			unsigned int raw = *xadc_reg;
			unsigned low = raw & 0xffff;
			printf("   0x%04X", low);
		}
		printf("\n");
		if(showscaled) {
			printf("  Scaled values: ");
			for(unsigned i=0; i<0x1c; i+=4) {
				xadc_reg = (volatile unsigned int *)((char *)map_base + offset + i);
				unsigned int raw = *xadc_reg;
				int low = (raw & 0xffff) >> 4;
				if(bipolar[i/4] && low >= 0x800)
					low = low - 0xfff - 1;
					
				printf(i == 0 ? " %7.2fC" : " %7.4fV", low*scale[i/4]/4096.0 + (i == 0 ? Toff : 0));
			}
			printf("\n");
		}
        usleep(1000*ms);
        n++;
    }

    munmap(map_base, XADC_SPAN);
    close(fd);
    return 0;
}
