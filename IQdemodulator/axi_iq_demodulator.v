// Top-level AXI4-Lite wrapper for IQ Demodulator
module axi_iq_demodulator #(
    parameter C_S_AXI_DATA_WIDTH = 32,
    parameter C_S_AXI_ADDR_WIDTH = 4
)(
    input wire clk,
    input wire rst,

    // AXI4-Lite interface
    input wire [C_S_AXI_ADDR_WIDTH-1:0] s_axi_awaddr,
    input wire s_axi_awvalid,
    output wire s_axi_awready,
    input wire [C_S_AXI_DATA_WIDTH-1:0] s_axi_wdata,
    input wire [C_S_AXI_DATA_WIDTH/8-1:0] s_axi_wstrb,
    input wire s_axi_wvalid,
    output wire s_axi_wready,
    output wire [1:0] s_axi_bresp,
    output wire s_axi_bvalid,
    input wire s_axi_bready,
    input wire [C_S_AXI_ADDR_WIDTH-1:0] s_axi_araddr,
    input wire s_axi_arvalid,
    output wire s_axi_arready,
    output wire [C_S_AXI_DATA_WIDTH-1:0] s_axi_rdata,
    output wire [1:0] s_axi_rresp,
    output wire s_axi_rvalid,
    input wire s_axi_rready,

    // XADC input
    input wire [11:0] xadc_in
);

// Internal wires
wire [15:0] phase_100k, phase_110k, phase_120k;

// Instantiate IQ Demodulator core
iq_demodulator_core demod_core (
    .clk(clk),
    .rst(rst),
    .xadc_in(xadc_in),
    .phase_100k(phase_100k),
    .phase_110k(phase_110k),
    .phase_120k(phase_120k)
);

// AXI4-Lite register interface
reg [C_S_AXI_DATA_WIDTH-1:0] axi_rdata;
reg axi_rvalid;
reg axi_arready;

assign s_axi_rdata = axi_rdata;
assign s_axi_rvalid = axi_rvalid;
assign s_axi_rresp = 2'b00;
assign s_axi_arready = axi_arready;

assign s_axi_awready = 1'b1;
assign s_axi_wready  = 1'b1;
assign s_axi_bvalid  = s_axi_wvalid;
assign s_axi_bresp   = 2'b00;

always @(posedge clk) begin
    if (rst) begin
        axi_rvalid <= 0;
        axi_arready <= 0;
    end else begin
        axi_arready <= s_axi_arvalid;
        axi_rvalid <= s_axi_arvalid;

        case (s_axi_araddr[3:2])
            2'b00: axi_rdata <= {16'b0, phase_100k};
            2'b01: axi_rdata <= {16'b0, phase_110k};
            2'b10: axi_rdata <= {16'b0, phase_120k};
            default: axi_rdata <= 32'hDEADBEEF;
        endcase
    end
end

endmodule
