module iq_demodulator (
    input wire clk,                  // 100 MHz system clock
    input wire rst,
    output reg [15:0] adc_data_reg,
    output reg [31:0] adc_sample_count,
    output wire [15:0] phase_100k,
    output wire [15:0] phase_110k,
    output wire [15:0] phase_120k
);

// DDS parameters
localparam PHASE_WIDTH = 32;
localparam LUT_SIZE = 256;
localparam SAMPLE_RATE = 1000000;
localparam FREQ_100K =    100000;
localparam FREQ_110K =    110000;
localparam FREQ_120K =    120000;

// Phase increment = (freq * LUT_SIZE) / SAMPLE_RATE
localparam DDS_PHASE_INC_100K = (FREQ_100K * LUT_SIZE) / SAMPLE_RATE;
localparam DDS_PHASE_INC_110K = (FREQ_110K * LUT_SIZE) / SAMPLE_RATE;
localparam DDS_PHASE_INC_120K = (FREQ_120K * LUT_SIZE) / SAMPLE_RATE;

// DDS phase accumulators
reg [PHASE_WIDTH-1:0] dds_phase_acc_100k = 0;
reg [PHASE_WIDTH-1:0] dds_phase_acc_110k = 0;
reg [PHASE_WIDTH-1:0] dds_phase_acc_120k = 0;

// LUT for sine and cosine
reg signed [15:0] sin_lut [0:LUT_SIZE-1];
reg signed [15:0] cos_lut [0:LUT_SIZE-1];

// Initialize LUT
integer i;
initial begin
    for (i = 0; i < LUT_SIZE; i = i + 1) begin
        sin_lut[i] = $rtoi(32767 * $sin(2.0 * 3.1415926535 * i / LUT_SIZE));
        cos_lut[i] = $rtoi(32767 * $cos(2.0 * 3.1415926535 * i / LUT_SIZE));
    end
end

// DDS update
wire [7:0] addr_100k = dds_phase_acc_100k[PHASE_WIDTH-1 -: 8];
wire [7:0] addr_110k = dds_phase_acc_110k[PHASE_WIDTH-1 -: 8];
wire [7:0] addr_120k = dds_phase_acc_120k[PHASE_WIDTH-1 -: 8];

// Mixers
wire signed [15:0] in_signal = adc_data_reg;
wire signed [31:0] i_100k = in_signal * cos_lut[addr_100k];
wire signed [31:0] q_100k = in_signal * sin_lut[addr_100k];
wire signed [31:0] i_110k = in_signal * cos_lut[addr_110k];
wire signed [31:0] q_110k = in_signal * sin_lut[addr_110k];
wire signed [31:0] i_120k = in_signal * cos_lut[addr_120k];
wire signed [31:0] q_120k = in_signal * sin_lut[addr_120k];

// Simple moving average filter (low-pass)
reg signed [31:0] i_avg_100k = 0, q_avg_100k = 0;
reg signed [31:0] i_avg_110k = 0, q_avg_110k = 0;
reg signed [31:0] i_avg_120k = 0, q_avg_120k = 0;


// ADC data sampling and counting, on xadc_ready rising edge
reg prev_xadc_ready;

always @(posedge clk) begin
    if (rst) begin
        adc_sample_count <= 0;
        adc_data_reg <= 0;
        prev_xadc_ready <= 0;

        dds_phase_acc_100k <= 0;
        dds_phase_acc_110k <= 0;
        dds_phase_acc_120k <= 0;
    end else begin
        if (xadc_ready && !prev_xadc_ready) begin
            adc_sample_count <= adc_sample_count + 1;
            adc_data_reg <= adc_data;

            // Update DDS phase accumulators
            dds_phase_acc_100k <= dds_phase_acc_100k + DDS_PHASE_INC_100K;
            dds_phase_acc_110k <= dds_phase_acc_110k + DDS_PHASE_INC_110K;
            dds_phase_acc_120k <= dds_phase_acc_120k + DDS_PHASE_INC_120K;

            // Update moving average filters
            i_avg_100k <= (i_avg_100k >> 1) + (i_100k >> 1);
            q_avg_100k <= (q_avg_100k >> 1) + (q_100k >> 1);

            i_avg_110k <= (i_avg_110k >> 1) + (i_110k >> 1);
            q_avg_110k <= (q_avg_110k >> 1) + (q_110k >> 1);

            i_avg_120k <= (i_avg_120k >> 1) + (i_120k >> 1);
            q_avg_120k <= (q_avg_120k >> 1) + (q_120k >> 1);
        end
        prev_xadc_ready <= xadc_ready;
    end
end

// Phase calculation using CORDIC 
cordic_atan2 cordic_100k (
    .clk(clk),
    .rst(rst),
    .x(i_avg_100k[31:16]),
    .y(q_avg_100k[31:16]),
    .phase(phase_100k)
);

cordic_atan2 cordic_110k (
    .clk(clk),
    .rst(rst),
    .x(i_avg_110k[31:16]),
    .y(q_avg_110k[31:16]),
    .phase(phase_110k)
);

cordic_atan2 cordic_120k (
    .clk(clk),
    .rst(rst),
    .x(i_avg_120k[31:16]),
    .y(q_avg_120k[31:16]),
    .phase(phase_120k)
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
