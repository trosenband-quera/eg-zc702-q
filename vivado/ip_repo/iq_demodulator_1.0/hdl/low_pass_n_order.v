// pipelined stack of N 1st order low-pass filters
// each stage is like an RC filter with time constant set by COEFF and SHIFT
// the 1/e time is (2^SHIFT)/COEFF clock cycles
// e.g. if clk is 1 MHz, COEFF=8, SHIFT=7, then time constant is 16 us 
// 
// thanks, chatGPT

`timescale 1 ns / 1 ps

module low_pass_n_order #(
    parameter integer ORDER = 3,
    parameter integer COEFF = 10,  // Coefficient for each stage
    parameter integer SHIFT = 7,   // Right shift amount for scaling
    parameter integer WIDTH = 32    // Width of the input and output signals
) (
    input  wire        clk,    // Clock signal
    input  wire        reset,  // Reset signal
    input  wire signed [WIDTH-1:0] in,     // Input signal
    output reg  signed [WIDTH-1:0] out     // Filtered output signal
);

  // Internal registers for intermediate stages
  reg signed [WIDTH-1:0] stage[0:ORDER-1];
  integer i;

  always @(posedge clk or posedge reset) begin
    if (reset) begin
      // Reset all stages
      for (i = 0; i < ORDER; i = i + 1) begin
        stage[i] <= 0;
      end
      out <= 0;
    end else begin
      // First stage takes input directly
      // NOTE: The result of a multiplication has a width equal to the 
      // sum of the widths of the two operands.
      stage[0] <= stage[0] + ((COEFF * (in - stage[0])) >>> SHIFT);
      for (i = 1; i < ORDER; i = i + 1) begin
        // Subsequent stages take output from previous stage
        stage[i] <= stage[i] + ((COEFF * (stage[i-1] - stage[i])) >>> SHIFT);
      end
      out <= stage[ORDER-1];
    end
  end

endmodule
