"""Empirical test: does MyHDL initial_values=True apply to OUTPUT PORT regs
(not just internal regs)? The AXI master drives bus signals that become
output port regs; the slave uses internal regs. Test both cases."""
from myhdl import block, Signal, intbv, always_seq, ResetSignal

@block
def init_test(clk, reset, dout):
    r0 = Signal(intbv(5)[8:])   # INTERNAL reg -> expect power-on 5
    @always_seq(clk.posedge, reset=reset)
    def seq():
        r0.next = r0 + 1
        dout.next = r0 + 1       # dout is a PORT, assigned in always_seq -> OUTPUT REG
    return seq

clk   = Signal(bool(0))
reset = ResetSignal(0, active=1, isasync=False)
dout  = Signal(intbv(9)[8:])     # port, init 9

inst = init_test(clk, reset, dout)
inst.convert(name="init_test", path="formal/rtl", initial_values=True)
print("=== generated init_test.v ===")
print(open("formal/rtl/init_test.v").read())
