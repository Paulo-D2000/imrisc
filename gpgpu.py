from isa import *
from sasm import SAsm

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
    def __init__(self, program: list[Instr], NWAYS = 4, base=0x00000000):
        self.base = base
        self.pc = base
        self.nways = NWAYS
        self.program = program
        self.rfs = [RegisterFile(tid) for tid in range(self.nways)]
        self.warp_csrs = {
            CSR.reconvpc.value: (base+4) + 4*len(program)
        }
        self.last_endpc = (base+4) + 4*len(program)

        self.active = [1 for _ in range(self.nways)]
        self.stack: list[StackItem] = [
            StackItem(base, self.active.copy(), self.last_endpc+0, self.active.copy())
        ]

        self.debug_state = False
        self.debug_stack = False
        self.debug_set_reconv = False

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

    pseudo_asm = """
    .start:
        LI x1, 3
        LA x10, .end0
        CSRRW  x0, reconvpc, x10
        LI x10, 0
        CSRRS x5, mhartid, x0
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

    warp = Warp(prog, 8, base=sasm.base)

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
