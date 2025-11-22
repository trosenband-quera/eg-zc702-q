
SELF := $(shell basename $(PWD))

default:
	vivado -source recreate.tcl

.PHONY : bit
bit:
	vivado -mode batch -source genbitstream.tcl

# configure FPGA for Xilinx 2025.1 tools
.PHONY: fpgacfg
fpgacfg:
	cp ../../vivado/iq_project/top.bit pl.bit
	bootgen -w -image bitstream.bif -arch zynq -process_bitstream bin
	ssh zc702 -C "mkdir -p /lib/firmware"
	scp top.bit.bin zc702:/lib/firmware
	ssh zc702 -C "echo 0 > /sys/class/fpga_manager/fpga0/flags"
	ssh zc702 -C "echo top.bit.bin > /sys/class/fpga_manager/fpga0/firmware"
	
.PHONY: bitcfg
bitcfg: bit fpgacfg

.PHONY : check
check:
	echo "Compile & elaborate to check Verilog."
	$(MAKE) -C xsim nosim
	
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
