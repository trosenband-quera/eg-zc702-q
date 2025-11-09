module iq_demodulator #(
    parameter integer NUM_CHANNELS = 3,
    parameter integer PHASE_WIDTH = 32,
    parameter integer LUT_SIZE = 256,
    parameter integer SAMPLE_RATE_HZ = 1000000,
    parameter FREQ_HZ = {24'd100000, 24'd110000, 24'd120000} // in Hz, one entry per channel
) (
    input wire clk,                  // 100 MHz system clock
    input wire rst,
    output reg [15:0] adc_data_reg,
    output reg [31:0] adc_sample_count,
    output wire [(16*NUM_CHANNELS-1):0] phases
);

// DDS parameters
localparam COS_OFFSET = LUT_SIZE / 4;

// DDS phase accumulators (separate for sin and cos)
reg [PHASE_WIDTH-1:0] dds_phase_acc_sin[NUM_CHANNELS-1:0];
reg [PHASE_WIDTH-1:0] dds_phase_acc_cos[NUM_CHANNELS-1:0];

// LUT for sine only
reg signed [15:0] sin_lut [0:LUT_SIZE-1];

// Initialize LUT
integer index;
real sinval;
initial begin
    for (index = 0; index < LUT_SIZE; index = index + 1) begin
		sinval = 32767 * $sin(2.0 * 3.1415926535 * index / LUT_SIZE);
        sin_lut[index] = sinval;
    end
end

// DDS update (separate for sin and cos)
wire [7:0] addr_sin[NUM_CHANNELS-1:0];
wire [7:0] addr_cos[NUM_CHANNELS-1:0];
genvar ch;
generate
    for (ch = 0; ch < NUM_CHANNELS; ch = ch + 1) begin : dds_addr_gen
        assign addr_sin[ch] = dds_phase_acc_sin[ch][PHASE_WIDTH-1 -: 8];
        assign addr_cos[ch] = dds_phase_acc_cos[ch][PHASE_WIDTH-1 -: 8];
    end
endgenerate

// Mixers (use same LUT for sin and cos)
wire signed [15:0] in_signal = adc_data_reg;
wire signed [31:0] mixerI[NUM_CHANNELS-1:0];
wire signed [31:0] mixerQ[NUM_CHANNELS-1:0];
generate
    for (ch = 0; ch < NUM_CHANNELS; ch = ch + 1) begin : mixer_gen
        assign mixerI[ch] = in_signal * sin_lut[addr_cos[ch]];
        assign mixerQ[ch] = in_signal * sin_lut[addr_sin[ch]];
    end
endgenerate

// Simple moving average filter (low-pass)
reg signed [31:0] i_avg[NUM_CHANNELS-1:0];
reg signed [31:0] q_avg[NUM_CHANNELS-1:0];

// ADC data sampling and counting, on xadc_ready rising edge
// see UG480 p. 83-85 for a complete example which seems to
// contradict p. 74 "DEN should only go high for one DCLK period."

reg prev_xadc_ready;
wire [15:0] adc_data;

always @(posedge clk) begin
	if (rst) begin
		adc_sample_count <= 0;
		adc_data_reg <= 0;
		prev_xadc_ready <= 0;
	end else begin
		// init read
		if (xadc_ready == 1 && prev_xadc_ready == 0) begin
			adc_sample_count <= adc_sample_count + 1;
			adc_data_reg <= adc_data;
		end
		prev_xadc_ready <= xadc_ready;
	end
end

// for each channel, update DDS phase, I/Q, run CORDIC
generate
	for (ch = 0; ch < NUM_CHANNELS; ch = ch + 1) begin
		always @(posedge clk) begin
			if (rst) begin
				dds_phase_acc_sin[ch] <= 0;
				dds_phase_acc_cos[ch] <= COS_OFFSET;
				i_avg[ch] <= 0;
				q_avg[ch] <= 0;
			end else begin
				if (xadc_ready == 1 && prev_xadc_ready == 0) begin
					dds_phase_acc_sin[ch] <= dds_phase_acc_sin[ch] + (FREQ_HZ[(ch*24+23):(ch*24)] * LUT_SIZE) / SAMPLE_RATE_HZ;
					dds_phase_acc_cos[ch] <= dds_phase_acc_cos[ch] + (FREQ_HZ[(ch*24+23):(ch*24)] * LUT_SIZE) / SAMPLE_RATE_HZ;
					i_avg[ch] <= (i_avg[ch] >> 1) + (mixerI[ch] >> 1);
					q_avg[ch] <= (q_avg[ch] >> 1) + (mixerQ[ch] >> 1);
				end
				
				
			end
		end

        cordic_atan2 cordic_inst (
            .clk(clk),
            .rst(rst),
            .x(i_avg[ch][31:16]),
            .y(q_avg[ch][31:16]),
            .phase(phases[(16*ch+15):(16*ch)])
        );
    end
endgenerate

// XADC
wire xadc_ready;

XADC #( // see Xilinx UG480 for details, p. 22 and 44
    .INIT_40(16'h0403), // no avg, continuous sampling, bipolar, single channel VP/VN
    .INIT_41(16'h3F0F), // no sequencer, no alarms, no calibration
    .INIT_42(16'h0200), // ADCCLK = clk/4.  26 ADCCLK per conversion
    .INIT_48(16'h0000), // Sequencer mode
    .INIT_49(16'h0000),
    .INIT_4A(16'h0000),
    .INIT_4B(16'h0000),
    .INIT_4C(16'h0000),
    .INIT_4D(16'h0000),
    .INIT_4E(16'h0000),
    .INIT_4F(16'h0000),
    .SIM_MONITOR_FILE("voltages.txt")// Analog Stimulus file for simulation
) adc_inst (
	.CONVST(1'b0), // not used
	.CONVSTCLK(1'b0), // not used
    .DADDR(7'h03),    // Address for channel selection (Vp/Vn), see UH480, p. 36
    .DCLK(clk),
    .DEN(eoc),
    .DWE(1'b0),
    .DI(16'h0000),
    .DRDY(xadc_ready),
    .DO(adc_data),
    .RESET(rst),
    .EOC (eoc),
	.EOS (eos),
	.BUSY (busy),
    .VAUXN(16'h0000),
	.VAUXP(16'h0000),
	.JTAGBUSY(),// not used
	.JTAGLOCKED(),// not used
	.JTAGMODIFIED(), // not used
	.VP (VP),
	.VN (VN)
);

endmodule
