// Reference 1: The CORDIC Computing Technique, Jack Volder
// in 1959 PROCEEDINGS OF THE WESTERN JOINT COMPUTER CONFERENCE

`timescale 1 ns / 1 ps

module cordic_atan2 #(
    parameter integer PHASE_WIDTH = 16,
    parameter integer WRAP_WIDTH  = 8
) (
    input wire clk,
    input wire rst,
    input wire signed [PHASE_WIDTH-1:0] x,  // I component
    input wire signed [PHASE_WIDTH-1:0] y,  // Q component
    output reg signed [PHASE_WIDTH-1:0] phase,  // Output phase (fixed-point)
    output reg signed [WRAP_WIDTH-1:0] wraps,  // Number of phase wraparounds
    output reg [PHASE_WIDTH:0] magnitude,  // Output magnitude (fixed-point with extra bit)
    input wire signal_good
);

  // Parameters
  localparam integer ITER = PHASE_WIDTH;
  localparam integer MAX = 2 ** (ITER - 1);  // 180 deg.
  localparam real SCALE = MAX / 3.1415926535;

  reg signed [PHASE_WIDTH+1:0] x_reg[0:ITER];  // extra bits for overflow
  reg signed [PHASE_WIDTH+1:0] y_reg[0:ITER];
  reg signed [PHASE_WIDTH-1:0] angle[0:ITER];  // accumulated angle

  // Arctangent lookup table (in radians, scaled to 16-bit)
  reg signed [PHASE_WIDTH-1:0] atan_table[0:ITER-1];
  integer i;
  initial begin
    for (i = 0; i < ITER; i = i + 1) atan_table[i] = ($atan(2.0 ** (-i)) * SCALE);  // eq. 27
  end

  always @(posedge clk) begin
    if (rst) begin
      phase <= 0;
      wraps <= 0;
      magnitude <= 0;
    end else begin
      // first rotate by +/- 90 deg. -- eq. 32, 33
      if (y < 0) begin
        x_reg[0] <= -{y[PHASE_WIDTH-1], y[PHASE_WIDTH-1], y};  // extend sign bit
        y_reg[0] <= {x[PHASE_WIDTH-1], x[PHASE_WIDTH-1], x};
        angle[0] <= -(MAX / 2);
      end else begin
        x_reg[0] <= {y[PHASE_WIDTH-1], y[PHASE_WIDTH-1], y};
        y_reg[0] <= -{x[PHASE_WIDTH-1], x[PHASE_WIDTH-1], x};
        angle[0] <= MAX / 2;
      end

      for (i = 0; i < ITER; i = i + 1) begin
        // incrementally rotate toward y=0 -- eq. 12, 13
        if (y_reg[i] > 0) begin
          x_reg[i+1] <= x_reg[i] + (y_reg[i] >>> i);
          y_reg[i+1] <= y_reg[i] - (x_reg[i] >>> i);
          angle[i+1] <= angle[i] + atan_table[i];
        end else begin
          x_reg[i+1] <= x_reg[i] - (y_reg[i] >>> i);
          y_reg[i+1] <= y_reg[i] + (x_reg[i] >>> i);
          angle[i+1] <= angle[i] - atan_table[i];
        end
      end

      // detect phase wraparound if signal is good
      //
      // sign of angle and phase must differ
      // and angle magnitude must be large: sign of angle and its previous bit differ
      // and phase magnitude must be large: sign of phase and its previous bit differ
      if (signal_good) begin
        if(angle[ITER][PHASE_WIDTH-1] != phase[PHASE_WIDTH-1]  &&
          angle[ITER][PHASE_WIDTH-2] != angle[ITER][PHASE_WIDTH-1] &&
          phase[PHASE_WIDTH-2] != phase[PHASE_WIDTH-1]) begin
          // phase wraparound occurred
          if (angle[ITER][PHASE_WIDTH-1] == 0) begin
            // angle is positive, phase is negative
            wraps <= wraps + 1;
          end else begin
            // angle is negative, phase is positive
            wraps <= wraps - 1;
          end
        end
      end else begin
        wraps <= 0;
      end

      phase <= angle[ITER];
      magnitude <= x_reg[ITER];
    end
  end

endmodule
