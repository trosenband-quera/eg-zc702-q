Vivado
======

These are example Vivado projects for generating bitstream files suitable for
congfiguring the Programmable Logic (PL) part of the Zynq FPGA.


iq_project
-------------

This is a multi-channel IQ-demodulator.  Demodulator Verilog code is a `custom IP`
in `../ip_repo/iq_demodulator_1.0/hdl`.  Some parameters related to the AXI-bus are in
`../ip_repo/iq_demodulator_1.0/component.xml`

`make` generates a Vivado project and runs in "Project Mode" where Vivado
provides all workflow infrastructure and integration with GUI bells and whistles.
This allows generation of a bitstream.

`make bit` generates the bitstream `top.bit` w/o GUI.

`make bitcfg` same as above and configure attached zc702 board.

`make check` check for Verilog errors.

`make clean` removes all generated files.

This workflow has been tested on Vivado 2018.2 and 2025.1 under Ubuntu.

Software to control the demodulator is in `../gcc/iq`.

Project generation via TCL script is derived from `projmode-led8` below.

projmode-led8
-------------

This is the simplest Vivado project and runs in "Project Mode" where Vivado
provides all workflow infrastructure and integration with the GUI.
The goal is to generate a bitstream equivalent to the one provided with the
2016.2-mod boot image files called `pl.bit`.
To get started:

- `cd projmode-led8`
- Start the Vivado GUI and recreate the project
    `vivado -source recreate.tcl`
- For XADC, run `vivado -source xadc_project.tcl` -- tested w/ version 2018.2
- Make design changes (optional, skip this first time)
- On LHS of the GUI click "Generate Bitstream"
    - Once finished running (a few minutes) a bitstream file will be available
      as `projmode-led8.runs/impl_1/led8_wrapper.bit`
- Test the new bitstream
    - `scp led8_wrapper.bit zc702:~/`
    - `ssh zc702 -C "cat led8_wrapper.bit > /dev/xdevcfg"`
    - The LEDs should be in the pattern 0x55.

Note that this method of using the GUI requires much more care and attention
with respect to source control.
You'll have to painstakingly figure out which files to add and which to ignore.
Non-project mode which doesn't rely on the GUI so much is an easier option to
use with source control.


nonproj-led8
------------

This is roughly equivalent in function to projmode-led8 but without the managed
project structure.

    +-nonproj-led8
        |
        +-hdl       Verilog, SystemVerilog, VHDL source files.
            |
            top_m.v         Top level module for connecting ps7, rst, and PL.
            pl_m.sv         Programmable Logic module.
            pl_m_stub.v     Programmable Logic stub, for synth only.
            ...             Other source files.
        +-xdc       Constraints.
            |
            clkrst.xdc      Clock and reset constraints.
            ddr.xdc         DDR pins, usually leave alone.
            mio.xdc         Multiplexed IO pin constraints.
            ...             Other constraints.
        +-tcl       TCL scripts to control Vivado.
            |
            cfg.tcl         Config file containing parameters, e.g. part number.
            common.tcl      Config file should be sourced first in TCL scripts.
            local.tcl       Untracked configuration changes go here.
            synth_ip.tcl    Script to synthesize all Xilinx catalog IP.
            synth_ip_*.tcl  Script to synthesize a specific Xilinx catalog IP.
            top.tcl         Main script for compliation to bitstream.
            ...             Other TCL scripts
        +-build     Generated files appear in here.
        +-release   Optional place for pre-built bitstreams etc.
        README.md   Optional, Basic documentation.
        Makefile    Make recipies to control Vivado with scripts under `tcl`.

This file structure is quite flat and because all generated files appear under
build it is easy to ignore them for version control.
Note that Vivado will generate hidden directories too which are dealt with by
`.gitignore` and `make clean`.

To get started run `make all` to generate the bitstream, send it to the zc702
(assuming you have SSH configured), and program the bitstream using the xdevcfg
driver.
