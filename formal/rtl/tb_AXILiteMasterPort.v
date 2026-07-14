module tb_AXILiteMasterPort;

reg clk;
reg reset;
reg [2:0] i_op;
reg [31:0] i_data;
reg i_cpu_reset;
reg o_err;
reg i_stb;
reg w_misaligned;
wire r_flushing;
reg [31:0] i_addr;
wire bus_AW_awvalid;
reg bus_AW_awready;
wire [31:0] bus_AW_awaddr;
wire [2:0] bus_AW_awprot;
wire bus_W_wvalid;
reg bus_W_wready;
wire [31:0] bus_W_wdata;
wire [3:0] bus_W_wstrb;
reg bus_B_bvalid;
wire bus_B_bready;
reg [1:0] bus_B_bresp;
wire bus_AR_arvalid;
reg bus_AR_arready;
wire [31:0] bus_AR_araddr;
wire [2:0] bus_AR_arprot;
reg bus_R_rvalid;
wire bus_R_rready;
reg [31:0] bus_R_rdata;
reg [1:0] bus_R_rresp;

initial begin
    $from_myhdl(
        clk,
        reset,
        i_op,
        i_data,
        i_cpu_reset,
        o_err,
        i_stb,
        w_misaligned,
        i_addr,
        bus_AW_awready,
        bus_W_wready,
        bus_B_bvalid,
        bus_B_bresp,
        bus_AR_arready,
        bus_R_rvalid,
        bus_R_rdata,
        bus_R_rresp
    );
    $to_myhdl(
        r_flushing,
        bus_AW_awvalid,
        bus_AW_awaddr,
        bus_AW_awprot,
        bus_W_wvalid,
        bus_W_wdata,
        bus_W_wstrb,
        bus_B_bready,
        bus_AR_arvalid,
        bus_AR_araddr,
        bus_AR_arprot,
        bus_R_rready
    );
end

AXILiteMasterPort dut(
    clk,
    reset,
    i_op,
    i_data,
    i_cpu_reset,
    o_err,
    i_stb,
    w_misaligned,
    r_flushing,
    i_addr,
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
