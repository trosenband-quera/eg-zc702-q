
SELF := $(shell basename $(PWD))

default:
	vivado -source recreate.tcl

.PHONY : bit
bit:
	vivado -mode batch -source genbitstream.tcl
	
.PHONY : clean
clean:
	-rm -f *.xpr *.jou
	-rm -f vivado_*
	-rm -rf *.sim
	-rm -rf *.cache
	-rm -rf *.hw
	-rm -rf *.ip_user_files
	-rm -rf *.runs
	-rm -rf srcs/sources_1/bd
	-rm -rf iq-demod-zynq

# List all Make targets.
.PHONY : list
list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | \
		awk -v RS= -F: '/^# File/,/^# Finished Make data base/ \
			{if ($$1 !~ "^[#.]") {print $$1}}' | \
		sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
