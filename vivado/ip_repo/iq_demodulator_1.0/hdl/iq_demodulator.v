module iq_demodulator #(
    parameter integer NUM_CHANNELS = 3,
    parameter integer PHASE_WIDTH = 32,
    parameter integer LUT_SIZE = 256,
    parameter integer freq [0:NUM_CHANNELS-1] = '{100000, 110000, 120000}
) (
    input wire clk,                  // 100 MHz system clock
    input wire rst,
    output reg [15:0] adc_data_reg,
    output reg [31:0] adc_sample_count,
    output wire [15:0] phase[NUM_CHANNELS-1:0]
);

// DDS parameters
localparam COS_OFFSET = LUT_SIZE / 4;
localparam SAMPLE_RATE = 1000000;

// Phase increment array
localparam int DDS_PHASE_INC[NUM_CHANNELS];
integer ch;
initial begin
    for (ch = 0; ch < NUM_CHANNELS; ch = ch + 1) begin
        DDS_PHASE_INC[i] = (freq[i] * LUT_SIZE) / SAMPLE_RATE;
    end
end
// DDS phase accumulators (separate for sin and cos)
reg [PHASE_WIDTH-1:0] dds_phase_acc_sin[NUM_CHANNELS-1:0];
reg [PHASE_WIDTH-1:0] dds_phase_acc_cos[NUM_CHANNELS-1:0];

// LUT for sine only
reg signed [15:0] sin_lut [0:LUT_SIZE-1];

// Initialize LUT
integer i;
initial begin
    for (i = 0; i < LUT_SIZE; i = i + 1) begin
        sin_lut[i] = $rtoi(32767 * $sin(2.0 * 3.1415926535 * i / LUT_SIZE));
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
wire signed [31:0] i[NUM_CHANNELS-1:0];
wire signed [31:0] q[NUM_CHANNELS-1:0];
generate
    for (ch = 0; ch < NUM_CHANNELS; ch = ch + 1) begin : mixer_gen
        assign i[ch] = in_signal * sin_lut[addr_cos[ch]];
        assign q[ch] = in_signal * sin_lut[addr_sin[ch]];
    end
endgenerate

// Simple moving average filter (low-pass)
reg signed [31:0] i_avg[NUM_CHANNELS-1:0];
reg signed [31:0] q_avg[NUM_CHANNELS-1:0];

// ADC data sampling and counting, on xadc_ready rising edge
reg prev_xadc_ready;

integer ch_idx;
always @(posedge clk) begin
    if (rst) begin
        adc_sample_count <= 0;
        adc_data_reg <= 0;
        prev_xadc_ready <= 0;
        for (ch_idx = 0; ch_idx < NUM_CHANNELS; ch_idx = ch_idx + 1) begin
            dds_phase_acc_sin[ch_idx] <= 0;
            dds_phase_acc_cos[ch_idx] <= COS_OFFSET;
            i_avg[ch_idx] <= 0;
            q_avg[ch_idx] <= 0;
        end
    end else begin
        if (xadc_ready && !prev_xadc_ready) begin
            adc_sample_count <= adc_sample_count + 1;
            adc_data_reg <= adc_data;
            for (ch_idx = 0; ch_idx < NUM_CHANNELS; ch_idx = ch_idx + 1) begin
                dds_phase_acc_sin[ch_idx] <= dds_phase_acc_sin[ch_idx] + DDS_PHASE_INC[ch_idx];
                dds_phase_acc_cos[ch_idx] <= dds_phase_acc_cos[ch_idx] + DDS_PHASE_INC[ch_idx];
                i_avg[ch_idx] <= (i_avg[ch_idx] >> 1) + (i[ch_idx] >> 1);
                q_avg[ch_idx] <= (q_avg[ch_idx] >> 1) + (q[ch_idx] >> 1);
            end
        end
        prev_xadc_ready <= xadc_ready;
    end
end

// Phase calculation using CORDIC
generate
    for (ch = 0; ch < NUM_CHANNELS; ch = ch + 1) begin : cordic_gen
        cordic_atan2 cordic_inst (
            .clk(clk),
            .rst(rst),
            .x(i_avg[ch][31:16]),
            .y(q_avg[ch][31:16]),
            .phase(phase[ch])
        );
    end
endgenerate
// XADC
wire xadc_ready;

XADC #( // see Xilinx UG480 for details, p. 22 and44
    .INIT_40(16'h7403), // no acg, continuous sampling, bipolar, single channel VP/VN
    .INIT_41(16'h2F0F), // no sequencer, no alarms, no calibration
    .INIT_42(16'h0400), // ADCCLK = clk/4.  26 ADCCLK per conversion
    .INIT_48(16'h0000), // Sequencer mode
    .INIT_49(16'h0000),
    .INIT_4A(16'h0000),
    .INIT_4B(16'h0000),
    .INIT_4C(16'h0000),
    .INIT_4D(16'h0000),
    .INIT_4E(16'h0000),
    .INIT_4F(16'h0000)
) adc_inst (
    .DADDR(7'h14),       // Address for channel selection (e.g., temperature, Vp/Vn)
    .DCLK(clk),
    .DEN(1'b1),
    .DI(16'h0000),
    .DRDY(xadc_ready),
    .DO(adc_data),
    .RESET(rst),
    .VP(1'b0),
    .VN(1'b0)
);

endmodule
