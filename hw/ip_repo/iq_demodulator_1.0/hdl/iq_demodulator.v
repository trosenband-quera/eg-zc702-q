// multichannel IQ demodulator with XADC input
// uses CORDIC atan2 to compute phase from I/Q
// generate NUM_CHANNELS demodulators in parallel

`timescale 1 ns / 1 ps

module iq_demodulator #(
    parameter integer NUM_CHANNELS = 3,
    parameter integer LO_PHASE_WIDTH = 32,
    parameter integer LUT_WIDTH = 8,
    parameter integer OUTPUT_PHASE_WIDTH = 32,
    parameter integer CORDIC_PHASE_WIDTH = 16,
    parameter integer CORDIC_WRAP_WIDTH = 8,
    parameter integer NUM_DEBUG = 8,
    parameter integer NUM_XADC = 1,  // set to 1 to enable XADC instantiation
    parameter integer NUM_ADAQ4001 = 0  // set to 1 to enable ADAQ4001 instantiation
) (
    input  wire                         clk,               // 100 MHz system clock
    input  wire                         rst,
    input  wire [(LO_PHASE_WIDTH*NUM_CHANNELS-1):0] lo_dds_phase_inc,
    input  wire signed [15:0]           kp,              // proportional gain for reference phase error correction
    input  wire signed [15:0]           ki,              // integral gain for reference phase error correction
    input  wire [15:0] signal_good_threshold,
    output reg  [                 15:0] adc_data_reg,
    output reg  [                 31:0] adc_sample_count,
    output wire [(OUTPUT_PHASE_WIDTH*NUM_CHANNELS-1):0] phases,
    output wire [(NUM_CHANNELS*CORDIC_WRAP_WIDTH-1):0] wraps,
    output wire [NUM_DEBUG*32-1:0] debug,
    output reg  [NUM_CHANNELS-1:0]   signal_good
  );
  localparam integer IQ_AVG_ORDER = 5;  // 5th order low-pass filter for I/Q
  localparam integer IQ_AVG_SHIFT = 6;  // 1/e filter shift (2^n samples)
  localparam integer IQ_AVG_COEFF = 1;  // 1/e filter coefficient

  // DDS parameters
  localparam integer LUT_SIZE = 2**LUT_WIDTH;
  localparam integer COS_OFFSET = (LUT_SIZE / 4) << (LO_PHASE_WIDTH - LUT_WIDTH);

  // DDS phase accumulators (separate for sin and cos)
  reg [LO_PHASE_WIDTH-1:0] dds_phase_acc_sin[NUM_CHANNELS-1:0];
  reg [LO_PHASE_WIDTH-1:0] dds_phase_acc_cos[NUM_CHANNELS-1:0];

  reg [LO_PHASE_WIDTH-1:0] lo_dds_phase_inc_reg[NUM_CHANNELS-1:0];

  // LUT for sine only
  reg signed [15:0] sin_lut[0:LUT_SIZE-1];

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
    for (ch = 0; ch < NUM_CHANNELS; ch = ch + 1) begin : gen_dds_addr
      assign addr_sin[ch] = dds_phase_acc_sin[ch][LO_PHASE_WIDTH-1-:8];
      assign addr_cos[ch] = dds_phase_acc_cos[ch][LO_PHASE_WIDTH-1-:8];
    end
  endgenerate

  // Mixers (use same LUT for sin and cos)
  wire signed [15:0] in_signal = adc_data_reg;
  reg signed [31:0] mixerI[NUM_CHANNELS-1:0];
  reg signed [31:0] mixerQ[NUM_CHANNELS-1:0];
  
  // low-pass filtered I/Q
  wire signed [31:0] i_avg[NUM_CHANNELS-1:0];
  wire signed [31:0] q_avg[NUM_CHANNELS-1:0];

  // final signal average
  reg [31:0] signal_avg[NUM_CHANNELS-1:0];

  wire [31:0] debug_array [NUM_DEBUG-1:0];
  assign debug_array[0] = {mixerI[0][31:16], mixerQ[0][31:16]};
  assign debug_array[1] = {i_avg[0][31:16], q_avg[0][31:16]};
  assign debug_array[2] = {sin_lut[addr_cos[0]], sin_lut[addr_sin[0]]};

  assign debug_array[3] = lo_dds_phase_inc_reg[0];
  assign debug_array[4] = {signal_avg[0][15:0], signal_avg[1][15:0]};
  assign debug_array[5] = {signal_avg[2][15:0], signal_avg[3][15:0]};
  
  // ADC data sampling and counting, on adc_ready rising edge
  wire adc_ready;
  reg prev_adc_ready;
  wire [15:0] adc_data;
  // reference phase = channel 0
  wire signed [CORDIC_PHASE_WIDTH-1:0] phases_array [NUM_CHANNELS-1:0];
  wire signed [CORDIC_WRAP_WIDTH-1:0] wraps_array [NUM_CHANNELS-1:0];
  wire [CORDIC_PHASE_WIDTH:0] magnitude_array [NUM_CHANNELS-1:0]; // should be positive

  generate
    for (ch = 0; ch < NUM_CHANNELS; ch = ch + 1) begin : gen_phases
      assign phases[(OUTPUT_PHASE_WIDTH*ch+CORDIC_PHASE_WIDTH-1):(OUTPUT_PHASE_WIDTH*ch)] = 
                phases_array[ch];
      // sign-extend to OUTPUT_PHASE_WIDTH
      assign phases[(OUTPUT_PHASE_WIDTH*(ch+1)-1):(OUTPUT_PHASE_WIDTH*ch+CORDIC_PHASE_WIDTH)] =
              {{(OUTPUT_PHASE_WIDTH-CORDIC_PHASE_WIDTH){phases_array[ch][CORDIC_PHASE_WIDTH-1]}}};

      assign wraps[(CORDIC_WRAP_WIDTH*ch+CORDIC_WRAP_WIDTH-1):(CORDIC_WRAP_WIDTH*ch)] = 
                wraps_array[ch];
    end
    for (ch = 0; ch < NUM_DEBUG; ch = ch + 1) begin : gen_debug
      assign debug[(32*ch+31):(32*ch)] = debug_array[ch];
    end
  endgenerate

  reg signed [31:0] f0; // for channel 0 LO phase lock
  reg signed [31:0] f00; // for channel 0 LO phase lock
  reg signed [31:0] unwrapped_phase0;
  reg signed [31:0] integrator;
  localparam integer MAX = 2 ** (CORDIC_PHASE_WIDTH);  // 2 pi

  always @(posedge clk) begin
    if (rst) begin
      adc_sample_count <= 0;
      adc_data_reg <= 0;
      prev_adc_ready <= 0;
      f0 <= lo_dds_phase_inc[LO_PHASE_WIDTH-1:0];
      f00 <= lo_dds_phase_inc[LO_PHASE_WIDTH-1:0];
      lo_dds_phase_inc_reg[0] <= lo_dds_phase_inc[LO_PHASE_WIDTH-1:0];
      integrator <= 0;
    end else begin
      // init read
      if (adc_ready == 1 && prev_adc_ready == 0) begin
        unwrapped_phase0 <= phases_array[0] - MAX*wraps_array[0];
        adc_sample_count <= adc_sample_count + 1;
        adc_data_reg <= adc_data;
        if (signal_good[0] != 0) begin
          // THIS IS SLOW!!!
          if ((adc_sample_count & 1) == 0) begin
            // adjust LO phase inc based on phase error
            f0 <= f00 - (unwrapped_phase0 * kp[7:0]) - integrator;
            integrator <= integrator + (unwrapped_phase0 >>> ki[7:0]);
          end
        end else begin
          // signal not good, hold f0 constant
          f0 <= f0;
          integrator <= integrator;
        end
      end
      lo_dds_phase_inc_reg[0] <= f0;
      prev_adc_ready <= adc_ready;
    end
  end



  // for each channel, update DDS phase, I/Q, averages, run CORDIC
  generate
    for (ch = 0; ch < NUM_CHANNELS; ch = ch + 1) begin : gen_update_channels
      always @(posedge clk) begin
        if (rst) begin
          dds_phase_acc_sin[ch] <= 0;
          dds_phase_acc_cos[ch] <= COS_OFFSET;

          signal_good[ch] <= 0;
          signal_avg[ch] <= 0;
          if (ch > 0)
            lo_dds_phase_inc_reg[ch] <= lo_dds_phase_inc[((ch+1)*LO_PHASE_WIDTH-1):(ch*LO_PHASE_WIDTH)];

          mixerI[ch] <= 0;
          mixerQ[ch] <= 0;
        end else begin
          if (ch > 0)
            lo_dds_phase_inc_reg[ch] <= lo_dds_phase_inc[((ch+1)*LO_PHASE_WIDTH-1):(ch*LO_PHASE_WIDTH)];

          if (adc_ready == 1 && prev_adc_ready == 0) begin
            dds_phase_acc_sin[ch] <= dds_phase_acc_sin[ch] + lo_dds_phase_inc_reg[ch];
            dds_phase_acc_cos[ch] <= dds_phase_acc_cos[ch] + lo_dds_phase_inc_reg[ch];
            
            signal_avg[ch] <= magnitude_array[ch];
            signal_good[ch] <= (magnitude_array[ch] >= signal_good_threshold) ? 1'b1 : 1'b0;

            // mixer
            mixerI[ch] <= (in_signal * sin_lut[addr_cos[ch]]);
            mixerQ[ch] <= (in_signal * sin_lut[addr_sin[ch]]);
          end
        end
      end

      low_pass_n_order #(
          .ORDER(IQ_AVG_ORDER),
          .COEFF(IQ_AVG_COEFF),
          .SHIFT(IQ_AVG_SHIFT)
      ) lpf_i_inst (
          .clk(adc_ready),
          .reset(rst),
          .in(mixerI[ch]),
          .out(i_avg[ch])
      );

      low_pass_n_order #(
          .ORDER(IQ_AVG_ORDER),
          .COEFF(IQ_AVG_COEFF),
          .SHIFT(IQ_AVG_SHIFT)
      ) lpf_q_inst (
          .clk(adc_ready),
          .reset(rst),
          .in(mixerQ[ch]),
          .out(q_avg[ch])
      );

      cordic_atan2 #(
          .PHASE_WIDTH(CORDIC_PHASE_WIDTH),
          .WRAP_WIDTH(CORDIC_WRAP_WIDTH)
      ) cordic_atan2_inst (
          .clk(clk),
          .rst(rst),
          .x(i_avg[ch][31:16]),
          .y(q_avg[ch][31:16]),
          .phase(phases_array[ch]),
          .wraps(wraps_array[ch]),
          .magnitude(magnitude_array[ch]),
          .signal_good(signal_good[ch])
      );
    end
  endgenerate

  // XADC instantiation
  genvar k;
  generate
    for (k=0; k<NUM_XADC; k = k+1) begin : gen_xadc
      XADC #(  // see Xilinx UG480 for details, p. 22 and 44
          .INIT_40(16'h0403),  // no avg, continuous sampling, bipolar, single channel VP/VN
          .INIT_41(16'h3F0F),  // no sequencer, no alarms, no calibration
          .INIT_42(16'h0200),  // ADCCLK = clk/4.  26 ADCCLK per conversion
          .INIT_48(16'h0000),  // Sequencer mode
          .INIT_49(16'h0000),
          .INIT_4A(16'h0000),
          .INIT_4B(16'h0000),
          .INIT_4C(16'h0000),
          .INIT_4D(16'h0000),
          .INIT_4E(16'h0000),
          .INIT_4F(16'h0000),
          .SIM_MONITOR_FILE("voltages.txt")  // Analog Stimulus file for simulation
      ) adc_inst (
          .CONVST(1'b0),  // not used
          .CONVSTCLK(1'b0),  // not used
          .DADDR(7'h03),  // Address for channel selection (Vp/Vn), see UH480, p. 36
          .DCLK(clk),
          .DEN(eoc),
          .DWE(1'b0),
          .DI(16'h0000),
          .DRDY(adc_ready),
          .DO(adc_data),
          .RESET(rst),
          .EOC(eoc),
          .EOS(eos),
          .BUSY(busy),
          .VAUXN(16'h0000),
          .VAUXP(16'h0000),
          .JTAGBUSY(),  // not used
          .JTAGLOCKED(),  // not used
          .JTAGMODIFIED(),  // not used
          .VP(VP),
          .VN(VN)
      );
    end

    for (k=0; k<NUM_ADAQ4001; k = k+1) begin : gen_adaq4001
      spi_ADAQ4001 #(
      ) adc_inst (
          .clk(clk),
          .ready(adc_ready),
          .data(adc_data),
          .rst(rst)
      );
    end
  endgenerate
endmodule
