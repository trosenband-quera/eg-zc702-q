
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
CXXFLAGS := -Wall -Wextra -Wno-psabi  -O2  -Iinclude/ \
            -Wl,--rpath=/newglibc \
			-Wl,--dynamic-linker=/newglibc/ld-linux-armhf.so.3#-g -O1 -gdwarf-4

# The minimal linux does not have shared libraries to dynamically link to so
# executable must be statically linked.
# Otherwise, when running ./foo you get an error like:
#	-sh: ./foo: No such file or directory
LDFLAGS := -lm -lgcc -L./lib

# Link to libraries.
#LDLIBS := -lm

# First target is the default so name it as such.
default: iq

iq: iq.cpp Novatech409c.cpp message.pb.cc message.pb.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) iq.cpp Novatech409c.cpp message.pb.cc -o iq -lprotobuf -latomic

ppstep: ppstep.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) ppstep.cpp -o ppstep -lzmq -lpthread -lmsgpack-c -lrt
	scp ppstep zc702:~/
	ssh zc702 -C "chmod u+x ./ppstep"
	
.PHONY : pp
pp: ppstep
	ssh zc702 -C "./ppstep -h || ./ppstep -u tcp://10.0.0.111:8710 -n 100"

proto: message.proto
	$(PROTOC) --cpp_out=. --python_out=. message.proto

serial_comm: serial_comm.cpp Novatech409c.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) serial_comm.cpp Novatech409c.cpp -o serial_comm

udpsend: udpsend.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) udpsend.cpp -o udpsend

udpsend2: udpsend2.cpp message.pb.cc message.pb.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS)  udpsend2.cpp message.pb.cc -o udpsend2  -lprotobuf -latomic

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

.PHONY : fpgacfg
fpgacfg:
	$(MAKE) -C ../hw/iq_project fpgacfg
	scp ../thirdparty/protobuf/src/.libs/libprotobuf.so zc702:/usr/local/lib/

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
transfer: default ppstep
	scp iq iq_config*.txt config.pb.txt ppstep zc702:~/

.PHONY : transferlibs
transferlibs:
	scp lib/libmsgpack-c.so.2 lib/libprotobuf.so lib/libzmq.so.5 zc702:/usr/lib/

.PHONY : run
run: transfer
	ssh zc702 -C "./iq"

.PHONY : getdata
getdata: transfer
	ssh zc702 -C "./iq iq_config.txt"
	scp zc702:~/0_1.csv data/
	python3 plot_csv.py data/0_1.csv

.PHONY : nolock
nolock: transfer
	ssh zc702 -C "./iq iq_config_nolock.txt"
	scp zc702:~/0_2.csv data/
	python3 plot_csv.py data/0_2.csv

.PHONY : live
live: transfer
	ssh zc702 -C "./iq iq_config_live.txt"

.PHONY : gethist
gethist:
	ssh zc702 -C "./iq -c 0,1 -s -q -f 1.csv -n 2000 -m 1"
	scp zc702:~/1.csv .
	python3 plot_csv.py 1.csv --hist-raw

.PHONY : clean
clean:
	-rm -f *.o *.bit *.csv
	-rm -f iq serial_comm udpsend udpsend2 ppstep

# List all Make targets.
.PHONY : list
list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | \
		awk -v RS= -F: '/^# File/,/^# Finished Make data base/ \
			{if ($$1 !~ "^[#.]") {print $$1}}' | \
		sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
