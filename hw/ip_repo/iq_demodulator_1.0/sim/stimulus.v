`timescale 1ns / 1ps

module ADC_TestBench_Sim(
    output wire adc_spi_cs_n,
    output wire adc_spi_clk,
    output wire adc_spi_mosi,
    input wire adc_spi_miso
);

reg clk;
reg rst;
// Output Data
wire [15 : 0] data_out;
wire data_ready_out;


parameter PERIOD = 20;
parameter data = 16'hABCD; // Example ADC data to be shifted out on MISO during SPI transaction

initial clk = 1'b0;
always #(PERIOD / 2.0)
  clk = ~clk;
  
initial rst = 1;


// MISO register to simulate ADC data shifting out during SPI transaction
reg [15:0] miso_reg;

// Signal to connect to the ADC's MISO input, updated on the negative edge of the SPI clock
// MSB, delayed by one SPI clock cycle to simulate real SPI behavior
reg miso_sig; 

initial
begin
  // Start :
  #(2*PERIOD);
  rst = 0;
  miso_reg = data;
  miso_sig = 1;
end

always @(posedge adc_spi_clk) begin
  miso_sig <= miso_reg[15];
    // Simulate ADC data shifting out on MISO during SPI transaction
    if (adc_spi_cs_n == 0) begin
      miso_reg <= {miso_reg[14:0], 1'b1}; // Shift in '1's for testing, MSB first
    end else begin
      miso_reg <= data; // Clear MISO register when not in transaction
    end
end

assign adc_spi_miso = miso_sig;


// Instantiate the ADC module, DUT
adc_adaq4001_spi_min adc (
  .clk(clk),
  .rst(rst),
 
  .adc_data(data_out),                         // [15:0] binary sample data
  .adc_ready(data_ready_out),                  // Data ready signal for the dynamic reconfiguration port.
  .spi_cs_n(adc_spi_cs_n),
  .spi_clk(adc_spi_clk),
  .spi_mosi(adc_spi_mosi),
  .spi_miso(adc_spi_miso)
);

endmodule
