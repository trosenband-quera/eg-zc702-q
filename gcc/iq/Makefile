
SELF := $(shell basename $(PWD))

# Zynq on ZC702 is running linux so use arm-linux-gnueabi
# For bare-metal target use arm-none-gnueabi
CROSS_COMPILE := arm-linux-gnueabihf

# Setup variables used by make's implicit rules.
# https://www.gnu.org/software/make/manual/html_node/Catalogue-of-Rules.html
AR := $(CROSS_COMPILE)-ar
AS := $(CROSS_COMPILE)-as
CC := $(CROSS_COMPILE)-gcc
CXX := $(CROSS_COMPILE)-g++
LD := $(CROSS_COMPILE)-ld

# More warnings.
CFLAGS := -Wall -Wextra
CXXFLAGS := -Wall -Wextra -Wno-psabi  #-g -O1 -gdwarf-4

# The minimal linux does not have shared libraries to dynamically link to so
# executable must be statically linked.
# Otherwise, when running ./foo you get an error like:
#	-sh: ./foo: No such file or directory
LDFLAGS := -static -lm

# Link to libraries.
#LDLIBS := -lm

# First target is the default so name it as such.
default: $(SELF)

serial_comm: serial_comm.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) serial_comm.cpp -o serial_comm

serial: serial_comm
	scp serial_comm zc702:~/
	ssh zc702 -C "chmod u+x ./serial_comm && ./serial_comm -d /dev/ttyUSB0 -m q -B"

$(SELF): $(DEPS)

# For this target to work you need to have ssh setup appropriately using
# ~/.ssh/config on the host.
# I.e. containing something like this:
#	Host zc702
#		Hostname 10.0.0.10
#		User root
#		StrictHostKeyChecking no
#		UserKnownHostsFile /dev/null
#		LogLevel QUIET
.PHONY : transfer
transfer: default
	scp $(SELF) zc702:~/

# configure FPGA for Xilinx 2025.1 tools
.PHONY: fpgacfg
fpgacfg:
	cp ../../vivado/iq_project/top.bit pl.bit
	bootgen -w -image bitstream.bif -arch zynq -process_bitstream bin
	ssh zc702 -C "mkdir -p /lib/firmware"
	scp pl.bit.bin zc702:/lib/firmware
	ssh zc702 -C "echo 0 > /sys/class/fpga_manager/fpga0/flags"
	ssh zc702 -C "echo pl.bit.bin > /sys/class/fpga_manager/fpga0/firmware"

# configure FPGA for Xilinx 2018.2 tools
.PHONY: xdevcfg
xdevcfg:
	cp ../../vivado/iq_project/iq-demod-zynq/iq-demod-zynq.runs/impl_1/top_wrapper.bit pl.bit
	scp pl.bit.bin zc702:~/
	ssh zc702 -C "cat pl.bit > /dev/xdevcfg"
	
.PHONY : run
run: transfer
	ssh zc702 -C "./$(SELF)"

.PHONY : getdata
getdata: transfer
	ssh zc702 -C "./$(SELF) -c 0,1,5,6,7,8,9,10,2,3,11 -q -f 1.csv -n 500 -m 1 -s -k 4"
	scp zc702:~/1.csv .
	python3 plot_csv.py 1.csv

.PHONY : gethist
gethist:
	ssh zc702 -C "./$(SELF) -c 0,1 -s -q -f 1.csv -n 2000 -m 1"
	scp zc702:~/1.csv .
	python3 plot_csv.py 1.csv --hist-raw

.PHONY : clean
clean:
	-rm -f *.o *.bit *.csv
	-rm -f $(SELF) serial_comm

# List all Make targets.
.PHONY : list
list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | \
		awk -v RS= -F: '/^# File/,/^# Finished Make data base/ \
			{if ($$1 !~ "^[#.]") {print $$1}}' | \
		sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
