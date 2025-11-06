
SELF := $(shell basename $(PWD))

default:
	vivado -source recreate.tcl

.PHONY : clean
clean:
	-rm -f *.xpr
	-rm -f vivado_*
	-rm -rf *.sim
	-rm -rf *.cache
	-rm -rf *.hw
	-rm -rf *.ip_user_files
	-rm -rf *.runs
	-rm -rf project2.srcs/sources_1/bd/top/hw_handoff
	-rm -rf project2.srcs/sources_1/bd/top/synth
	-rm -rf project2.srcs/sources_1/bd/top/sim
	-rm -rf project2.srcs/sources_1/bd/top/hdl
	-rm -rf project2.srcs/sources_1/bd/top/ip
	-rm -rf project2.srcs/sources_1/bd/top/ipshared
	-rm -rf project2.srcs/sources_1/bd/top/*.bxml
	-rm -rf project2.srcs/sources_1/bd/top/*.bmm
	-rm -rf project2

# List all Make targets.
.PHONY : list
list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | \
		awk -v RS= -F: '/^# File/,/^# Finished Make data base/ \
			{if ($$1 !~ "^[#.]") {print $$1}}' | \
		sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
