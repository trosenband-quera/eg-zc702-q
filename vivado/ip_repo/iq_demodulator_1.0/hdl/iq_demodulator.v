module iq_demodulator (
    input wire clk,                  // 100 MHz system clock
    input wire rst,
    output reg [15:0] adc_data_reg,
    output reg [31:0] adc_sample_count,
    output wire [15:0] phase_100k,
    output wire [15:0] phase_110k,
    output wire [15:0] phase_120k,
    // XADC configuration supplied externally
    input wire [15:0] INIT_40,
    input wire [15:0] INIT_41,
    input wire [15:0] INIT_42,
    input wire [15:0] INIT_48,
    input wire [15:0] INIT_49,
    input wire [15:0] INIT_4A,
    input wire [15:0] INIT_4B,
    input wire [15:0] INIT_4C,
    input wire [15:0] INIT_4D,
    input wire [15:0] INIT_4E,
    input wire [15:0] INIT_4F
);

// DDS parameters
localparam PHASE_WIDTH = 32;
localparam LUT_SIZE = 256;
localparam SAMPLE_RATE = 1000000;
localparam FREQ_100K =    100000;
localparam FREQ_110K =    110000;
localparam FREQ_120K =    120000;

// Phase increment = (freq * LUT_SIZE) / SAMPLE_RATE
localparam PHASE_INC_100K = (FREQ_100K * LUT_SIZE) / SAMPLE_RATE;
localparam PHASE_INC_110K = (FREQ_110K * LUT_SIZE) / SAMPLE_RATE;
localparam PHASE_INC_120K = (FREQ_120K * LUT_SIZE) / SAMPLE_RATE;

// DDS phase accumulators
reg [PHASE_WIDTH-1:0] phase_acc_100k = 0;
reg [PHASE_WIDTH-1:0] phase_acc_110k = 0;
reg [PHASE_WIDTH-1:0] phase_acc_120k = 0;

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
wire [7:0] addr_100k = phase_acc_100k[PHASE_WIDTH-1 -: 8];
wire [7:0] addr_110k = phase_acc_110k[PHASE_WIDTH-1 -: 8];
wire [7:0] addr_120k = phase_acc_120k[PHASE_WIDTH-1 -: 8];

always @(posedge adc_ready) begin
    if (rst) begin
        phase_acc_100k <= 0;
        phase_acc_110k <= 0;
        phase_acc_120k <= 0;
    end else begin
        phase_acc_100k <= phase_acc_100k + PHASE_INC_100K;
        phase_acc_110k <= phase_acc_110k + PHASE_INC_110K;
        phase_acc_120k <= phase_acc_120k + PHASE_INC_120K;
    end
end

// Mixers
wire signed [15:0] in_signal = {adc_in, 4'b0}; // scale to 16-bit
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

always @(posedge adc_ready) begin
    i_avg_100k <= (i_avg_100k >> 1) + (i_100k >> 1);
    q_avg_100k <= (q_avg_100k >> 1) + (q_100k >> 1);
    i_avg_110k <= (i_avg_110k >> 1) + (i_110k >> 1);
    q_avg_110k <= (q_avg_110k >> 1) + (q_110k >> 1);
    i_avg_120k <= (i_avg_120k >> 1) + (i_120k >> 1);
    q_avg_120k <= (q_avg_120k >> 1) + (q_120k >> 1);
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
    .INIT_40(INIT_40),
    .INIT_41(INIT_41),
    .INIT_42(INIT_42),
    .INIT_48(INIT_48),
    .INIT_49(INIT_49),
    .INIT_4A(INIT_4A),
    .INIT_4B(INIT_4B),
    .INIT_4C(INIT_4C),
    .INIT_4D(INIT_4D),
    .INIT_4E(INIT_4E),
    .INIT_4F(INIT_4F)
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

// ADC data sampling and counting, on xadc_ready rising edge
reg prev_xadc_ready;

always @(posedge clk) begin
    if (rst) begin
        adc_sample_count <= 0;
        adc_data_reg <= 0;
        prev_xadc_ready <= 0;
    end else begin
        if (xadc_ready && !prev_xadc_ready) begin
            adc_sample_count <= adc_sample_count + 1;
            adc_data_reg <= adc_data;
        end
        prev_xadc_ready <= xadc_ready;
    end
end

endmodule
