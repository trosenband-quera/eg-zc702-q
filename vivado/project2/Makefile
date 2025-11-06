
SELF := $(shell basename $(PWD))

default:
	vivado -source recreate.tcl

.PHONY : clean
clean:
	-rm -f *.xpr
	-rm -rf *.sim
	-rm -rf *.cache
	-rm -rf *.hw
	-rm -rf *.ip_user_files
	-rm -rf *.runs
	-rm -rf project2

# List all Make targets.
.PHONY : list
list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | \
		awk -v RS= -F: '/^# File/,/^# Finished Make data base/ \
			{if ($$1 !~ "^[#.]") {print $$1}}' | \
		sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
