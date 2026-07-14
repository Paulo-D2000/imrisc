//
// Formal verification top-level: AXI-Lite Slave Port
//
// Instantiates the auto-generated AXILiteSlavePort DUT and ZipCPU's
// faxil_slave property checker. Master-side signals are free inputs;
// slave outputs are asserted against the AXI-Lite protocol.
//
`default_nettype none

module axilite_slave_top;

    reg clk;
    reg reset;

    // Master-side AXI signals (free inputs for formal)
    reg [31:0] awaddr;
    reg [2:0]  awprot;
    reg        awvalid;
    wire       awready;

    reg [31:0] wdata;
    reg [3:0]  wstrb;
    reg        wvalid;
    wire       wready;

    wire [1:0] bresp;
    wire       bvalid;
    reg        bready;

    reg [31:0] araddr;
    reg [2:0]  arprot;
    reg        arvalid;
    wire       arready;

    wire [31:0] rdata;
    wire [1:0]  rresp;
    wire        rvalid;
    reg         rready;

    // DUT
    AXILiteSlavePort dut (
        .clk(clk),
        .reset(reset),
        .bus_AW_awvalid(awvalid),
        .bus_AW_awready(awready),
        .bus_AW_awaddr(awaddr),
        .bus_AW_awprot(awprot),
        .bus_W_wvalid(wvalid),
        .bus_W_wready(wready),
        .bus_W_wdata(wdata),
        .bus_W_wstrb(wstrb),
        .bus_B_bvalid(bvalid),
        .bus_B_bready(bready),
        .bus_B_bresp(bresp),
        .bus_AR_arvalid(arvalid),
        .bus_AR_arready(arready),
        .bus_AR_araddr(araddr),
        .bus_AR_arprot(arprot),
        .bus_R_rvalid(rvalid),
        .bus_R_rready(rready),
        .bus_R_rdata(rdata),
        .bus_R_rresp(rresp)
    );

    // ZipCPU formal property checker (slave perspective)
    wire [3:0] f_awr_out, f_wr_out, f_rd_out;

    faxil_slave #(
        .C_AXI_DATA_WIDTH(32),
        .C_AXI_ADDR_WIDTH(32),
        .F_LGDEPTH(4),
        .F_AXI_MAXWAIT(3),
        .F_AXI_MAXDELAY(3),
        .F_AXI_MAXRSTALL(5)
    ) props (
        .i_clk(clk),
        .i_axi_reset_n(~reset),

        .i_axi_awvalid(awvalid),
        .i_axi_awready(awready),
        .i_axi_awaddr(awaddr),
        .i_axi_awprot(awprot),

        .i_axi_wvalid(wvalid),
        .i_axi_wready(wready),
        .i_axi_wdata(wdata),
        .i_axi_wstrb(wstrb),

        .i_axi_bvalid(bvalid),
        .i_axi_bready(bready),
        .i_axi_bresp(bresp),

        .i_axi_arvalid(arvalid),
        .i_axi_arready(arready),
        .i_axi_araddr(araddr),
        .i_axi_arprot(arprot),

        .i_axi_rvalid(rvalid),
        .i_axi_rready(rready),
        .i_axi_rdata(rdata),
        .i_axi_rresp(rresp),

        .f_axi_rd_outstanding(f_rd_out),
        .f_axi_wr_outstanding(f_wr_out),
        .f_axi_awr_outstanding(f_awr_out)
    );

    // DUT-specific invariants -- mirrors ZipCPU easyaxil (SIMPLE, no skidbuffer):
    // single-slot; AW and W are accepted together; outstanding tied to BVALID/RVALID.
    always @(*) begin
        assert(f_wr_out  == (bvalid ? 4'd1 : 4'd0));
        assert(f_awr_out == f_wr_out);
        assert(f_rd_out  == (rvalid ? 4'd1 : 4'd0));
    end

    // Clock generation
    initial clk = 0;
    always #5 clk = ~clk;

endmodule
