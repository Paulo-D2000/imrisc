from dataclasses import dataclass
from isa import *
from sasm import SAsm
import random 
from myhdl import block, Signal, intbv, always_seq, always_comb, ResetSignal

# -----------------------------
# AXI Channel Base Class
# -----------------------------
class AXIChannel:
    def handshake(self, valid_name: str, ready_name: str) -> bool:
        # Return True if both valid and ready are high
        return getattr(self, valid_name) and getattr(self, ready_name)

# -----------------------------
# AXI Lite Channels
# -----------------------------
class AXILiteAW(AXIChannel):
    def __init__(self):
        self.awvalid = Signal(bool(0))
        self.awready = Signal(bool(0))
        self.awaddr  = Signal(intbv(0)[32:])
        self.awprot  = Signal(intbv(0)[3:])

class AXILiteW(AXIChannel):
    def __init__(self):
        self.wvalid = Signal(bool(0))
        self.wready = Signal(bool(0))
        self.wdata  = Signal(intbv(0)[32:])
        self.wstrb  = Signal(intbv(0)[4:])

class AXILiteB(AXIChannel):
    def __init__(self):
        self.bvalid = Signal(bool(0))
        self.bready = Signal(bool(0))
        self.bresp  = Signal(intbv(0)[2:])

class AXILiteAR(AXIChannel):
    def __init__(self):
        self.arvalid = Signal(bool(0))
        self.arready = Signal(bool(0))
        self.araddr  = Signal(intbv(0)[32:])
        self.arprot  = Signal(intbv(0)[3:])

class AXILiteR(AXIChannel):
    def __init__(self):
        self.rvalid = Signal(bool(0))
        self.rready = Signal(bool(0))
        self.rdata  = Signal(intbv(0)[32:])
        self.rresp  = Signal(intbv(0)[2:])

class AXILiteBus(AXIChannel):
    def __init__(self):
        self.AW = AXILiteAW()
        self.W  = AXILiteW()
        self.B  = AXILiteB()
        self.AR = AXILiteAR()
        self.R  = AXILiteR()

# -----------------------------
# SlaveDevice Model
# -----------------------------
class SlaveDevice:
    def __init__(self, address:int , n_words: int, word_size: int = 4, can_write: bool = False):
        self.data = [0]*n_words
        self.n_words = n_words
        self.address = address
        self.word_size = word_size
        self.can_write = can_write

    def get_offset(self, addr):
        if self.address <= addr < self.address + self.word_size * self.n_words:
            offset = (addr - self.address)//self.word_size
            return offset
        return None

    def read(self, addr):
        offset = self.get_offset(addr)
        if offset is None:
            return None
        return self.data[offset]
    
    def write(self, addr, data):
        offset = self.get_offset(addr)
        if offset is None:
            return None
        self.data[offset] = data
        return self.data[offset]
    
@dataclass
class MemoryReq:
    addr: int
    data: int = 0
    we: bool = False

@dataclass
class Request:
    valid: bool = False
    addr: int = 0
    we: bool = False
    data: int = 0

# -----------------------------
# MasterDevice Model
# -----------------------------
class MasterDevice:
    def __init__(self):
        self.reqs: list(MemoryReq) = []

    def write(self, addr, data):
        # TODO
        # push req?
        # wait on result ? (would need some sort of ''stall'')
        raise NotImplementedError("Write Not implemented")
    
    def clk(self):
        # Overrided by chidl device
        raise NotImplementedError("Clock Not Implemented")
    
    def read(self, addr):
        # TODO
        # push req ?
        # wait on result ? (would need some sort of ''stall'')
        raise NotImplementedError("Read Not implemented")

# -----------------------------
# AXI Lite Master Port (MyHDL)
# -----------------------------
@block
def AXILiteMasterPort(clk, reset, bus: AXILiteBus, i_op = Signal(intbv(0)[3:]), i_data = Signal(intbv(0)[32:]), i_cpu_reset = Signal(bool(0)), o_err = Signal(bool(0)), i_stb = Signal(bool(0)), w_misaligned = Signal(bool(0)), r_flushing = Signal(bool(0)), i_addr = Signal(intbv(0)[32:])):
    # -----------------------------
    # Internal Wires/Regs
    # -----------------------------

    # -----------------------------
    # Synchronous logic: all on rising edge
    # -----------------------------
    @always_seq(clk.posedge, reset=reset)
    def seq_logic():
        # Signaling Logic
        if(reset): # Reset
            bus.AW.awvalid.next = 0
            bus.W.wvalid.next = 0
            bus.AR.arvalid.next = 0
            bus.B.bready.next = 0
            bus.R.rready.next = 0
            r_flushing.next = 0
            bus.W.wstrb.next = 0
            bus.W.wdata.next = 0
            bus.AW.awaddr.next = 0
        elif(bus.B.bready or bus.R.rready): # Something is outstanding
            if(i_cpu_reset):
                r_flushing.next = 1

            if(bus.AW.awready):
                bus.AW.awvalid.next = 0
            if(bus.W.wready):
                bus.W.wvalid.next = 0
                bus.W.wstrb.next = 0
            if(bus.AR.arready):
                bus.AR.arvalid.next = 0

            if(bus.B.bvalid or bus.R.rvalid):
                r_flushing.next = 0
                bus.B.bready.next = 0
                bus.R.rready.next = 0
            
        else: # New Memory operation
            r_flushing.next = 0

            # Latch address for this transaction
            bus.AW.awaddr.next = i_addr

            # Init req
            bus.AW.awvalid.next = i_op[0] # Write req
            bus.W.wvalid.next   = i_op[0] # Write req
            bus.AR.arvalid.next = not i_op[0] # Read req

            # Set B/R ready
            bus.B.bready.next = i_op[0]
            bus.R.rready.next = not i_op[0]

            if(i_cpu_reset or o_err or not i_stb or w_misaligned):
                bus.AW.awvalid.next = 0
                bus.W.wvalid.next = 0
                bus.AR.arvalid.next = 0

                bus.B.bready.next = 0
                bus.R.rready.next = 0
                bus.W.wstrb.next = 0
            else:
                if(i_op[0]):
                    bus.W.wdata.next = i_data
                    bus.W.wstrb.next = 0xF
    
    # -----------------------------
    # Combinational assignments for Ready/valid propagation
    # -----------------------------
    @always_comb
    def comb_logic():
        bus.AR.araddr.next = bus.AW.awaddr

        bus.AW.awprot.next = 0
        bus.AR.arprot.next = 0

    return seq_logic, comb_logic

# -----------------------------
# AXI Lite Slave Port (MyHDL)
# -----------------------------
@block
def AXILiteSlavePort(clk, reset, bus: AXILiteBus, device: SlaveDevice):
    # -----------------------------
    # Internal Wires/Regs
    # -----------------------------

    # AW Chanel Regs
    awready_reg = Signal(bool(0))

    # Write done Signal
    write_ready = Signal(bool(0))

    # B Channel Regs
    bvalid_reg  = Signal(bool(0))
    bresp_reg   = Signal(intbv(0)[2:])

    # AR Channel Regs
    arready_reg = Signal(bool(0))

    # Read done Signal
    read_ready = Signal(bool(0))

    # R channel Regs
    rvalid_reg  = Signal(bool(0))
    rdata_reg   = Signal(intbv(0)[32:])

    # -----------------------------
    # Synchronous logic: all on rising edge
    # -----------------------------
    @always_seq(clk.posedge, reset=reset)
    def seq_logic():
    
        # -----------------------------
        # Read Data Channel (R)
        # -----------------------------
        if(reset):
            rvalid_reg.next = 0
        elif read_ready:
            rdata_reg.next = device.read(int(bus.AR.araddr))
            rvalid_reg.next = 1
        elif rvalid_reg and bus.R.rready:
            rvalid_reg.next = 0

        # -----------------------------
        # Write Signaling
        # -----------------------------
        if(reset):
            awready_reg.next = 0
        else:
            awready_reg.next = not awready_reg and (bus.AW.awvalid and bus.W.wvalid) and (not bus.B.bvalid or bus.B.bready)

        # -----------------------------
        # Write Response Channel (B)
        # -----------------------------
        if reset:
            bvalid_reg.next = 0
            # BVALID = 0, BRESP doesn't matter!
        elif write_ready:
            result = device.write(int(bus.AW.awaddr), int(bus.W.wdata))
            bvalid_reg.next = 1
            bresp_reg.next  = 0 if result is not None else 2  # OKAY / SLVERR
        elif bus.B.bready:
            bvalid_reg.next = 0
            # BVALID = 0, BRESP doesn't matter!

    # -----------------------------
    # Combinational assignments for Ready/valid propagation
    # -----------------------------
    @always_comb
    def comb_logic():
        # Write Address Assign
        bus.AW.awready.next = awready_reg

        # Write Signaling
        bus.W.wready.next = awready_reg
        write_ready.next  = awready_reg

        # Write Response Assign
        bus.B.bvalid.next = bvalid_reg
        bus.B.bresp.next  = bresp_reg

        # Read Address Assign
        bus.AR.arready.next = not bus.R.rvalid
        
        # Read signaling
        read_ready.next = bus.AR.arvalid and bus.AR.arready

        # Read Data Assign
        bus.R.rvalid.next = rvalid_reg
        bus.R.rdata.next  = rdata_reg
        bus.R.rresp.next  = 0  # Always OKAY for reads


    return seq_logic, comb_logic

# -----------------------------
# AXI Lite Arbiter (MyHDL)
# -----------------------------
# Connects N master ports (each with its own AXILiteBus) to a single
# shared slave-side AXILiteBus using round-robin arbitration.
#
# Protocol:
#   - A master requests by asserting AWVALID (write) or ARVALID (read) on its bus.
#   - The arbiter selects one master per clock cycle (round-robin).
#   - The selected master's address/data channels are forwarded to the shared bus.
#   - The shared bus's response (BVALID or RVALID) is routed back to the
#     winning master's bus.
#   - Other masters see AWREADY=0 / ARREADY=0 (stalled).
#
# Timing:
#   - Cycle 0: master asserts ARVALID/AWVALID on its port
#   - Cycle 1: arbiter selects this port, forwards to shared bus
#   - Cycle 2+: slave responds via shared bus, arbiter routes back
# -----------------------------
@block
def AXILiteArbiter(clk, reset, master_buses, shared_bus, n_ports):
    """
    Args:
        clk, reset: clock and reset
        master_buses: list of AXILiteBus (one per master port)
        shared_bus: AXILiteBus connected to the slave
        n_ports: number of master ports
    """
    # Grant register: which master currently has access (-1 = none)
    last_gnt = Signal(intbv(-1, min=-1, max=n_ports))

    @always_seq(clk.posedge, reset=reset)
    def arb_logic():
        if reset:
            last_gnt.next = -1
        else:
            # Check if current grant is still active (transaction in flight)
            gnt = int(last_gnt)
            if gnt >= 0:
                mbus = master_buses[gnt]
                # Transaction still in flight if we have outstanding valids
                # or if we're waiting for response
                if (int(mbus.AW.awvalid) and not int(shared_bus.AW.awready)):
                    pass  # keep current grant
                elif (int(mbus.AR.arvalid) and not int(shared_bus.AR.arready)):
                    pass  # keep current grant
                elif (int(shared_bus.B.bvalid) and not int(mbus.B.bready)):
                    pass  # waiting for master to accept B response
                elif (int(shared_bus.R.rvalid) and not int(mbus.R.rready)):
                    pass  # waiting for master to accept R response
                else:
                    # Current transaction finished, try to find new request
                    found = False
                    for offset in range(1, n_ports + 1):
                        idx = (gnt + offset) % n_ports
                        mb = master_buses[idx]
                        if int(mb.AW.awvalid) or int(mb.AR.arvalid):
                            last_gnt.next = idx
                            found = True
                            break
                    if not found:
                        last_gnt.next = -1
            else:
                # No current grant — scan round-robin from last+1
                for offset in range(1, n_ports + 1):
                    idx = (0 + offset - 1) % n_ports
                    mb = master_buses[idx]
                    if int(mb.AW.awvalid) or int(mb.AR.arvalid):
                        last_gnt.next = idx
                        break

    @always_comb
    def arb_route():
        gnt = int(last_gnt)
        if gnt < 0:
            gnt = -1

        # Default: stall all masters, deassert shared bus
        for i in range(n_ports):
            master_buses[i].AW.awready.next = 0
            master_buses[i].W.wready.next = 0
            master_buses[i].AR.arready.next = 0
            master_buses[i].B.bvalid.next = 0
            master_buses[i].B.bresp.next = 0
            master_buses[i].R.rvalid.next = 0
            master_buses[i].R.rdata.next = 0
            master_buses[i].R.rresp.next = 0

        shared_bus.AW.awvalid.next = 0
        shared_bus.AW.awaddr.next = 0
        shared_bus.W.wvalid.next = 0
        shared_bus.W.wdata.next = 0
        shared_bus.W.wstrb.next = 0
        shared_bus.B.bready.next = 0
        shared_bus.AR.arvalid.next = 0
        shared_bus.AR.araddr.next = 0
        shared_bus.R.rready.next = 0

        if gnt >= 0:
            mb = master_buses[gnt]

            # Forward master -> shared bus (address/write channels)
            shared_bus.AW.awvalid.next = mb.AW.awvalid
            shared_bus.AW.awaddr.next = mb.AW.awaddr
            shared_bus.W.wvalid.next = mb.W.wvalid
            shared_bus.W.wdata.next = mb.W.wdata
            shared_bus.W.wstrb.next = mb.W.wstrb
            shared_bus.B.bready.next = mb.B.bready

            shared_bus.AR.arvalid.next = mb.AR.arvalid
            shared_bus.AR.araddr.next = mb.AR.araddr
            shared_bus.R.rready.next = mb.R.rready

            # Route shared bus -> winning master (response channels)
            mb.AW.awready.next = shared_bus.AW.awready
            mb.W.wready.next = shared_bus.W.wready
            mb.AR.arready.next = shared_bus.AR.arready
            mb.B.bvalid.next = shared_bus.B.bvalid
            mb.B.bresp.next = shared_bus.B.bresp
            mb.R.rvalid.next = shared_bus.R.rvalid
            mb.R.rdata.next = shared_bus.R.rdata
            mb.R.rresp.next = shared_bus.R.rresp

    return arb_logic, arb_route

class CPU(MasterDevice):
    def __init__(self, prog_rom: list[Instr], ram: SlaveDevice = None, base: int = 0):
        super().__init__()
        self.pc = base
        self.regs = [0 for _ in range(32)]
        self.csrs = [0 for _ in range(4096)]
        self.prog_rom = prog_rom
        self.ram = ram
        self.base = base
        self.debug = False

    def read(self, addr):
        if self.ram is not None:
            return self.ram.read(addr)
        raise NotImplementedError("No RAM attached")

    def write(self, addr, data):
        if self.ram is not None:
            return self.ram.write(addr, data)
        raise NotImplementedError("No RAM attached")

    def print_state(self, label=""):
        print(f"----- {label} -----")
        print(f"PC: 0x{self.pc:08x}")
        for x in range(32):
            if self.regs[x] != 0:
                print(f"  x{x:2d} = 0x{self.regs[x] & 0xFFFFFFFF:08x}")

    def clk(self):
        # Fetch
        idx = (self.pc - self.base) // 4
        if idx < 0 or idx >= len(self.prog_rom):
            if self.debug:
                print(f"[PC: 0x{self.pc:08x}] ERROR: out of bounds (idx={idx}, prog_len={len(self.prog_rom)})")
            return 0 # error
        instr = self.prog_rom[idx]
        next_pc = self.pc + 4

        # Decode
        op = instr.op
        src1 = instr.src1
        src2 = instr.src2
        imm = instr.imm
        dst = instr.dst

        r1 = self.regs[src1]
        r2 = self.regs[src2]
        rwb = 0
        taken = False

        if self.debug:
            self.print_state(f"Before: 0x{self.pc:08x} {op.name}")

        # Execute
        if(op == Op.ADD):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] ADD x{dst}, x{src1}, x{src2}  -> x{src1}=0x{r1 & 0xFFFFFFFF:08x} + x{src2}=0x{r2 & 0xFFFFFFFF:08x}")
            rwb = r1 + r2
        elif(op == Op.SUB):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] SUB x{dst}, x{src1}, x{src2}  -> x{src1}=0x{r1 & 0xFFFFFFFF:08x} - x{src2}=0x{r2 & 0xFFFFFFFF:08x}")
            rwb = r1 - r2
        elif(op == Op.MUL):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] MUL x{dst}, x{src1}, x{src2}  -> x{src1}=0x{r1 & 0xFFFFFFFF:08x} * x{src2}=0x{r2 & 0xFFFFFFFF:08x}")
            rwb = r1 * r2
        elif(op == Op.DIV):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] DIV x{dst}, x{src1}, x{src2}  -> x{src1}=0x{r1 & 0xFFFFFFFF:08x} / x{src2}=0x{r2 & 0xFFFFFFFF:08x}")
            rwb = r1 // r2 if (r2 != 0) else 0
        elif(op == Op.ADDI):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] ADDI x{dst}, x{src1}, {imm}  -> x{src1}=0x{r1 & 0xFFFFFFFF:08x} + {imm}")
            rwb = r1 + imm
        elif(op == Op.BEQ):
            if r1 == r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BEQ x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'} (x{src1}=0x{r1 & 0xFFFFFFFF:08x} == x{src2}=0x{r2 & 0xFFFFFFFF:08x})")
        elif(op == Op.BNE):
            if r1 != r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BNE x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'} (x{src1}=0x{r1 & 0xFFFFFFFF:08x} != x{src2}=0x{r2 & 0xFFFFFFFF:08x})")
        elif(op == Op.BLT):
            if r1 < r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BLT x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'}")
        elif(op == Op.BGT):
            if r1 > r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BGT x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'}")
        elif(op == Op.BLE):
            if r1 <= r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BLE x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'}")
        elif(op == Op.BGE):
            if r1 >= r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BGE x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'}")
        elif(op == Op.JMP):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] JMP -> 0x{self.pc + imm:08x}")
            next_pc = self.pc + imm
        elif(op == Op.CSRRS):
            csr_val = self.csrs[imm.value] if isinstance(imm, int) else self.csrs[imm.value]
            if r1 != 0:
                self.csrs[imm.value] = csr_val | r1
            rwb = csr_val
            if self.debug: print(f"[PC: 0x{self.pc:08x}] CSRRS x{dst}, {imm}, x{src1}  -> csr=0x{csr_val & 0xFFFFFFFF:08x}")
        elif(op == Op.CSRRW):
            csr_val = self.csrs[imm.value] if isinstance(imm, int) else self.csrs[imm.value]
            if r1 != 0:
                self.csrs[imm.value] = r1
            rwb = csr_val
            if self.debug: print(f"[PC: 0x{self.pc:08x}] CSRRW x{dst}, {imm}, x{src1}  -> csr=0x{csr_val & 0xFFFFFFFF:08x}")
        elif(op == Op.AUIPC):
            rwb = (self.pc + (imm << 12)) & 0xFFFFFFFF
            if self.debug: print(f"[PC: 0x{self.pc:08x}] AUIPC x{dst}, {imm}  -> pc=0x{self.pc:08x} + (0x{imm:x} << 12) = 0x{rwb & 0xFFFFFFFF:08x}")
        elif(op == Op.LUI):
            rwb = (imm << 12) & 0xFFFFFFFF
            if self.debug: print(f"[PC: 0x{self.pc:08x}] LUI x{dst}, 0x{imm:x}  -> (0x{imm:x} << 12) = 0x{rwb & 0xFFFFFFFF:08x}")
        elif(op == Op.LW):
            addr = r1 + imm
            if self.debug: print(f"[PC: 0x{self.pc:08x}] LW x{dst}, {imm}(x{src1})  -> addr=0x{addr & 0xFFFFFFFF:08x}")
            result = self.read(addr)
            if result is not None:
                rwb = result
                if self.debug: print(f"  -> rdata = 0x{result & 0xFFFFFFFF:08x}")
            else:
                print(f"[ERROR] Read Error at 0x{addr & 0xFFFFFFFF:08x}")
                return 0
        elif(op == Op.SW):
            addr = r1 + imm
            if self.debug: print(f"[PC: 0x{self.pc:08x}] SW x{src2}, {imm}(x{src1})  -> addr=0x{addr & 0xFFFFFFFF:08x}, data=0x{r2 & 0xFFFFFFFF:08x}")
            result = self.write(addr, r2)
            if result is None:
                print(f"[ERROR] Write Error at 0x{addr & 0xFFFFFFFF:08x}")
                return 0
            if self.debug: print(f"  -> write OK")
            
        # Writeback
        if(dst != 0):
            self.regs[dst] = rwb & 0xFFFFFFFF
            if self.debug: print(f"  -> WB x{dst} = 0x{rwb & 0xFFFFFFFF:08x}")
        
        if self.debug:
            print(f"  -> next_pc = 0x{next_pc:08x}")
        
        self.pc = next_pc
        return 1

    def axi_clk(self):
        """Generator version of clk() for AXI-Lite testbench integration.
        
        Instead of calling self.read()/self.write() directly, yields commands
        that the testbench wrapper handles through real AXI-Lite transactions:
            yield ('read',  addr)   -> testbench does AXI read, sends rdata back
            yield ('write', addr, data) -> testbench does AXI write, sends None back
            yield ('nop',)          -> ALU/branch: testbench runs 1 clock cycle
        
        Usage in testbench:
            gen = cpu.axi_clk()
            cmd = next(gen)
            while cmd:
                if cmd[0] == 'read':
                    rdata = yield from do_read(cmd[1])
                    cmd = gen.send(rdata)
                elif cmd[0] == 'write':
                    yield from do_write(cmd[1], cmd[2])
                    cmd = gen.send(None)
                elif cmd[0] == 'nop':
                    yield clk.posedge; yield delay(1)
                    cmd = gen.send(None)
        """
        # Fetch
        idx = (self.pc - self.base) // 4
        if idx < 0 or idx >= len(self.prog_rom):
            if self.debug:
                print(f"[PC: 0x{self.pc:08x}] ERROR: out of bounds (idx={idx}, prog_len={len(self.prog_rom)})")
            return  # generator ends -> testbench knows it's done
        instr = self.prog_rom[idx]
        next_pc = self.pc + 4

        # Decode
        op = instr.op
        src1 = instr.src1
        src2 = instr.src2
        imm = instr.imm
        dst = instr.dst

        r1 = self.regs[src1]
        r2 = self.regs[src2]
        rwb = 0
        taken = False

        if self.debug:
            self.print_state(f"Before: 0x{self.pc:08x} {op.name}")

        # Execute
        if(op == Op.ADD):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] ADD x{dst}, x{src1}, x{src2}  -> x{src1}=0x{r1 & 0xFFFFFFFF:08x} + x{src2}=0x{r2 & 0xFFFFFFFF:08x}")
            rwb = r1 + r2
        elif(op == Op.SUB):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] SUB x{dst}, x{src1}, x{src2}  -> x{src1}=0x{r1 & 0xFFFFFFFF:08x} - x{src2}=0x{r2 & 0xFFFFFFFF:08x}")
            rwb = r1 - r2
        elif(op == Op.MUL):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] MUL x{dst}, x{src1}, x{src2}  -> x{src1}=0x{r1 & 0xFFFFFFFF:08x} * x{src2}=0x{r2 & 0xFFFFFFFF:08x}")
            rwb = r1 * r2
        elif(op == Op.DIV):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] DIV x{dst}, x{src1}, x{src2}  -> x{src1}=0x{r1 & 0xFFFFFFFF:08x} / x{src2}=0x{r2 & 0xFFFFFFFF:08x}")
            rwb = r1 // r2 if (r2 != 0) else 0
        elif(op == Op.ADDI):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] ADDI x{dst}, x{src1}, {imm}  -> x{src1}=0x{r1 & 0xFFFFFFFF:08x} + {imm}")
            rwb = r1 + imm
        elif(op == Op.BEQ):
            if r1 == r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BEQ x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'} (x{src1}=0x{r1 & 0xFFFFFFFF:08x} == x{src2}=0x{r2 & 0xFFFFFFFF:08x})")
        elif(op == Op.BNE):
            if r1 != r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BNE x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'} (x{src1}=0x{r1 & 0xFFFFFFFF:08x} != x{src2}=0x{r2 & 0xFFFFFFFF:08x})")
        elif(op == Op.BLT):
            if r1 < r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BLT x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'}")
        elif(op == Op.BGT):
            if r1 > r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BGT x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'}")
        elif(op == Op.BLE):
            if r1 <= r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BLE x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'}")
        elif(op == Op.BGE):
            if r1 >= r2:
                next_pc = self.pc + imm
                taken = True
            if self.debug: print(f"[PC: 0x{self.pc:08x}] BGE x{src1}, x{src2}, {imm}  -> {'TAKEN' if taken else 'NOT TAKEN'}")
        elif(op == Op.JMP):
            if self.debug: print(f"[PC: 0x{self.pc:08x}] JMP -> 0x{self.pc + imm:08x}")
            next_pc = self.pc + imm
        elif(op == Op.CSRRS):
            csr_val = self.csrs[imm.value] if isinstance(imm, int) else self.csrs[imm.value]
            if r1 != 0:
                self.csrs[imm.value] = csr_val | r1
            rwb = csr_val
            if self.debug: print(f"[PC: 0x{self.pc:08x}] CSRRS x{dst}, {imm}, x{src1}  -> csr=0x{csr_val & 0xFFFFFFFF:08x}")
        elif(op == Op.CSRRW):
            csr_val = self.csrs[imm.value] if isinstance(imm, int) else self.csrs[imm.value]
            if r1 != 0:
                self.csrs[imm.value] = r1
            rwb = csr_val
            if self.debug: print(f"[PC: 0x{self.pc:08x}] CSRRW x{dst}, {imm}, x{src1}  -> csr=0x{csr_val & 0xFFFFFFFF:08x}")
        elif(op == Op.AUIPC):
            rwb = (self.pc + (imm << 12)) & 0xFFFFFFFF
            if self.debug: print(f"[PC: 0x{self.pc:08x}] AUIPC x{dst}, {imm}  -> pc=0x{self.pc:08x} + (0x{imm:x} << 12) = 0x{rwb & 0xFFFFFFFF:08x}")
        elif(op == Op.LUI):
            rwb = (imm << 12) & 0xFFFFFFFF
            if self.debug: print(f"[PC: 0x{self.pc:08x}] LUI x{dst}, 0x{imm:x}  -> (0x{imm:x} << 12) = 0x{rwb & 0xFFFFFFFF:08x}")
        elif(op == Op.LW):
            addr = r1 + imm
            if self.debug: print(f"[PC: 0x{self.pc:08x}] LW x{dst}, {imm}(x{src1})  -> addr=0x{addr & 0xFFFFFFFF:08x}")
            # yield to testbench: ('read', addr) -> resumes with rdata
            rdata = yield ('read', addr & 0xFFFFFFFF)
            if rdata is not None:
                rwb = rdata
                if self.debug: print(f"  -> rdata = 0x{rdata & 0xFFFFFFFF:08x}")
            else:
                print(f"[ERROR] Read Error at 0x{addr & 0xFFFFFFFF:08x}")
                return
        elif(op == Op.SW):
            addr = r1 + imm
            if self.debug: print(f"[PC: 0x{self.pc:08x}] SW x{src2}, {imm}(x{src1})  -> addr=0x{addr & 0xFFFFFFFF:08x}, data=0x{r2 & 0xFFFFFFFF:08x}")
            # yield to testbench: ('write', addr, data) -> resumes with None
            yield ('write', addr & 0xFFFFFFFF, r2 & 0xFFFFFFFF)
            if self.debug: print(f"  -> write OK")

        # Writeback
        if(dst != 0):
            self.regs[dst] = rwb & 0xFFFFFFFF
            if self.debug: print(f"  -> WB x{dst} = 0x{rwb & 0xFFFFFFFF:08x}")

        if self.debug:
            print(f"  -> next_pc = 0x{next_pc:08x}")

        self.pc = next_pc

        # Non-memory ops: yield nop so testbench advances the clock
        if op != Op.LW and op != Op.SW:
            yield ('nop',)


class Arbiter:
    def __init__(self, n_ports):
        self.n_ports = n_ports
        self.req = [Request() for _ in range(n_ports)]
        self.gnt = [0 for _ in range(n_ports)]
        self.last_gnt = -1

    def request(self, port, req: Request):
        self.req[port] = req
        print("SlaveDevices requesting:",[i for i in range(self.n_ports) if self.req[i].valid])

    def clk(self):
        # clear grant ?
        self.gnt = [0 for _ in range(self.n_ports)]

        for offset in range(1, self.n_ports + 1):
            idx = (self.last_gnt + offset)
            if(idx >= self.n_ports):
                idx -= self.n_ports
            if self.req[idx].valid:
                self.gnt[idx] = 1
                self.last_gnt = idx
                break

        return self.gnt

@dataclass
class StackItem:
    pc: int          # starting PC of this path
    mask: list[int]  # active threads for this path
    end_pc: int      # PC to resume after finishing this path
    union_mask: list[int]  # all threads that need to converge

def print_R(pc, tid, op, dst, src1, src2, base=0):
    #print(f"[PC {pc} | {pc-base}][WAY {tid}] {op.name} {dst}, {src1}, {src2}")
    print(f"[PC: 0x{pc:08x}][WAY {tid}] {op.name} {dst}, {src1}, {src2}")

def print_B(pc, tid, op, src1, src2, imm, base=0):
    #print(f"[PC {pc} | {pc-base}][WAY {tid}] {op.name} {src1}, {src2}, {imm}")
    print(f"[PC: 0x{pc:08x}][WAY {tid}] {op.name} {src1}, {src2}, {imm}")

def print_J(pc, tid, op, dst, imm, base=0):
    #print(f"[PC {pc} | {pc-base}][WAY {tid}] {op.name} {dst}, {imm}")
    print(f"[PC: 0x{pc:08x}][WAY {tid}] {op.name} {dst}, {imm}")

def print_I(pc, tid, op, dst, src1, imm, base=0):
    #print(f"[PC {pc} | {pc-base}][WAY {tid}] {op.name} {dst}, {src1}, {imm}")
    print(f"[PC: 0x{pc:08x}][WAY {tid}] {op.name} {dst}, {src1}, {imm}")

class Warp:
    def __init__(self, program: list[Instr], NWAYS = 4, base=0x00000000, ram:SlaveDevice=None, arbiter:Arbiter=None):
        self.base = base
        self.pc = base
        self.nways = NWAYS
        self.program = program
        self.rfs = [RegisterFile(tid) for tid in range(self.nways)]
        self.warp_csrs = {
            CSR.reconvpc.value: (base+4) + 4*len(program),
            CSR.execmask.value: int('1'*self.nways,2)
        }
        self.last_endpc = (base+4) + 4*len(program)

        self.active = [1 for _ in range(self.nways)]
        self.stack: list[StackItem] = [
            StackItem(base, self.active.copy(), self.last_endpc+0, self.active.copy())
        ]

        self.debug_state = False
        self.debug_stack = False
        self.debug_set_reconv = False
        self.ram = ram
        self.arb = arbiter

    def print_state(self, label=""):
        #print(f"----- {label} -----")
        #print(f"PC: {self.pc}")
        print(f"Active Mask: {self.active}")
        print("Mask Stack (top->bottom):")
        for entry in reversed(self.stack):
            print(f"  PC: 0x{entry.pc:08x}, Mask: {entry.mask}, End_PC: 0x{entry.end_pc:08x}, Union: {entry.union_mask}")
        for tid, rf in enumerate(self.rfs):
            print(f"[WAY {tid}] Registers: {[rf.regs[i] for i in range(0, 32)]}")  # truncated
        #print("------------------------------")

    def clk(self, clk=0):
        if(self.debug_state):
            self.print_state(f"Start of Cycle {clk}")

        # Before fetch: check if we should pop from stack
        if self.stack:
            top = self.stack[-1]

            popped_item = self.stack.pop()

            if(self.debug_stack):
                print(f"[DEBUG] Poped item: {popped_item}")
                print(f"[DEBUG] After pop stack has ({len(self.stack)}) entries:")
                print(self.stack)

            # Check if current PC is the same as poped_pc
            if(self.pc != popped_item.pc):
                print(f"[DEBUG] Switching to next path -> PC: 0x{popped_item.pc:08x} | Mask: {popped_item.mask}")
            self.active = popped_item.mask[:]
            self.pc = popped_item.pc
            self.last_endpc = popped_item.end_pc

            # Check if current PC has reached the convergence point
            #if self.pc == top.end_pc:
            #    print(f"[DEBUG] Reached convergence point at PC: 0x{self.pc:08x} (0x{self.pc:08x} == 0x{top.end_pc:08x})")
#
            #    # Check if there are more paths to execute
            #    if self.stack:
            #        # There's another path waiting - switch to it
            #        next_item = self.stack.pop()
#
            #        if(self.debug_stack):
            #            print(f"[DEBUG] Poped Next item: {next_item}")
            #            print(f"[DEBUG] After pop stack has ({len(self.stack)}) entries:")
            #            print(self.stack)
            #        print(f"[DEBUG] Switching to next path -> PC: 0x{next_item.pc:08x} | Mask: {next_item.mask}")
            #        self.active = next_item.mask[:]
            #        self.pc = next_item.pc
            #        # Do NOT pop here - leave it on the stack to be popped when this path finishes

        # fetch
        if((self.pc-self.base)//4 >= len(self.program)):
            self.active = [0 for _ in range(self.nways)]
            return # Raise exception ? Wrap around ?
        instr = self.program[(self.pc-self.base)//4]
        next_pc = self.pc + 4

        # decode
        op = instr.op
        src1 = instr.src1
        src2 = instr.src2
        imm = instr.imm
        dst = instr.dst

        r1s = [self.rfs[tid].regs[src1] for tid in range(self.nways)]
        r2s = [self.rfs[tid].regs[src2] for tid in range(self.nways)]
        rwbs = [0 for _ in range(self.nways)]
        tgts = [next_pc  for _ in range(self.nways)]
        taken_mask = [0 for _ in range(self.nways)]
        not_taken_mask = [0 for _ in range(self.nways)]

        print(f"[ACTIVE]: {''.join(str(x) for x in self.active)}")

        # execute 
        for tid in range(self.nways):
            if(self.active[tid]):
                if(op == Op.ADD):
                    print_R(self.pc, tid, op, dst, src1, src2, self.base)
                    rwbs[tid] = r1s[tid] + r2s[tid]
                elif(op == Op.SUB):
                    print_R(self.pc, tid, op, dst, src1, src2, self.base)
                    rwbs[tid] = r1s[tid] - r2s[tid]
                elif(op == Op.MUL):
                    print_R(self.pc, tid, op, dst, src1, src2, self.base)
                    rwbs[tid] = r1s[tid] * r2s[tid]
                elif(op == Op.DIV):
                    print_R(self.pc, tid, op, dst, src1, src2, self.base)
                    rwbs[tid] = r1s[tid] * r2s[tid] if (r2s[tid] != 0) else 0
                elif(op == Op.ADDI):
                    print_I(self.pc, tid, op, dst, src1, imm, self.base)
                    rwbs[tid] = r1s[tid] + imm
                elif(op == Op.BEQ):
                    print_B(self.pc, tid, op, src1, src2, imm, self.base)
                    if(r1s[tid] == r2s[tid]):
                        tgts[tid] = self.pc + imm
                        taken_mask[tid] = 1
                        print(f"Branched To {tgts[tid]}!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = 1
                elif(op == Op.BNE):
                    print_B(self.pc, tid, op, src1, src2, imm, self.base)
                    if(r1s[tid] != r2s[tid]):
                        tgts[tid] = self.pc + imm
                        taken_mask[tid] = 1
                        print(f"Branched To {tgts[tid]}!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = 1
                elif(op == Op.BLT):
                    print_B(self.pc, tid, op, src1, src2, imm, self.base)
                    if(r1s[tid] < r2s[tid]):
                        tgts[tid] = self.pc + imm
                        taken_mask[tid] = 1
                        print(f"Branched To {tgts[tid]}!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = 1
                elif(op == Op.BGT):
                    print_B(self.pc, tid, op, src1, src2, imm, self.base)
                    if(r1s[tid] > r2s[tid]):
                        tgts[tid] = self.pc + imm
                        taken_mask[tid] = 1
                        print(f"Branched To {tgts[tid]}!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = 1
                elif(op == Op.BLE):
                    print_B(self.pc, tid, op, src1, src2, imm, self.base)
                    if(r1s[tid] <= r2s[tid]):
                        tgts[tid] = self.pc + imm
                        taken_mask[tid] = 1
                        print(f"Branched To {tgts[tid]}!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = 1
                elif(op == Op.BGE):
                    print_B(self.pc, tid, op, src1, src2, imm, self.base)
                    if(r1s[tid] >= r2s[tid]):
                        tgts[tid] = self.pc + imm
                        taken_mask[tid] = 1
                        print(f"Branched To {tgts[tid]}!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = 1
                elif(op == Op.JMP):
                    print_J(self.pc, tid, op, dst, imm, self.base)
                    tgts[tid] = self.pc + imm
                    print(f"Jumping To {tgts[tid]}!")
                elif(op == Op.CSRRS):
                    print_I(self.pc, tid, op, dst, imm.name, src1, self.base)
                    if imm == CSR.reconvpc:  # Warp-level CSR
                        initial_val = self.warp_csrs.get(imm.value, 0)
                        if r1s[tid] != 0:
                            self.warp_csrs[imm.value] = initial_val | r1s[tid]
                            if(self.debug_set_reconv):
                                print(f"[DEBUG] Setting RECONV_PC = {initial_val | r1s[tid]} | [BASE+{(initial_val | r1s[tid])-self.base}]")
                        rwbs[tid] = initial_val
                    elif imm == CSR.execmask:
                        initial_val = self.rfs[tid].csrs[imm.value]
                        new_val = initial_val | r1s[tid]
                        self.rfs[tid].csrs[imm.value] = new_val
                        rwbs[tid] = initial_val
                        print(f"[DEBUG] Setting EXEC_MASK = {bin(new_val)[2:].zfill(self.nways)}")
                    else:  # Thread-local CSR
                        initial_val = self.rfs[tid].csrs[imm.value]
                        if(r1s[tid] != 0):
                            self.rfs[tid].csrs[imm.value] = initial_val | r1s[tid]
                        rwbs[tid] = initial_val
                elif(op == Op.CSRRW):
                    print_I(self.pc, tid, op, dst, imm.name, src1, self.base)
                    if imm == CSR.reconvpc:  # Warp-level CSR
                        initial_val = self.warp_csrs.get(imm.value, 0)
                        if r1s[tid] != 0:
                            self.warp_csrs[imm.value] = r1s[tid]
                            if(self.debug_set_reconv):
                                print(f"[DEBUG] Setting RECONV_PC = {r1s[tid]} | [BASE+{r1s[tid]-self.base}]")
                        rwbs[tid] = initial_val
                    elif imm == CSR.execmask:
                        initial_val = self.rfs[tid].csrs[imm.value]
                        self.rfs[tid].csrs[imm.value] = r1s[tid]
                        rwbs[tid] = initial_val
                    else:  # Thread-local CSR
                        initial_val = self.rfs[tid].csrs[imm.value]
                        if(r1s[tid] != 0):
                            self.rfs[tid].csrs[imm.value] = r1s[tid]
                        rwbs[tid] = initial_val
                elif(op == Op.AUIPC):
                    print_I(self.pc, tid, op, dst, src1, imm, self.base)
                    rwbs[tid] = (self.pc + (imm << 12)) & 0xFFFFFFFF
                elif(op == Op.LUI):
                    print_I(self.pc, tid, op, dst, src1, imm, self.base)
                    rwbs[tid] = (imm << 12) & 0xFFFFFFFF
                elif(op == Op.LW):
                    print(f"[PC: 0x{self.pc:08x}][WAY {tid}] {op.name} {dst}, {imm}({src1}), ")
                    result = self.ram.read(r1s[tid] + imm)
                    self.arb.request(tid, Request(valid=True, addr=r1s[tid] + imm, we=False))
                    if result is not None:
                        rwbs[tid] = result
                    else:
                        print("[DEBUG] Read Error!")
                        return
                elif(op == Op.SW):
                    print(f"[PC: 0x{self.pc:08x}][WAY {tid}] {op.name} {src2}, {imm}({src1}), ")
                    result = self.ram.write(r1s[tid] + imm, r2s[tid])
                    self.arb.request(tid, Request(valid=True, addr=r1s[tid] + imm, we=True, data=r2s[tid]))
                    if result is None:
                        print("[ERROR] Write Error!")
                        return

        # If execmask was written (CSRRW), update the warp active mask from the written CSR value
        if op == Op.CSRRW and imm == CSR.execmask:
            # Use the first active thread's execmask value (only active threads executed the write)
            writer_tid = next(i for i in range(self.nways) if self.active[i])
            new_mask = self.rfs[writer_tid].csrs[imm.value]
            self.active = [(new_mask >> i) & 1 for i in range(self.nways)]
            print(f"[DEBUG] Setting EXEC_MASK = {bin(new_mask)[2:].zfill(self.nways)} -> Active={self.active}")

        # Push branch paths if divergence
        if any(taken_mask) and any(not_taken_mask):
            end_pc = self.warp_csrs.get(CSR.reconvpc.value, 0)
            print(f"[DEBUG] Divergence detected! end_pc from reconvpc CSRs: [0x{end_pc:08x}] | [PC+{end_pc-self.pc}] | [BASE+{end_pc-self.base}]")
            
            # Create union mask - all threads that participated in the branch
            union_mask = [1*(taken_mask[i] or not_taken_mask[i]) for i in range(self.nways)]
            
            # Push both paths to stack (order matters - we'll execute top first)
            self.stack.append(StackItem(pc=end_pc, mask=union_mask, end_pc=self.last_endpc, union_mask=union_mask))
            self.stack.append(StackItem(pc=next_pc, mask=not_taken_mask[:], end_pc=end_pc, union_mask=union_mask))
            self.stack.append(StackItem(pc=self.pc + imm, mask=taken_mask[:], end_pc=end_pc, union_mask=union_mask))
            self.last_endpc = end_pc
            if(self.debug_stack):
                print(f"[DEBUG] Stack after push:")
                for i, item in enumerate(self.stack[::-1]):
                    print(f"  [{i}] PC: 0x{item.pc:08x}, Mask: {item.mask}, End_PC: 0x{item.end_pc:08x}")

            # Start with the top path (taken) but DON'T pop it yet
            top = self.stack[-1]
            self.active = top.mask[:]
            self.pc = top.pc
            print(f"[DEBUG] Starting first path: PC: 0x{self.pc:08x}, Active={self.active}")
        else:
            # writeback
            for tid in range(self.nways):
                if (dst != 0 and self.active[tid]):
                    self.rfs[tid].regs[dst] = rwbs[tid]

            # Update warp PC
            for tid in range(self.nways):
                if (self.active[tid] or tgts[tid] != next_pc):
                    self.pc = tgts[tid]
                    break
            
            if(self.pc >= self.last_endpc):
                print(f"[DEBUG] Reached convergence point at PC: 0x{self.pc:08x} (0x{self.pc:08x} == 0x{self.last_endpc:08x})")
            else:
                self.stack.append(StackItem(pc=self.pc, mask=self.active, end_pc=self.last_endpc, union_mask=self.active))
            if(self.debug_stack):
                print(f"[DEBUG] Stack after push:")
                for i, item in enumerate(self.stack[::-1]):
                    print(f"  [{i}] PC: 0x{item.pc:08x}, Mask: {item.mask}, End_PC: 0x{item.end_pc:08x}")
        # self.print_state("End of Cycle")

def main():
    sasm = SAsm(base=0x80000000)

    N_WAYS = 8

    """
    main() {
        x5 = thread_id()
        if(x5 > 3) {
            x16 = 16
            x17 = 17
        }
        else {
            x14 = 14
            x15 = 15
            if(x5 > 1 ) {
                x22 = 22
                x23 = 23
            }
            else {
                x20 = 20
                x21 = 21
            }
            x24 = 24
            x25 = 25
        }
        x18 = 18
        x19 = 19
    }
    """

    pseudo_asm = """
    .start:
        LI x10, 128
        CSRRW x0, execmask, x10
        LI x10, 8
    .loop:
        ADDI x11, x11, 1
        BLT x11, x10, .loop
    .loop_end:
        LI x10, 0xff
        CSRRW x0, execmask, x10
        LI x1, 3
        LA x10, .end0
        CSRRW  x0, reconvpc, x10
        LI x10, 0
        CSRRS x5, mhartid, x0
        LI x4, 4
        #MUL x11, x5, x4
        LA x12, .data
        ADD x11, x12, x11
        SW x5, 0(x11)
        LW x6, 0(x11)
        BGT x5, x1, .if0
    .else0:
        LI x14, 14
        LI x15, 15
        LI x2, 1
        LA x10, .end1
        CSRRW  x0, reconvpc, x10
        LI x10, 0
        BGT x5, x2, .if1
    .else1:
        LI x20, 20
        LI x21, 21
        JMP .end1
    .if1:
        LI x22, 22
        LI x23, 23
    .end1:
        LI x24, 24
        LI x25, 25
        #LA x10, .end0
        #CSRRW  x0, reconvpc, x10
        #LI x10, 0
        JMP .end0
    .if0:
        LI x16, 16
        LI x17, 17
    .end0:
        LI x18, 18
        LI x19, 19
    .data:
    """

    expected_regs = [
        # mhartid = 0
        {'x14': 14, 'x15': 15, 'x16': 0, 'x17': 0, 'x18': 18, 'x19': 19,
        'x20': 20, 'x21': 21, 'x22': 0, 'x23': 0, 'x24': 24, 'x25': 25},

        # mhartid = 1
        {'x14': 14, 'x15': 15, 'x16': 0, 'x17': 0, 'x18': 18, 'x19': 19,
        'x20': 20, 'x21': 21, 'x22': 0, 'x23': 0, 'x24': 24, 'x25': 25},

        # mhartid = 2
        {'x14': 14, 'x15': 15, 'x16': 0, 'x17': 0, 'x18': 18, 'x19': 19,
        'x20': 0,  'x21': 0,  'x22': 22, 'x23': 23, 'x24': 24, 'x25': 25},

        # mhartid = 3
        {'x14': 14, 'x15': 15, 'x16': 0, 'x17': 0, 'x18': 18, 'x19': 19,
        'x20': 0,  'x21': 0,  'x22': 22, 'x23': 23, 'x24': 24, 'x25': 25},

        # mhartid = 4
        {'x14': 0,  'x15': 0,  'x16': 16, 'x17': 17, 'x18': 18, 'x19': 19,
        'x20': 0,  'x21': 0,  'x22': 0,  'x23': 0,  'x24': 0, 'x25': 0},

        # mhartid = 5
        {'x14': 0,  'x15': 0,  'x16': 16, 'x17': 17, 'x18': 18, 'x19': 19,
        'x20': 0,  'x21': 0,  'x22': 0,  'x23': 0,  'x24': 0, 'x25': 0},

        # mhartid = 6
        {'x14': 0,  'x15': 0,  'x16': 16, 'x17': 17, 'x18': 18, 'x19': 19,
        'x20': 0,  'x21': 0,  'x22': 0,  'x23': 0,  'x24': 0, 'x25': 0},

        # mhartid = 7
        {'x14': 0,  'x15': 0,  'x16': 16, 'x17': 17, 'x18': 18, 'x19': 19,
        'x20': 0,  'x21': 0,  'x22': 0, 'x23': 0,  'x24': 0, 'x25': 0},
    ]

    print("[INFO] Input Program:")
    print(pseudo_asm)

    print(f"[INFO] Linking to 0x{sasm.base:08x}")
    _pc = sasm.base
    prog = sasm.compile(pseudo_asm)

    print("[DEBUG] Instructions:")
    for instr in prog:
        lab_name = [k for k, v in sasm.labels.items() if v == _pc]
        if(len(lab_name)>0):
            print(lab_name[0]+":")
        #print("   ",_pc,'|',_pc-sasm.base,instr)
        print(f"    {_pc:08x} {instr}")
        _pc+=4
    
    # Simulation Signals
    clk = Signal(bool(0))
    reset = ResetSignal(0, active=1, isasync=False)
    bus = AXILiteBus()  # shared bus instance

    # Data RAM
    ram = SlaveDevice(_pc-4, 128, 4)
    ram_axil = AXILiteSlavePort(clk, reset, bus, ram)

    # Arbiter
    arb = Arbiter(N_WAYS)

    # WARP Core
    warp = Warp(prog, N_WAYS, base=sasm.base, ram=ram, arbiter=arb)
    

    # WARP Execution
    clk = 0
    while(any(a == True for a in warp.active)):
        if(clk > 70):
            break
        print(f"------- Cycle {clk} ------- ")
        warp.clk(clk)
        print("------------------------------")
        clk+=1

    fail = False

    for way in range(warp.nways):
        expected_way = expected_regs[way]
        print(f"\n[Testing Way {way}]")
        for reg, expected_val in expected_way.items():
            x = int(reg[1:])  # convert 'x14' -> 14
            actual_val = warp.rfs[way].regs[x]
            string = "PASS"
            if expected_val != actual_val:
                fail = True
                string = "FAIL"
            print(f"Register {reg}: expected={expected_val:02d}, actual={actual_val:02d} [{string}]")

    if fail:
        print("\n[ERROR] Tests don't pass!")
    else:
        print("\n[OK] All tests pass!")


    for way in range(warp.nways):
        print(f"[Way {way}]: Register File Contents")
        for x in range(32):
            print(f"  [x{x}]: {warp.rfs[way].regs[x]}")



if __name__ == "__main__":
    main()
