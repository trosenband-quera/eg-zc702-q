
SELF := $(shell basename $(PWD))

# Zynq on ZC702 is running linux so use arm-linux-gnueabi
# For bare-metal target use arm-none-gnueabi
CROSS_COMPILE := arm-linux-gnueabihf

# Setup variables used by make's implicit rules.
# https://www.gnu.org/software/make/manual/html_node/Catalogue-of-Rules.html
AR := $(CROSS_COMPILE)-ar
AS := $(CROSS_COMPILE)-as
CC := $(CROSS_COMPILE)-gcc
CPP := $(CROSS_COMPILE)-cpp
LD := $(CROSS_COMPILE)-ld

# More warnings.
CFLAGS := -Wall -Wextra

# The minimal linux does not have shared libraries to dynamically link to so
# executable must be statically linked.
# Otherwise, when running ./foo you get an error like:
#	-sh: ./foo: No such file or directory
LDFLAGS := -static

# Link to libraries.
#LDLIBS := -lm

# First target is the default so name it as such.
default: $(SELF)

# DEPS := xadc.o
$(SELF): $(DEPS)

# For this target to work you need to have ssh setup appropriately using
# ~/.ssh/config on the host.
# I.e. containing something like this:
#	Host zc702
#		Hostname 192.168.0.10
#		User root
#		StrictHostKeyChecking no
#		UserKnownHostsFile /dev/null
#		LogLevel QUIET
.PHONY : transfer
transfer: default
	scp $(SELF) zc702:~/

.PHONY: xdevcfg
xdevcfg:
	cp ../../vivado/projmode-led8/projmode-led8.runs/impl_1/led8_wrapper.bit pl.bit
	scp pl.bit zc702:~/
	ssh zc702 -C "cat pl.bit > /dev/xdevcfg"
	
.PHONY : run
run: transfer
	ssh zc702 -C "./$(SELF)"

.PHONY : clean
clean:
	-rm -f *.o
	-rm -f $(SELF)

# List all Make targets.
.PHONY : list
list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | \
		awk -v RS= -F: '/^# File/,/^# Finished Make data base/ \
			{if ($$1 !~ "^[#.]") {print $$1}}' | \
		sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
