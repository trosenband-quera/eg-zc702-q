//---------------------------------------------------------------------------------
// Company:  QuEra Computing
// Author:   Till Rosenband
// Created:  2026-02
//
// adc_adaq4001_spi_min.v
// Wrapper module to be used in the IQ Demodulator design.
// Minimal number of signals are exposed to the top-level design.
//
// For timing, follow "Figure 53. Register Write Timing Diagram" in the ADAQ4001 datasheet, rev B.
//---------------------------------------------------------------------------------

`timescale 1ns / 1ps
module adc_adaq4001_spi_min #(
    parameter real    C_CLOCK_FREQ_HZ = 50_000_000,  // Clock frequency in Hz
    parameter integer C_ADC_BIT_WIDTH = 16           // ADC resolution in bits
) (  // Inputs and outputs for the wrapper
    input wire clk,
    input wire rst,
    output wire adc_ready, // Indicates when adc_data is valid and ready to be read
    output wire [C_ADC_BIT_WIDTH-1:0] adc_data, // ADC data output, valid when adc_ready is high
    // SPI interface signals -- external connections to the ADAQ4001 ADC
    output wire spi_clk,
    output wire spi_mosi,  // change on falling edge, valid on the rising edge of spi_clk
    input wire spi_miso,  // valid on the falling edge of spi_clk
    output wire spi_cs_n
);
  // Number of clock cycles for TCONV=320 ns (conversion time) - adjust as needed
  localparam integer TCONV_CYCLES = 20;

  // Number of clock cycles for TSCNVSCK=6 ns (time from CS deassertion to first SCK) - adjust as needed
  // Note: For clk < 166 MHz, the state machine automatically meets this timing requirement.
  localparam integer TSCNVSCK_CYCLES = 1;

  reg [1:0] state; // State variable for SPI transaction control
  reg [C_ADC_BIT_WIDTH-1:0] adc_data_reg; // Shift register for incoming ADC data
  reg adc_ready_reg; // Register to indicate when ADC data is ready
  reg [7:0] clock_counter; // Counter for timing control
  reg [C_ADC_BIT_WIDTH-1:0] output_reg;  // Shift register for output data

  assign adc_data = adc_data_reg;
  assign adc_ready = adc_ready_reg;
  assign spi_cs_n = (state == 2'h0) ? 1 : 0;  // pull high to start conversion, pull low during SPI transaction
  assign spi_clk = (state == 2'h2) ? clk : 0;  // Generate SPI clock during transaction
  assign spi_mosi = (state > 2'h0) ? output_reg[C_ADC_BIT_WIDTH-1] : 0;  // Shift out command/data during transaction, MSB first

  always @(posedge clk) begin
    if (rst) begin
      adc_ready_reg <= 0;
      state <= 2'h0;
      clock_counter <= 0;
    end else begin
      case (state)
        2'h0: begin
          // Initiate conversion by asserting CS and waiting for TCONV_CYCLES
          adc_ready_reg <= 0;
          if (clock_counter < TCONV_CYCLES) begin
            clock_counter <= clock_counter + 1;
          end else begin
            clock_counter <= 0;
            state <= 2'h1;  // Move to SPI transaction state
          end
        end
        2'h1: begin
          state <= 2'h2;
        end
        2'h2: begin // SPI transaction: shift out command and read data, run SPI clock
          if (clock_counter < C_ADC_BIT_WIDTH-1) begin
            clock_counter <= clock_counter + 1;  // Shift out command and read data bits
          end else begin
            clock_counter <= 0;
            adc_ready_reg <= 1;  // Indicate that ADC data is ready after shifting out all bits
            state <= 2'h0;  // Start next conversion
          end
        end
        default: begin
          state <= 2'h0; // Reset to initial state in case of undefined state
        end
      endcase
    end
  end


  always @(negedge clk) begin
    if (rst) begin
      adc_data_reg <= 0;
      output_reg <= 0;
    end else begin
      case (state)
      2'h0: begin
        output_reg <= 16'h1400; // default command to read ADC data and preserve default settings (see ADAQ4001 datasheet, rev B, Table 14 & Figure 53)
      end
      2'h2: begin
        // SPI transaction: shift out command and read data
        output_reg   <= {output_reg[C_ADC_BIT_WIDTH-2:0], 1'b0};
        adc_data_reg <= {adc_data_reg[C_ADC_BIT_WIDTH-2:0], spi_miso};  // Shift in data from MISO
      end
      default: begin
        output_reg <= output_reg; // Hold current value in other states
        adc_data_reg <= adc_data_reg; // Hold current value in other states
      end
      endcase
    end
  end
endmodule

