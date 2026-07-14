"""
convert_formal.py
Generate Verilog from MyHDL blocks suitable for formal verification.

Outputs:
  formal/rtl/AXILiteMasterPort.v  (from gpgpu.py AXILiteMasterPort)
  formal/rtl/AXILiteSlavePort.v   (from AXILiteSlavePortFormal model below)
"""
import os
from myhdl import (
    block, Signal, intbv, always_seq, always_comb,
    ResetSignal
)
from gpgpu import AXILiteBus, AXILiteMasterPort
import re


def add_port_reg_init(path):
    """MyHDL's initial_values=True only initializes INTERNAL regs, not output
    PORT regs (the AXI master drives bus signals that become output regs).
    All AXI output regs reset to 0, so add '= 0' power-on init to any bare
    'reg' declaration lacking one. Idempotent (skips lines with '='); ignores
    memory arrays (name followed by an index)."""
    with open(path) as f:
        lines = f.readlines()
    pat = re.compile(r'^(\s*reg\s+(?:\[[^\]]+\]\s+)?)(\w+)\s*;\s*$')
    out = []
    for line in lines:
        m = pat.match(line.rstrip('\n'))
        if m:
            line = '%s%s = 0;\n' % (m.group(1), m.group(2))
        out.append(line)
    with open(path, 'w') as f:
        f.writelines(out)


# -----------------------------------------------------------------------
# Convertible AXI-Lite Slave (formal model)
#
# Mirrors the logic of gpgpu.py AXILiteSlavePort but replaces the
# non-synthesizable SlaveDevice Python calls with a Signal array memory.
# Includes the RVALID stability fix (hold until rready handshake).
# -----------------------------------------------------------------------
@block
def AXILiteSlavePortFormal(clk, reset, bus, N_WORDS=16):
    mem = [Signal(intbv(0)[32:]) for _ in range(N_WORDS)]

    awready_reg = Signal(bool(0))
    bvalid_reg  = Signal(bool(0))
    bresp_reg   = Signal(intbv(0)[2:])
    rvalid_reg  = Signal(bool(0))
    rdata_reg   = Signal(intbv(0)[32:])

    write_ready = Signal(bool(0))
    read_ready  = Signal(bool(0))

    LGWORDS = (N_WORDS - 1).bit_length()

    @always_seq(clk.posedge, reset=reset)
    def seq_logic():

        if reset:
            rvalid_reg.next = 0
        elif read_ready:
            rvalid_reg.next = 1
            rdata_reg.next = mem[int(bus.AR.araddr[LGWORDS+2:2])]
        elif rvalid_reg and bus.R.rready:
            rvalid_reg.next = 0

        if reset:
            awready_reg.next = 0
        else:
            awready_reg.next = (
                (not awready_reg)
                and bus.AW.awvalid
                and bus.W.wvalid
                and (not bvalid_reg or bus.B.bready)
            )

        if reset:
            bvalid_reg.next = 0
        elif write_ready:
            bvalid_reg.next = 1
            bresp_reg.next = 0
        elif bus.B.bready:
            bvalid_reg.next = 0

        if write_ready:
            mem[int(bus.AW.awaddr[LGWORDS+2:2])].next = bus.W.wdata

    @always_comb
    def comb_logic():
        bus.AW.awready.next = awready_reg
        bus.W.wready.next   = awready_reg
        write_ready.next    = awready_reg

        bus.B.bvalid.next = bvalid_reg
        bus.B.bresp.next  = bresp_reg

        bus.AR.arready.next = not rvalid_reg
        read_ready.next     = bus.AR.arvalid and (not rvalid_reg)

        bus.R.rvalid.next = rvalid_reg
        bus.R.rdata.next  = rdata_reg
        bus.R.rresp.next  = 0

        bus.AW.awprot.next = 0
        bus.AR.arprot.next = 0

    return seq_logic, comb_logic


# -----------------------------------------------------------------------
# Conversion functions
# -----------------------------------------------------------------------
def convert_master():
    clk = Signal(bool(0))
    reset = ResetSignal(0, active=1, isasync=False)
    bus = AXILiteBus()

    i_op = Signal(intbv(0)[3:])
    i_data = Signal(intbv(0)[32:])
    i_cpu_reset = Signal(bool(0))
    o_err = Signal(bool(0))
    i_stb = Signal(bool(0))
    w_misaligned = Signal(bool(0))
    r_flushing = Signal(bool(0))
    i_addr = Signal(intbv(0)[32:])

    inst = AXILiteMasterPort(
        clk, reset, bus,
        i_op=i_op, i_data=i_data,
        i_cpu_reset=i_cpu_reset, o_err=o_err,
        i_stb=i_stb, w_misaligned=w_misaligned,
        r_flushing=r_flushing, i_addr=i_addr,
    )

    outdir = os.path.join(os.path.dirname(__file__), "formal", "rtl")
    inst.convert(path=outdir, name="AXILiteMasterPort", initial_values=True)
    add_port_reg_init(os.path.join(outdir, "AXILiteMasterPort.v"))
    print("[OK] formal/rtl/AXILiteMasterPort.v")


def convert_slave():
    clk = Signal(bool(0))
    reset = ResetSignal(0, active=1, isasync=False)
    bus = AXILiteBus()

    inst = AXILiteSlavePortFormal(clk, reset, bus, N_WORDS=16)

    outdir = os.path.join(os.path.dirname(__file__), "formal", "rtl")
    inst.convert(path=outdir, name="AXILiteSlavePort", initial_values=True)
    add_port_reg_init(os.path.join(outdir, "AXILiteSlavePort.v"))
    print("[OK] formal/rtl/AXILiteSlavePort.v")


if __name__ == "__main__":
    convert_master()
    convert_slave()
