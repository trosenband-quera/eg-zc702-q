`timescale 1ns / 1ps

module testbench;
reg RESET;
reg DCLK;
wire [4:0] CHANNEL;
initial
begin
DCLK = 0;
RESET = 1;
#100 RESET = 0;
end
always #(10) DCLK= ~DCLK;
// Instantiate the Unit Under Test (UUT)
iq_demodulator_v1_0_S00_AXI uut (
.S_AXI_ARESETN (~RESET),
.S_AXI_ACLK (DCLK)
);
endmodule