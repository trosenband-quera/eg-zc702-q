
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

PROTOC := protoc 

# More warnings.
CFLAGS := -Wall -Wextra
CXXFLAGS := -Wall -Wextra -Wno-psabi  -O2  -Iinclude/ #-g -O1 -gdwarf-4

# The minimal linux does not have shared libraries to dynamically link to so
# executable must be statically linked.
# Otherwise, when running ./foo you get an error like:
#	-sh: ./foo: No such file or directory
LDFLAGS := -static -lm -lgcc

# Link to libraries.
#LDLIBS := -lm

# First target is the default so name it as such.
default: iq

iq: iq.cpp Novatech409c.cpp message.pb.cc message.pb.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) iq.cpp Novatech409c.cpp message.pb.cc -o $(SELF)  lib/libprotobuf-lite.a -latomic

proto: message.proto
	$(PROTOC) --cpp_out=. --python_out=. message.proto

serial_comm: serial_comm.cpp Novatech409c.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) serial_comm.cpp Novatech409c.cpp -o serial_comm

udpsend: udpsend.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) udpsend.cpp -o udpsend

udpsend2: udpsend2.cpp message.pb.cc message.pb.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS)  udpsend2.cpp message.pb.cc -o udpsend2  lib/libprotobuf-lite.a -latomic

.PHONY : udp
udp: udpsend
	scp udpsend zc702:~/
	ssh zc702 -C "chmod u+x ./udpsend && ./udpsend 10.0.0.1"

.PHONY : udp2
udp2: udpsend2
	scp udpsend2 zc702:~/
	ssh zc702 -C "chmod u+x ./udpsend2 && ./udpsend2 10.0.0.1"

.PHONY : serial
serial: serial_comm
	scp serial_comm zc702:~/
	ssh zc702 -C "chmod u+x ./serial_comm && ./serial_comm -d /dev/ttyUSB0 -m q -B"

$(SELF): $(DEPS) Novatech409c.cpp

.PHONY : bit
bit:
	$(MAKE) -C ../../vivado/iq_project bit

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
	scp $(SELF) iq_config*.txt zc702:~/

# configure FPGA for Xilinx 2025.1 tools
.PHONY: fpgacfg
fpgacfg:
	cp ../../vivado/iq_project/top.bit .
	bootgen -w -image bitstream.bif -arch zynq -process_bitstream bin
	ssh zc702 -C "mkdir -p /lib/firmware"
	scp top.bit.bin zc702:/lib/firmware
	ssh zc702 -C "echo 0 > /sys/class/fpga_manager/fpga0/flags"
	ssh zc702 -C "echo top.bit.bin > /sys/class/fpga_manager/fpga0/firmware"

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
	ssh zc702 -C "./$(SELF) iq_config.txt"
	scp zc702:~/0_1.csv .
	python3 plot_csv.py 0_1.csv

.PHONY : live
live: transfer
	ssh zc702 -C "./$(SELF) iq_config_live.txt"

.PHONY : gethist
gethist:
	ssh zc702 -C "./$(SELF) -c 0,1 -s -q -f 1.csv -n 2000 -m 1"
	scp zc702:~/1.csv .
	python3 plot_csv.py 1.csv --hist-raw

.PHONY : clean
clean:
	-rm -f *.o *.bit *.csv
	-rm -f $(SELF) serial_comm udpsend udpsend2

# List all Make targets.
.PHONY : list
list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | \
		awk -v RS= -F: '/^# File/,/^# Finished Make data base/ \
			{if ($$1 !~ "^[#.]") {print $$1}}' | \
		sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
