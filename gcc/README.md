C programs with GCC
===================

Each directory here contains a single C program in a .c file with the same name
as the directory.
To compile and run them please see the individual Makefiles for definitive
information.
Typically, you'll want to do something like this:

    cd hello
    make            # Build binary.
    make transfer   # Use SCP to copy binary onto target.
    ssh zc702       # Get a terminal on target.
    ./hello         # Run binary on target.

Some programs may have other make target/dependency combinations to simplify the
build, copy, ssh, and run process.
These Makefiles are kept very simple by using the implict rules.

iq
----

Run the IQ demodulator.  Needs analog input (0-1 V) to pins VP/VN.
    cd iq
    make            # Build binary.
    make transfer   # Use SCP to copy binary onto target.
    make fpgacfg    # Program the FPGA logic for IQ demodulation.
    make run        # Simple test run
    make getdata    # acquire IQ data and plot

 ```
tr@ubuntu-quera-tr:~/eg-zc702-q/gcc/iq$ make run
scp iq zc702:~/
iq                                                                                                            100% 1602KB   4.6MB/s   00:00    
ssh zc702 -C "./iq"
Channel[ 0] NSAMP (index 0): scale = 1
Channel[ 1] VP-VN (index 1): scale = 1.52588e-05
Channel[ 2] PHASE0 (index 2): scale = 9.58738e-05
Channel[ 3] PHASE1 (index 3): scale = 9.58738e-05
Channel[ 4] PHASE2 (index 4): scale = 9.58738e-05
Channel[ 5] mixI (index 5): scale = 2.32831e-10
Channel[ 6] mixQ (index 6): scale = 2.32831e-10
Channel[ 7] avgmixI (index 7): scale = 2.32831e-10
Channel[ 8] avgmixQ (index 8): scale = 2.32831e-10
Channel[ 9] LO_I (index 9): scale = 2.32831e-10
Start time: 28968.137094007
LO 0, target = 100000.000 Hz, true = 100000.000048 Hz, phase delta = 446676598.784
LO 1, target = 112500.000 Hz, true = 112500.000082 Hz, phase delta = 502511173.632
  offset address:     0x0080   0x0084   0x0088   0x008C   0x0090     0x00FC     0x00F8     0x00F4     0x00F0     0x00EC
   Channel names:      NSAMP    VP-VN   PHASE0   PHASE1   PHASE2       mixI       mixQ    avgmixI    avgmixQ       LO_I
   RAW   1209 us: 0x00000037   0xF265   0x8706   0x213E   0xF8FA 0xF0B743BA 0x162D956C 0xFFFE6506 0x00009D65 0x5ED755F5
   RAW   6313 us: 0x00001362   0xF83C   0x5EB4   0x8CF0   0x7CDA 0xFF4E19CC 0x24DE1524 0xFF5ADF0A 0x00AF786A 0xCC218AFC
   RAW  11417 us: 0x0000268E   0x17C1   0x5BAE   0x7310   0x81F0 0x088A22D6 0x149E0122 0xFEDEA263 0x0167FCF5 0x00007FFF
   RAW  16522 us: 0x000039BB   0xD2B5   0x5856   0x4D48   0x80AA 0x08A178D4 0x27FBDDC2 0xFE89EABA 0x02254954 0x36BA8C4B
   RAW  21625 us: 0x00004CE5   0x624C   0x550C   0xF8FA   0x80AA 0xE3E11310 0x0A0FCFC4 0xFE5C277F 0x02E1EFF5 0x80280648
   RAW  26733 us: 0x00006016   0x5F7C   0x51B4   0x9EC2   0x7F66 0xF368148D 0x0EDE5914 0xFE54596A 0x0399C4FD 0xB1419B18
   RAW  31839 us: 0x00007343   0xBCF4   0x4E36   0x7CE4   0x7F66 0xFDC58C80 0x00FCDF20 0xFE705728 0x0446FE26 0x7FF50324
   RAW  36944 us: 0x00008670   0x4A70   0x4ABE   0x4CF0   0x8008 0xF973D680 0xFBC24656 0xFEAD95E5 0x04E57B48 0x8C4BC946
   RAW  42048 us: 0x0000999B   0xA282   0x472A   0x0D48   0x80AA 0x0E24A7E9 0x123AD898 0xFF08DD5C 0x05714D0F 0x06487FD8
   RAW  47151 us: 0x0000ACC6   0x285E   0x436E   0xB310   0x8008 0x27E7CB40 0x1D56BBF8 0xFF7E56B3 0x05E69FBB 0x6DC9BE32
sample rate: 961.538 kHz
```


xadc
----

Test the FPGA's ADC converter.
    cd xadc
    make            # Build binary.
    make transfer   # Use SCP to copy binary onto target.
    ssh zc702       # Get a terminal on target.
    ./xadc          # Run binary on target.
    exit            # back to host
    make getdata    # acquire XADC data and plot
    make gethist    # acquire XADC data and show histogram
![histogram](xadc/hist.png)


hello
-----

A simple "Hello World!" which has code split over two c files.
This is the first program to attempt building and running.
To get started `make clean && make && make run` should return 0.


realmat
-------

Print information and some analysis on real matrices.
All code is contained in a single file.
This is the second program to attempt running.

This program uses the following features which may be used as examples:

- Reading from files.
- Allocating and freeing memory.
- Recursive function (`determinant()`).
- Parsing command line options with argp.
- Linking to math library (`math.h/libm`).

The file format is simply that column are separated by whitespace and rows are
separated by newlines.

This code is not specific to Zynq/ARM and should build and run without error or
warning on Linux and Windows.

ps7axim
-------

Drive the AXI master ports of the Zynq (`ps7`).
This is useful for peek/poking and linking the host to the PL via the route
host-script -> SSH -> ps7axim -> PL.

Accesses are performed by using `mmap()` and `/dev/mem` to get a pointer to the
memory mapped address of the ports.

This is specific to Zynq as the AXI ports have hard-coded physical addresses
at `0x4000_0000` (port 0), and `0x8000_0000` (port 1), both with a range of 1GB.

This may be used in conjunction with the `vivado/nonproj-led8` bitstream to
control the pattern of the 8 LEDs.
The bitsteam from `projmode-led8` is also usable but does not have a makefile
for easy reproduction.

    # Configure programmable logic with bitstream.
    cd <eg-zc702>/vivado/nonproj-led8
    make bitstream && make transfer && make program

    # Compile ARM linux binary and transfer to zc702.
    cd <eg-zc702>/(gcc|clang)/ps7axim
    make && make transfer

    # Get a terminal on zc702.
    ssh zc702

    # Read/write the addressable LEDs.
    ./ps7axim -w 0x12345678 0   # Set the LEDs to 0x78, full 32b is stored.
    ./ps7axim -r 0              # Read back the stored value (0x12345678).

Read are always executed before writes so this can be used to get the current
value and set a new one in the same command by using both `-r` and `-w`.
