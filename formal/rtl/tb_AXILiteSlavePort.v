module tb_AXILiteSlavePort;

reg clk;
reg reset;
reg bus_AW_awvalid;
wire bus_AW_awready;
reg [31:0] bus_AW_awaddr;
wire [2:0] bus_AW_awprot;
reg bus_W_wvalid;
wire bus_W_wready;
reg [31:0] bus_W_wdata;
reg [3:0] bus_W_wstrb;
wire bus_B_bvalid;
reg bus_B_bready;
wire [1:0] bus_B_bresp;
reg bus_AR_arvalid;
wire bus_AR_arready;
reg [31:0] bus_AR_araddr;
wire [2:0] bus_AR_arprot;
wire bus_R_rvalid;
reg bus_R_rready;
wire [31:0] bus_R_rdata;
wire [1:0] bus_R_rresp;

initial begin
    $from_myhdl(
        clk,
        reset,
        bus_AW_awvalid,
        bus_AW_awaddr,
        bus_W_wvalid,
        bus_W_wdata,
        bus_W_wstrb,
        bus_B_bready,
        bus_AR_arvalid,
        bus_AR_araddr,
        bus_R_rready
    );
    $to_myhdl(
        bus_AW_awready,
        bus_AW_awprot,
        bus_W_wready,
        bus_B_bvalid,
        bus_B_bresp,
        bus_AR_arready,
        bus_AR_arprot,
        bus_R_rvalid,
        bus_R_rdata,
        bus_R_rresp
    );
end

AXILiteSlavePort dut(
    clk,
    reset,
    bus_AW_awvalid,
    bus_AW_awready,
    bus_AW_awaddr,
    bus_AW_awprot,
    bus_W_wvalid,
    bus_W_wready,
    bus_W_wdata,
    bus_W_wstrb,
    bus_B_bvalid,
    bus_B_bready,
    bus_B_bresp,
    bus_AR_arvalid,
    bus_AR_arready,
    bus_AR_araddr,
    bus_AR_arprot,
    bus_R_rvalid,
    bus_R_rready,
    bus_R_rdata,
    bus_R_rresp
);

endmodule
