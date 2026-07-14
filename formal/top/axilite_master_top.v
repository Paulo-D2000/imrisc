//
// Formal verification top-level: AXI-Lite Master Port
//
// Instantiates the auto-generated AXILiteMasterPort DUT and ZipCPU's
// faxil_master property checker. Slave-side signals are assumed;
// master outputs are asserted against the AXI-Lite protocol.
//
`default_nettype none

module axilite_master_top;

    reg clk;
    reg reset;

    // Master control inputs (free for formal)
    reg [2:0]  i_op;
    reg [31:0] i_data;
    reg        i_cpu_reset;
    reg        o_err;
    reg        i_stb;
    reg        w_misaligned;
    reg [31:0] i_addr;
    wire       r_flushing;

    // Bus signals
    wire        awvalid, awready;
    wire [31:0] awaddr;
    wire [2:0]  awprot;
    wire        wvalid, wready;
    wire [31:0] wdata;
    wire [3:0]  wstrb;
    wire        bvalid, bready;
    wire [1:0]  bresp;
    wire        arvalid, arready;
    wire [31:0] araddr;
    wire [2:0]  arprot;
    wire        rvalid, rready;
    wire [31:0] rdata;
    wire [1:0]  rresp;

    // DUT
    AXILiteMasterPort dut (
        .clk(clk),
        .reset(reset),
        .i_op(i_op),
        .i_data(i_data),
        .i_cpu_reset(i_cpu_reset),
        .o_err(o_err),
        .i_stb(i_stb),
        .w_misaligned(w_misaligned),
        .r_flushing(r_flushing),
        .i_addr(i_addr),
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

    // ZipCPU formal property checker (master perspective)
    wire [3:0] f_awr_out, f_wr_out, f_rd_out;

    faxil_master #(
        .C_AXI_DATA_WIDTH(32),
        .C_AXI_ADDR_WIDTH(32),
        .F_LGDEPTH(4),
        .F_AXI_MAXWAIT(3),
        .F_AXI_MAXDELAY(3),
        .F_OPT_ASSUME_RESET(1'b1)
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

    // Master outstanding invariants (mirrors ZipCPU axilops single-op master):
    // single transaction; outstanding==1 iff request accepted but response pending
    always @(*) begin
        if (!bready && !rready) begin
            assert(f_awr_out == 4'd0);
            assert(f_wr_out  == 4'd0);
            assert(f_rd_out  == 4'd0);
        end else if (bready) begin
            assert(f_awr_out == (awvalid ? 4'd0 : 4'd1));
            assert(f_wr_out  == (wvalid  ? 4'd0 : 4'd1));
            assert(f_rd_out  == 4'd0);
        end else begin
            assert(f_awr_out == 4'd0);
            assert(f_wr_out  == 4'd0);
            assert(f_rd_out  == (arvalid ? 4'd0 : 4'd1));
        end
    end

    // Constrain i_op to read (0) or write (1) only
    always @(*) assume(i_op <= 1);

    // Clock generation
    initial clk = 0;
    always #5 clk = ~clk;

endmodule
