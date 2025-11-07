module iq_demodulator #(
    parameter integer FREQ0 = 100000,
    parameter integer FREQ1 = 110000,
    parameter integer FREQ2 = 120000
) (
    input wire clk,                  // 100 MHz system clock
    input wire rst,
    output reg [15:0] adc_data_reg,
    output reg [31:0] adc_sample_count,
    output wire [15:0] phase0,
    output wire [15:0] phase1,
    output wire [15:0] phase2
);

// DDS parameters
localparam PHASE_WIDTH = 32;
localparam LUT_SIZE = 256;
localparam COS_OFFSET = LUT_SIZE / 4;
localparam SAMPLE_RATE = 1000000;

// Phase increment = (freq * LUT_SIZE) / SAMPLE_RATE
localparam DDS_PHASE_INC0 = (FREQ0 * LUT_SIZE) / SAMPLE_RATE;
localparam DDS_PHASE_INC1 = (FREQ1 * LUT_SIZE) / SAMPLE_RATE;
localparam DDS_PHASE_INC2 = (FREQ2 * LUT_SIZE) / SAMPLE_RATE;

// DDS phase accumulators
reg [PHASE_WIDTH-1:0] dds_phase_acc0 = 0;
reg [PHASE_WIDTH-1:0] dds_phase_acc1 = 0;
reg [PHASE_WIDTH-1:0] dds_phase_acc2 = 0;

// DDS phase accumulators (separate for sin and cos)
reg [PHASE_WIDTH-1:0] dds_phase_acc0_sin = 0;
reg [PHASE_WIDTH-1:0] dds_phase_acc0_cos = 0;
reg [PHASE_WIDTH-1:0] dds_phase_acc1_sin = 0;
reg [PHASE_WIDTH-1:0] dds_phase_acc1_cos = 0;
reg [PHASE_WIDTH-1:0] dds_phase_acc2_sin = 0;
reg [PHASE_WIDTH-1:0] dds_phase_acc2_cos = 0;

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
wire [7:0] addr0_sin = dds_phase_acc0_sin[PHASE_WIDTH-1 -: 8];
wire [7:0] addr0_cos = dds_phase_acc0_cos[PHASE_WIDTH-1 -: 8];
wire [7:0] addr1_sin = dds_phase_acc1_sin[PHASE_WIDTH-1 -: 8];
wire [7:0] addr1_cos = dds_phase_acc1_cos[PHASE_WIDTH-1 -: 8];
wire [7:0] addr2_sin = dds_phase_acc2_sin[PHASE_WIDTH-1 -: 8];
wire [7:0] addr2_cos = dds_phase_acc2_cos[PHASE_WIDTH-1 -: 8];

// Mixers (use same LUT for sin and cos)
wire signed [15:0] in_signal = adc_data_reg;
wire signed [31:0] i0 = in_signal * sin_lut[addr0_cos];
wire signed [31:0] q0 = in_signal * sin_lut[addr0_sin];
wire signed [31:0] i1 = in_signal * sin_lut[addr1_cos];
wire signed [31:0] q1 = in_signal * sin_lut[addr1_sin];
wire signed [31:0] i2 = in_signal * sin_lut[addr2_cos];
wire signed [31:0] q2 = in_signal * sin_lut[addr2_sin];

// Simple moving average filter (low-pass)
reg signed [31:0] i_avg0 = 0, q_avg0 = 0;
reg signed [31:0] i_avg1 = 0, q_avg1 = 0;
reg signed [31:0] i_avg2 = 0, q_avg2 = 0;


// ADC data sampling and counting, on xadc_ready rising edge
reg prev_xadc_ready;

always @(posedge clk) begin
    if (rst) begin
        adc_sample_count <= 0;
        adc_data_reg <= 0;
        prev_xadc_ready <= 0;

        dds_phase_acc0_sin <= 0;
        dds_phase_acc0_cos <= COS_OFFSET;
        dds_phase_acc1_sin <= 0;
        dds_phase_acc1_cos <= COS_OFFSET;
        dds_phase_acc2_sin <= 0;
        dds_phase_acc2_cos <= COS_OFFSET;

    end else begin
        if (xadc_ready && !prev_xadc_ready) begin
            adc_sample_count <= adc_sample_count + 1;
            adc_data_reg <= adc_data;

            // Update DDS phase accumulators (separate for sin and cos)
            dds_phase_acc0_sin <= dds_phase_acc0_sin + DDS_PHASE_INC0;
            dds_phase_acc0_cos <= dds_phase_acc0_cos + DDS_PHASE_INC0;
            dds_phase_acc1_sin <= dds_phase_acc1_sin + DDS_PHASE_INC1;
            dds_phase_acc1_cos <= dds_phase_acc1_cos + DDS_PHASE_INC1;
            dds_phase_acc2_sin <= dds_phase_acc2_sin + DDS_PHASE_INC2;
            dds_phase_acc2_cos <= dds_phase_acc2_cos + DDS_PHASE_INC2;

            // Update moving average filters
            i_avg0 <= (i_avg0 >> 1) + (i0 >> 1);
            q_avg0 <= (q_avg0 >> 1) + (q0 >> 1);

            i_avg1 <= (i_avg1 >> 1) + (i1 >> 1);
            q_avg1 <= (q_avg1 >> 1) + (q1 >> 1);

            i_avg2 <= (i_avg2 >> 1) + (i2 >> 1);
            q_avg2 <= (q_avg2 >> 1) + (q2 >> 1);
        end
        prev_xadc_ready <= xadc_ready;
    end
end

// Phase calculation using CORDIC 
cordic_atan2 cordic0 (
    .clk(clk),
    .rst(rst),
    .x(i_avg0[31:16]),
    .y(q_avg0[31:16]),
    .phase(phase0)
);

cordic_atan2 cordic1 (
    .clk(clk),
    .rst(rst),
    .x(i_avg1[31:16]),
    .y(q_avg1[31:16]),
    .phase(phase1)
);

cordic_atan2 cordic2 (
    .clk(clk),
    .rst(rst),
    .x(i_avg2[31:16]),
    .y(q_avg2[31:16]),
    .phase(phase2)
);

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
