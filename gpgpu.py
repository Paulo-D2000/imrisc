from isa import *
from sasm import SAsm

@dataclass
class StackItem:
    pc: int          # starting PC of this path
    mask: list[int]  # active threads for this path
    end_pc: int      # PC to resume after finishing this path
    union_mask: list[int]  # all threads that need to converge

def print_R(pc, tid, op, dst, src1, src2):
    print(f"[PC {pc}][WAY {tid}] {op.name} {dst}, {src1}, {src2}")

def print_B(pc, tid, op, src1, src2, imm):
    print(f"[PC {pc}][WAY {tid}] {op.name} {src1}, {src2}, {imm}")

def print_J(pc, tid, op, dst, imm):
    print(f"[PC {pc}][WAY {tid}] {op.name} {dst}, {imm}")

def print_I(pc, tid, op, dst, src1, imm):
    print(f"[PC {pc}][WAY {tid}] {op.name} {dst}, {src1}, {imm}")

class Warp:
    def __init__(self, program: list[Instr], NWAYS = 4, base=0x00000000):
        self.base = base
        self.pc = base
        self.nways = NWAYS
        self.program = program
        self.rfs = [RegisterFile(tid) for tid in range(self.nways)]

        self.active = [1 for _ in range(self.nways)]
        self.stack: list[StackItem] = []

    def print_state(self, label=""):
        print(f"----- {label} -----")
        print(f"PC: {self.pc}")
        print(f"Active Mask: {self.active}")
        print("Mask Stack (top->bottom):")
        for entry in reversed(self.stack):
            print(f"  PC: {entry.pc}, Mask: {entry.mask}, End_PC: {entry.end_pc}, Union: {entry.union_mask}")
        for tid, rf in enumerate(self.rfs):
            print(f"[WAY {tid}] Registers: {[rf.regs[i] for i in range(0, 32)]}")  # truncated
        print("------------------------------")

    def clk(self, clk=0):
        #self.print_state(f"Start of Cycle {clk}")

        # Before fetch: check if we should pop from stack
        if self.stack:
            top = self.stack[-1]
            
            # Check if current PC has reached the convergence point
            if self.pc >= top.end_pc:
                print(f"[DEBUG] Reached convergence point at PC {self.pc}")
                popped_item = self.stack.pop()
                
                # Check if there are more paths to execute
                if self.stack:
                    # There's another path waiting - switch to it
                    next_item = self.stack[-1]
                    print(f"[DEBUG] Switching to next path -> PC: {next_item.pc} | Mask: {next_item.mask}")
                    self.active = next_item.mask[:]
                    self.pc = next_item.pc
                    # Do NOT pop here - leave it on the stack to be popped when this path finishes
                else:
                    # All paths complete - activate all threads for convergence
                    print(f"[DEBUG] All paths complete! Activating union mask: {popped_item.union_mask}")
                    self.active = popped_item.union_mask[:]
                    # PC stays at convergence point (end_pc)
                    # Continue execution with all threads active

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
        taken_mask = [False for _ in range(self.nways)]
        not_taken_mask = [False for _ in range(self.nways)]

        # execute 
        for tid in range(self.nways):
            if(self.active[tid]):
                if(op == Op.ADD):
                    print_R(self.pc, tid, op, dst, src1, src2)
                    rwbs[tid] = r1s[tid] + r2s[tid]
                elif(op == Op.SUB):
                    print_R(self.pc, tid, op, dst, src1, src2)
                    rwbs[tid] = r1s[tid] - r2s[tid]
                elif(op == Op.MUL):
                    print_R(self.pc, tid, op, dst, src1, src2)
                    rwbs[tid] = r1s[tid] * r2s[tid]
                elif(op == Op.DIV):
                    print_R(self.pc, tid, op, dst, src1, src2)
                    rwbs[tid] = r1s[tid] * r2s[tid] if (r2s[tid] != 0) else 0
                elif(op == Op.ADDI):
                    print_I(self.pc, tid, op, dst, src1, imm)
                    rwbs[tid] = r1s[tid] + imm
                elif(op == Op.BEQ):
                    print_B(self.pc, tid, op, src1, src2, imm)
                    if(r1s[tid] == r2s[tid]):
                        tgts[tid] = imm
                        taken_mask[tid] = True
                        print("Branched!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = True
                elif(op == Op.BNE):
                    print_B(self.pc, tid, op, src1, src2, imm)
                    if(r1s[tid] != r2s[tid]):
                        tgts[tid] = imm
                        taken_mask[tid] = True
                        print("Branched!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = True
                elif(op == Op.BLT):
                    print_B(self.pc, tid, op, src1, src2, imm)
                    if(r1s[tid] < r2s[tid]):
                        tgts[tid] = imm
                        taken_mask[tid] = True
                        print("Branched!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = True
                elif(op == Op.BGT):
                    print_B(self.pc, tid, op, src1, src2, imm)
                    if(r1s[tid] > r2s[tid]):
                        tgts[tid] = imm
                        taken_mask[tid] = True
                        print("Branched!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = True
                elif(op == Op.BLE):
                    print_B(self.pc, tid, op, src1, src2, imm)
                    if(r1s[tid] <= r2s[tid]):
                        tgts[tid] = imm
                        taken_mask[tid] = True
                        print("Branched!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = True
                elif(op == Op.BGE):
                    print_B(self.pc, tid, op, src1, src2, imm)
                    if(r1s[tid] >= r2s[tid]):
                        tgts[tid] = imm
                        taken_mask[tid] = True
                        print("Branched!")
                    else:
                        tgts[tid] = next_pc
                        not_taken_mask[tid] = True
                elif(op == Op.JMP):
                    print_J(self.pc, tid, op, dst, imm)
                    tgts[tid] = imm # fake absolute jmp
                    print("Jumping!")
                elif(op == Op.CSRRS):
                    print_I(self.pc, tid, op, dst, imm.name, src1)
                    initial_val = self.rfs[tid].csrs[imm.value]
                    if(r1s[tid] != 0):
                        self.rfs[tid].csrs[imm.value] = initial_val | r1s[tid]
                    rwbs[tid] = initial_val
                elif(op == Op.AUIPC):
                    print_I(self.pc, tid, op, dst, src1, imm)
                    rwbs[tid] = (self.pc + (imm << 12)) & 0xFFFFFFFF
                elif(op == Op.LUI):
                    print_I(self.pc, tid, op, dst, src1, imm)
                    rwbs[tid] = (imm << 12) & 0xFFFFFFFF

        # Push branch paths if divergence
        if any(taken_mask) and any(not_taken_mask):
            end_pc = max(rf.csrs[CSR.reconvpc.value] for rf in self.rfs)
            # print(f"[DEBUG] Divergence detected! end_pc from reconvpc CSRs: {end_pc}")
            
            # Create union mask - all threads that participated in the branch
            union_mask = [taken_mask[i] or not_taken_mask[i] for i in range(self.nways)]
            
            # Push both paths to stack (order matters - we'll execute top first)
            self.stack.append(StackItem(pc=next_pc, mask=not_taken_mask[:], end_pc=end_pc, union_mask=union_mask))
            self.stack.append(StackItem(pc=imm, mask=taken_mask[:], end_pc=end_pc, union_mask=union_mask))
            
            #print(f"[DEBUG] Stack after push:")
            #for i, item in enumerate(self.stack):
            #    print(f"  [{i}] PC: {item.pc}, Mask: {item.mask}, End_PC: {item.end_pc}")

            # Start with the top path (taken) but DON'T pop it yet
            top = self.stack[-1]
            self.active = top.mask[:]
            self.pc = top.pc
            # print(f"[DEBUG] Starting first path: PC={self.pc}, Active={self.active}")
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

        # self.print_state("End of Cycle")

def main():
    '''
    prog = [
        # Start
        Instr(op=Op.ADDI,  dst=5, src1=0, imm=48),             #0
        Instr(op=Op.CSRRS, dst=0, src1=5, imm=CSR.reconvpc),   #4
        Instr(op=Op.CSRRS, dst=4, src1=0, imm=CSR.mhartid),    #8
        Instr(op=Op.ADDI,  dst=1, src1=0, imm=1),              #12

        # If (Diverge)
        Instr(op=Op.BGT,  src1=4, src2=1, imm=32),             #16
        Instr(op=Op.ADDI, dst=10, src1=0, imm=10),             #20
        Instr(op=Op.ADDI, dst=11, src1=0, imm=11),             #24
        Instr(op=Op.JMP,  dst=0,  imm=48),                     #28

        # Else
        Instr(op=Op.ADDI, dst=12, src1=0,  imm=12),            #32
        Instr(op=Op.ADDI, dst=13, src1=0,  imm=13),            #36
        Instr(op=Op.ADDI, dst=0,  src1=0,  imm=0),             #40
        Instr(op=Op.JMP,  dst=0,  imm=48),                     #44

        # End (Converge)
        Instr(op=Op.ADDI, dst=14, src1=0, imm=14),             #48
        Instr(op=Op.ADDI, dst=15, src1=0, imm=15),             #52
        Instr(op=Op.ADDI, dst=16, src1=0, imm=16),             #56
        Instr(op=Op.ADDI, dst=17, src1=0, imm=17),             #60
    ]
    '''

    sasm = SAsm(base=0x80000000)

    pseudo_asm = """
    .start:
    LA x10, .end0
    LI x1, 3
    CRRS  x0. reconvpc, x10
    LI x10, 0
    CSRRS x5, mhartid, x0
    BGT x5, x1, .if0
    .else0:
        LI x9, 9
    .if0:
        LI x10, 10
    .end0:
        LI x11, 11
    """

    prog = sasm.compile(pseudo_asm)
    for instr in prog:
        print(instr)

    warp = Warp(prog, 8, base=sasm.base)

    clk = 0
    while(any(a == True for a in warp.active)):
        print(f"------- Cycle {clk} ------- ")
        warp.clk(clk)
        print("------------------------------")
        clk+=1

    for way in range(warp.nways):
        print(f"[Way {way}]: Register File Contents")
        for x in range(32):
            print(f"  [x{x}]: {warp.rfs[way].regs[x]}")



if __name__ == "__main__":
    main()
