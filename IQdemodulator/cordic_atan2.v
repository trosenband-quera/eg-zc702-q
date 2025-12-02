module cordic_atan2 (
    input wire clk,
    input wire rst,
    input wire signed [15:0] x,  // I component
    input wire signed [15:0] y,  // Q component
    output reg [15:0] phase      // Output phase (fixed-point)
);

// Parameters
localparam ITER = 16;
reg signed [15:0] x_reg [0:ITER];
reg signed [15:0] y_reg [0:ITER];
reg signed [15:0] z_reg [0:ITER];

// Arctangent lookup table (in radians, scaled to 16-bit)
reg signed [15:0] atan_table [0:ITER-1];
integer i;
initial begin
    for (i = 0; i < ITER; i = i + 1)
        atan_table[i] = $rtoi($atan(2.0**(-i)) * 32768 / 3.1415926535); // scale to [-32768, 32767]
end

// CORDIC rotation mode
always @(posedge clk) begin
    if (rst) begin
        phase <= 0;
    end else begin
        x_reg[0] <= x;
        y_reg[0] <= y;
        z_reg[0] <= 0;

        for (i = 0; i < ITER; i = i + 1) begin
            if (y_reg[i] > 0) begin
                x_reg[i+1] <= x_reg[i] + (y_reg[i] >>> i);
                y_reg[i+1] <= y_reg[i] - (x_reg[i] >>> i);
                z_reg[i+1] <= z_reg[i] + atan_table[i];
            end else begin
                x_reg[i+1] <= x_reg[i] - (y_reg[i] >>> i);
                y_reg[i+1] <= y_reg[i] + (x_reg[i] >>> i);
                z_reg[i+1] <= z_reg[i] - atan_table[i];
            end
        end

        phase <= z_reg[ITER];
    end
end

endmodule