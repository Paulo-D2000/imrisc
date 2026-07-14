"""
Testbench for AXI Lite Master Port and Slave Port (MyHDL blocks)

Tests:
  1. Reset - all signals low
  2. Single read transaction
  3. Single write transaction
  4. Read back written value
  5. Out-of-range write -> SLVERR
  6. No strobe -> no request
  7. Read pre-filled word
"""

from myhdl import (
    block, Signal, intbv, always_seq, always_comb,
    always, ResetSignal, instance, delay, now, StopSimulation,
    traceSignals
)
from gpgpu import AXILiteBus, AXILiteMasterPort, AXILiteSlavePort, SlaveDevice


# ==============================
# Helper: pretty-print bus state
# ==============================
def bus_state(bus, label=""):
    lines = [label] if label else []
    lines.append(
        f"  AW: valid={int(bus.AW.awvalid)} ready={int(bus.AW.awready)} "
        f"addr=0x{int(bus.AW.awaddr):08x}"
    )
    lines.append(
        f"  W:  valid={int(bus.W.wvalid)} ready={int(bus.W.wready)} "
        f"data=0x{int(bus.W.wdata):08x} strb=0x{int(bus.W.wstrb):x}"
    )
    lines.append(
        f"  B:  valid={int(bus.B.bvalid)} ready={int(bus.B.bready)} "
        f"resp={int(bus.B.bresp)}"
    )
    lines.append(
        f"  AR: valid={int(bus.AR.arvalid)} ready={int(bus.AR.arready)} "
        f"addr=0x{int(bus.AR.araddr):08x}"
    )
    lines.append(
        f"  R:  valid={int(bus.R.rvalid)} ready={int(bus.R.rready)} "
        f"data=0x{int(bus.R.rdata):08x} resp={int(bus.R.rresp)}"
    )
    return "\n".join(lines)


# ==============================
# Testbench
# ==============================
@block
def run_testbench():
    clk = Signal(bool(0))
    reset = ResetSignal(0, active=1, isasync=False)
    bus = AXILiteBus()

    # Master-side signals
    i_op = Signal(intbv(0)[3:])          # bit[0]=1 for write, 0 for read
    i_data = Signal(intbv(0)[32:])
    i_cpu_reset = Signal(bool(0))
    o_err = Signal(bool(0))
    i_stb = Signal(bool(0))
    w_misaligned = Signal(bool(0))
    r_flushing = Signal(bool(0))
    i_addr = Signal(intbv(0)[32:])

    # Slave: 16 words starting at 0x1000
    ram = SlaveDevice(0x1000, 16, word_size=4, can_write=True)
    ram.data[0] = 0xDEADBEEF
    ram.data[1] = 0xCAFEBABE
    ram.data[5] = 0x12345678

    # Instantiate DUTs
    master_inst = AXILiteMasterPort(
        clk, reset, bus,
        i_op=i_op, i_data=i_data,
        i_cpu_reset=i_cpu_reset, o_err=o_err,
        i_stb=i_stb, w_misaligned=w_misaligned,
        r_flushing=r_flushing, i_addr=i_addr,
    )
    slave_inst = AXILiteSlavePort(clk, reset, bus, ram)

    # Clock generator: 10 time units period
    @always(delay(5))
    def clkgen():
        clk.next = not clk

    # Stimulus + checking
    @instance
    def stimulus():
        def idle():
            """Clear strobe and run one idle clock to settle."""
            i_stb.next = 0
            i_op.next = 0
            yield clk.posedge

        # ============================================================
        # TEST 1: Reset
        # ============================================================
        print("\n===== TEST 1: Reset =====")
        reset.next = 1
        i_stb.next = 0
        i_op.next = 0
        yield clk.posedge
        yield delay(1)
        print(bus_state(bus, "After reset:"))
        assert int(bus.AW.awvalid) == 0, "AW.awvalid not 0 after reset"
        assert int(bus.W.wvalid) == 0,  "W.wvalid not 0 after reset"
        assert int(bus.AR.arvalid) == 0, "AR.arvalid not 0 after reset"
        assert int(bus.B.bready) == 0,  "B.bready not 0 after reset"
        assert int(bus.R.rready) == 0,  "R.rready not 0 after reset"
        print("  [PASS] All master-side valid/ready signals are 0")

        reset.next = 0
        yield clk.posedge
        yield delay(1)
        print(bus_state(bus, "After reset deassert (idle):"))
        print("  [PASS] Reset deasserted\n")

        # ============================================================
        # TEST 2: Single Read
        # Timing: setup -> posedge#1 (ARVALID+RREADY) -> posedge#2 (RVALID+RDATA)
        # ============================================================
        print("===== TEST 2: Single Read Transaction =====")
        i_addr.next = 0x1000        # ram.data[0] = 0xDEADBEEF
        i_op.next = 0               # read
        i_stb.next = 1

        yield clk.posedge           # Posedge #1: master asserts ARVALID+RREADY
        yield delay(1)
        print(bus_state(bus, "After posedge #1 (address phase):"))
        assert int(bus.AR.arvalid) == 1, "AR.arvalid should be 1"
        assert int(bus.R.rready) == 1,  "R.rready should be 1"
        assert int(bus.AR.araddr) == 0x1000, "AR.araddr mismatch"
        print("  [PASS] Master issued read request")

        yield clk.posedge           # Posedge #2: slave sets RVALID+RDATA
        yield delay(1)
        print(bus_state(bus, "After posedge #2 (data phase):"))
        assert int(bus.R.rvalid) == 1, "R.rvalid should be 1"
        rdata = int(bus.R.rdata)
        assert rdata == 0xDEADBEEF, f"Expected 0xDEADBEEF, got 0x{rdata:08X}"
        print(f"  R.rdata = 0x{rdata:08X}")
        print("  [PASS] Read data correct\n")

        yield from idle()           # Posedge #3: handshake completes, return to idle

        # ============================================================
        # TEST 3: Single Write
        # Timing: setup -> posedge#1 (master: AWVALID+WVALID+BREADY, captures i_data)
        #                 posedge#2 (slave: awready_reg=1, bus.W.wdata now valid)
        #                 posedge#3 (slave: write_ready=1, does write, BVALID=1)
        # ============================================================
        print("===== TEST 3: Single Write Transaction =====")
        i_addr.next = 0x1004        # ram.data[1]
        i_data.next = 0x42
        i_op.next = 1               # write
        i_stb.next = 1

        yield clk.posedge           # Posedge #1: master asserts AWVALID+WVALID+BREADY
        yield delay(1)
        print(bus_state(bus, "After posedge #1 (address+data phase):"))
        assert int(bus.AW.awvalid) == 1, "AW.awvalid should be 1"
        assert int(bus.W.wvalid) == 1,  "W.wvalid should be 1"
        assert int(bus.B.bready) == 1,  "B.bready should be 1"
        assert int(bus.AW.awaddr) == 0x1004, "AW.awaddr mismatch"
        print("  [PASS] Master issued write request")

        yield clk.posedge           # Posedge #2: slave sets awready_reg, wdata now valid
        yield delay(1)
        print(bus_state(bus, "After posedge #2 (awready_reg set):"))
        assert int(bus.W.wdata) == 0x42, f"W.wdata should be 0x42, got 0x{int(bus.W.wdata):08X}"

        yield clk.posedge           # Posedge #3: slave processes write, asserts BVALID
        yield delay(1)
        print(bus_state(bus, "After posedge #3 (response phase):"))
        assert int(bus.B.bvalid) == 1, "B.bvalid should be 1"
        bresp = int(bus.B.bresp)
        assert bresp == 0, f"Expected BRESP=0 (OKAY), got {bresp}"
        print(f"  B.resp = {bresp} (OKAY)")
        assert ram.data[1] == 0x42, f"RAM[1] should be 0x42, got {ram.data[1]}"
        print("  [PASS] Write response OKAY, value stored in RAM\n")

        yield from idle()           # Posedge #4: handshake completes

        # ============================================================
        # TEST 4: Read back the written value
        # ============================================================
        print("===== TEST 4: Read back written value =====")
        i_addr.next = 0x1004
        i_op.next = 0
        i_stb.next = 1

        yield clk.posedge           # Posedge #1
        yield clk.posedge           # Posedge #2: RVALID+RDATA
        yield delay(1)
        print(bus_state(bus, "After posedge #2:"))
        rdata = int(bus.R.rdata)
        print(f"  R.rdata = 0x{rdata:08X}")
        assert rdata == 0x42, f"Expected 0x42, got 0x{rdata:08X}"
        print("  [PASS] Read back matches written value\n")

        yield from idle()

        # ============================================================
        # TEST 5: Out-of-range write -> SLVERR
        # ============================================================
        print("===== TEST 5: Out-of-range write -> SLVERR =====")
        i_addr.next = 0xFFFF        # outside slave range
        i_data.next = 0xBAD
        i_op.next = 1
        i_stb.next = 1

        yield clk.posedge           # Posedge #1
        yield delay(1)
        print(bus_state(bus, "After posedge #1:"))

        yield clk.posedge           # Posedge #2: slave sets awready_reg
        yield delay(1)

        yield clk.posedge           # Posedge #3: BVALID with SLVERR
        yield delay(1)
        print(bus_state(bus, "After posedge #3:"))
        assert int(bus.B.bvalid) == 1, "B.bvalid should be 1"
        bresp = int(bus.B.bresp)
        print(f"  B.resp = {bresp} (0=OKAY, 2=SLVERR)")
        assert bresp == 2, f"Expected BRESP=2 (SLVERR), got {bresp}"
        print("  [PASS] Out-of-range write returns SLVERR\n")

        yield from idle()

        # ============================================================
        # TEST 6: No strobe -> no request
        # ============================================================
        print("===== TEST 6: No strobe -> no request =====")
        i_addr.next = 0x1000
        i_op.next = 0
        i_stb.next = 0

        yield clk.posedge
        yield delay(1)
        print(bus_state(bus, "No strobe:"))
        assert int(bus.AR.arvalid) == 0, "AR.arvalid should be 0 with no strobe"
        assert int(bus.AW.awvalid) == 0, "AW.awvalid should be 0 with no strobe"
        print("  [PASS] No request issued without strobe\n")

        # ============================================================
        # TEST 7: Read pre-filled word (ram.data[5] = 0x12345678)
        # ============================================================
        print("===== TEST 7: Read pre-filled word =====")
        i_addr.next = 0x1014        # ram.data[5] = 0x12345678
        i_op.next = 0
        i_stb.next = 1

        yield clk.posedge           # Posedge #1
        yield clk.posedge           # Posedge #2: RVALID+RDATA
        yield delay(1)
        print(bus_state(bus, "After posedge #2:"))
        rdata = int(bus.R.rdata)
        print(f"  R.rdata = 0x{rdata:08X}")
        assert rdata == 0x12345678, f"Expected 0x12345678, got 0x{rdata:08X}"
        print("  [PASS] Read data matches pre-filled value\n")

        yield from idle()

        # ============================================================
        # Done
        # ============================================================
        print("=" * 50)
        print("ALL TESTS PASSED")
        print("=" * 50)
        raise StopSimulation()

    return clkgen, master_inst, slave_inst, stimulus


def main():
    print("=" * 50)
    print("AXI Lite Master/Slave Port Testbench")
    print("=" * 50)
    try:
        tb = run_testbench()
        traceSignals(tb).run_sim(quiet=1)
    except StopSimulation:
        pass


if __name__ == "__main__":
    main()
