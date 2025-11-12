// Reference 1: The CORDIC Computing Technique, Jack Volder
// in 1959 PROCEEDINGS OF THE WESTERN JOINT COMPUTER CONFERENCE
module cordic_atan2 #(
    parameter integer PHASE_WIDTH = 16
) (
    input wire clk,
    input wire rst,
    input wire signed [PHASE_WIDTH-1:0] x,  // I component
    input wire signed [PHASE_WIDTH-1:0] y,  // Q component
    output reg [PHASE_WIDTH-1:0] phase      // Output phase (fixed-point)
);

// Parameters
localparam integer ITER = PHASE_WIDTH;
localparam integer MAX = 2**(ITER-1);  // 180 deg.
localparam real SCALE = MAX / 3.1415926535;

reg signed [PHASE_WIDTH:0] x_reg [0:ITER]; // extra bit for overflow
reg signed [PHASE_WIDTH:0] y_reg [0:ITER];
reg signed [PHASE_WIDTH-1:0] angle [0:ITER]; // accumulated angle

// Arctangent lookup table (in radians, scaled to 16-bit)
reg signed [PHASE_WIDTH-1:0] atan_table [0:ITER-1];
integer i;
initial begin
    for (i = 0; i < ITER; i = i + 1)
        atan_table[i] = ($atan(2.0**(-i)) * SCALE); // eq. 27
end

always @(posedge clk) begin
    if (rst) begin
        phase <= 0;
    end else begin   
        // first rotate by +/- 90 deg. -- eq. 32, 33
		if( y < 0 ) begin
			x_reg[0] <= -{y[PHASE_WIDTH-1], y}; // extend sign bit
            y_reg[0] <= {x[PHASE_WIDTH-1], x};
			angle[0] <= -(MAX/2);
		end else begin
			x_reg[0] <= {y[PHASE_WIDTH-1], y};
			y_reg[0] <= -{x[PHASE_WIDTH-1], x};
			angle[0] <= MAX/2;
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

        phase <= angle[ITER];
    end
end

endmodule
