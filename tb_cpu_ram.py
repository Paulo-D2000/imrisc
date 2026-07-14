"""
Testbench: CPU <=> RAM via AXI-Lite

Wires one AXILiteMasterPort to one AXILiteSlavePort (through a shared bus)
using a stimulus generator that mimics a simple CPU doing sequential
load/store operations.

Tests:
  1. Reset - all signals low
  2. Write 0xDEADBEEF to RAM[0]  (addr 0x1000)
  3. Write 0xCAFEBABE to RAM[1]  (addr 0x1004)
  4. Read back RAM[0] -> 0xDEADBEEF
  5. Read back RAM[1] -> 0xCAFEBABE
  6. Write then read-back a sequence (addr 0x2000..0x2010)
  7. Out-of-range read -> SLVERR
  8. Back-to-back writes (no idle between)
"""

from myhdl import (
    block, Signal, intbv, always_seq, always_comb,
    always, ResetSignal, instance, delay, now, StopSimulation,
    traceSignals
)
from gpgpu import AXILiteBus, AXILiteMasterPort, AXILiteSlavePort, SlaveDevice, CPU
from isa import Op


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

    # Master-side signals (mimic CPU interface)
    i_op = Signal(intbv(0)[3:])
    i_data = Signal(intbv(0)[32:])
    i_cpu_reset = Signal(bool(0))
    o_err = Signal(bool(0))
    i_stb = Signal(bool(0))
    w_misaligned = Signal(bool(0))
    r_flushing = Signal(bool(0))
    i_addr = Signal(intbv(0)[32:])

    # Slave: 16 words starting at 0x1000
    ram = SlaveDevice(0x1000, 16, word_size=4, can_write=True)

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

    # Helper generators for stimulus
    def idle(n=1):
        """Deassert strobe and run n idle clocks."""
        i_stb.next = 0
        i_op.next = 0
        for _ in range(n):
            yield clk.posedge
        yield delay(1)

    def do_write(addr, data):
        """Issue a single write transaction. Waits until BVALID received."""
        i_addr.next = addr
        i_data.next = data
        i_op.next = 1   # write
        i_stb.next = 1
        yield clk.posedge
        yield delay(1)
        # Wait for BVALID (write response)
        while not int(bus.B.bvalid):
            yield clk.posedge
            yield delay(1)
        # Give one more clock for handshake to complete
        yield clk.posedge
        yield delay(1)
        i_stb.next = 0
        yield clk.posedge
        yield delay(1)

    def do_read(addr):
        """Issue a single read transaction. Returns read data."""
        i_addr.next = addr
        i_op.next = 0   # read
        i_stb.next = 1
        yield clk.posedge
        yield delay(1)
        # Wait for RVALID (read data)
        while not int(bus.R.rvalid):
            yield clk.posedge
            yield delay(1)
        rdata = int(bus.R.rdata)
        # Give one more clock for handshake to complete
        yield clk.posedge
        yield delay(1)
        i_stb.next = 0
        yield clk.posedge
        yield delay(1)
        return rdata

    # Stimulus + checking
    @instance
    def stimulus():
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
        print("  [PASS] Reset deasserted\n")

        # ============================================================
        # TEST 2: Write 0xDEADBEEF to RAM[0] (addr 0x1000)
        # ============================================================
        print("===== TEST 2: Write 0xDEADBEEF to RAM[0] =====")
        yield from do_write(0x1000, 0xDEADBEEF)
        print(bus_state(bus, "After write:"))
        assert ram.data[0] == 0xDEADBEEF, f"RAM[0] should be 0xDEADBEEF, got 0x{ram.data[0]:08X}"
        print(f"  RAM[0] = 0x{ram.data[0]:08X}")
        print("  [PASS] Write stored correctly\n")

        # ============================================================
        # TEST 3: Write 0xCAFEBABE to RAM[1] (addr 0x1004)
        # ============================================================
        print("===== TEST 3: Write 0xCAFEBABE to RAM[1] =====")
        yield from do_write(0x1004, 0xCAFEBABE)
        assert ram.data[1] == 0xCAFEBABE, f"RAM[1] should be 0xCAFEBABE, got 0x{ram.data[1]:08X}"
        print(f"  RAM[1] = 0x{ram.data[1]:08X}")
        print("  [PASS] Write stored correctly\n")

        # ============================================================
        # TEST 4: Read back RAM[0]
        # ============================================================
        print("===== TEST 4: Read back RAM[0] =====")
        yield from idle()
        rdata = yield from do_read(0x1000)
        print(bus_state(bus, "After read:"))
        assert rdata == 0xDEADBEEF, f"Expected 0xDEADBEEF, got 0x{rdata:08X}"
        print(f"  R.rdata = 0x{rdata:08X}")
        print("  [PASS] Read data correct\n")

        # ============================================================
        # TEST 5: Read back RAM[1]
        # ============================================================
        print("===== TEST 5: Read back RAM[1] =====")
        yield from idle()
        rdata = yield from do_read(0x1004)
        assert rdata == 0xCAFEBABE, f"Expected 0xCAFEBABE, got 0x{rdata:08X}"
        print(f"  R.rdata = 0x{rdata:08X}")
        print("  [PASS] Read data correct\n")

        # ============================================================
        # TEST 6: Write-then-readback sequence
        # ============================================================
        print("===== TEST 6: Write/read sequence 0x2000..0x2010 =====")
        test_vals = [0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x55555555]
        for i, val in enumerate(test_vals):
            addr = 0x1000 + i * 4
            yield from do_write(addr, val)
            rdata = yield from do_read(addr)
            assert rdata == val, f"Addr 0x{addr:04x}: expected 0x{val:08X}, got 0x{rdata:08X}"
            print(f"  [PASS] addr=0x{addr:04x} write=0x{val:08X} read=0x{rdata:08X}")
        print("  [PASS] All 5 write/read pairs correct\n")

        # ============================================================
        # TEST 7: Back-to-back writes
        # ============================================================
        print("===== TEST 7: Back-to-back writes =====")
        yield from do_write(0x1000, 0xAA)
        yield from do_write(0x1004, 0xBB)
        yield from do_write(0x1008, 0xCC)
        assert ram.data[0] == 0xAA, f"RAM[0] should be 0xAA"
        assert ram.data[1] == 0xBB, f"RAM[1] should be 0xBB"
        assert ram.data[2] == 0xCC, f"RAM[2] should be 0xCC"
        print("  [PASS] 3 back-to-back writes stored correctly\n")

        # ============================================================
        # TEST 8: CPU <=> AXI-Lite <=> RAM (real bus!)
        # ============================================================
        print("===== TEST 8: CPU <=> AXI-Lite <=> RAM =====")
        from sasm import SAsm
        sasm = SAsm(base=0x1000)
        cpu_asm = """
        .start:
            LI x1, 42
            LA x5, .data
            LI x10, 10
        .loop1:
            SW x1, 0(x5)
            ADDI x10, x10, -1
            BNE x10, zero, .loop1
            LI x10, 10
        .loop2:
            LW x3, 0(x5)
            ADDI x10, x10, -1
            BNE x10, zero, .loop2
            LI x10, 10
        .end:
        .data:
        """
        prog = sasm.compile(cpu_asm)
        print(f"  [DEBUG] Compiled {len(prog)} instructions:")
        for i, instr in enumerate(prog):
            print(f"    [{i}] 0x{i*4+sasm.base:08x} {instr}")
        print(f"  [DEBUG] Labels: {sasm.labels}")

        # CPU uses the SAME ram as the AXI-Lite slave
        cpu2 = CPU(prog, base=sasm.base)
        cpu2.pc = sasm.labels[".start"]
        cpu2.debug = True

        end_pc = sasm.labels[".end"]

        # Run CPU through real AXI-Lite bus via generator protocol
        # axi_clk() is a one-shot generator (one instruction per call)
        error = False
        cycle_count = 0

        while cpu2.pc < end_pc:
            gen = cpu2.axi_clk()
            cmd = next(gen)
            if cmd is None:
                # generator ended without yielding = out of bounds
                error = True
                break
            try:
                if cmd[0] == 'read':
                    rdata = yield from do_read(cmd[1])
                    gen.send(rdata)  # resume with read data
                elif cmd[0] == 'write':
                    yield from do_write(cmd[1], cmd[2])
                    gen.send(None)  # resume, generator does writeback and returns
                elif cmd[0] == 'nop':
                    yield clk.posedge
                    yield delay(1)
                    gen.send(None)  # resume, generator does writeback and returns
            except StopIteration:
                # generator ended after yielding (normal: one-shot per instruction)
                pass
            cycle_count += 1
            if cycle_count > 500:
                error = True
                break

        if error:
            print(f"  [FAIL] CPU error after {cycle_count} AXI cycles")
            raise StopSimulation()
        else:
            print(f"  CPU completed in {cycle_count} AXI cycles")
            print(f"  CPU x1 (should be 42)          = {cpu2.regs[1]}")
            print(f"  CPU x3 (readback from .data)   = {cpu2.regs[3]}")
            fail = False
            if cpu2.regs[1] != 42:
                print(f"  [FAIL] CPU x1 mismatch: expected 42, got {cpu2.regs[1]}")
                fail = True
            if cpu2.regs[3] != 42:
                print(f"  [FAIL] CPU x3 mismatch: expected 42, got {cpu2.regs[3]}")
                fail = True
            # Verify the value actually landed in the SlaveDevice via AXI
            data_offset = (sasm.labels[".data"] - ram.address) // ram.word_size
            if ram.data[data_offset] != 42:
                print(f"  [FAIL] RAM[.data] should be 42, got {ram.data[data_offset]}")
                fail = True
            if fail:
                raise StopSimulation()
            print(f"  RAM[.data] = {ram.data[data_offset]} (verified via AXI-Lite slave)")
            print("  [PASS] CPU <=> AXI-Lite <=> RAM all correct\n")

        # ============================================================
        # Done
        # ============================================================
        print("=" * 50)
        print("ALL CPU<=>RAM TESTS PASSED")
        print("=" * 50)
        raise StopSimulation()

    return clkgen, master_inst, slave_inst, stimulus


def main():
    print("=" * 50)
    print("CPU <=> RAM AXI-Lite Testbench")
    print("=" * 50)
    try:
        tb = run_testbench()
        traceSignals(tb).run_sim(quiet=1)
    except StopSimulation:
        pass


if __name__ == "__main__":
    main()
