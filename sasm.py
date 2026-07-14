# sasm.py — Clean C → IR → Reconvergent RISC-V-like Assembly
# Features:
#   • Full C-like syntax (int, if/else, for, +=, __tid__)
#   • Generates clean Intermediate Representation (IR)
#   • Final stage emits reconvergent code:
#        → every branch is preceded by "ADDI x5, x0, label; CSRRS x0, x5, reconvpc"
#        → __tid__ automatically becomes CSRRS mhartid
#   • Boot code always starts at address 0

from dataclasses import dataclass
from enum import Enum, auto
import re
from typing import List, Any
from isa import *

def parse_imm(s: str) -> int:
    """Parse an immediate that may be decimal or hex (0x…), with optional leading '-'."""
    neg = s.startswith('-')
    if neg:
        s = s[1:]
    val = int(s, 0)
    return -val if neg else val

class SAsm:
    def __init__(self,base=0x00000000):
        self.program: List[Instr] = []
        self.labels: Dict[str, int] = {}  # label -> program index
        self.next_reg = 8  # Starting at x8 for vars
        self.base = base

    def new_label(self, label_name="L"):
        # Ensure the label only gets created once, and assign it to the current program length
        if label_name not in self.labels:
            self.labels[label_name] = 4*len(self.program) + self.base # Position of the label in the program
            # print(f"Creating label '{label_name}' at position {self.labels[label_name]}")
        return label_name

    def emit(self, op, dst, src1, src2, imm, comment=""):
        # print("Emiting",op,dst,src1,src2,imm)
        self.program.append(Instr(op, dst, src1, src2, imm, comment))

    def compile(self, pseudo_asm: str) -> List[Instr]:
        lines = pseudo_asm.splitlines()

        for line in lines:
            line = line.strip()

            # print(f"line: {line}")

            comment = line.split('#',1)
            if(comment):
                line = comment[0]

            # Parse labels (e.g., .if_end:, .end:)
            label_match = re.match(r'(\.\S+):', line)  # Match labels like ".if_end:"
            if label_match:
                label = label_match.group(1)
                self.new_label(label)
                continue

            # Match Pseudo Op LA
            mla = re.match(r'(LA)\s+x(\d+),\s*(\.\S+)', line)
            if mla:
                iop, dst, label = mla.groups()
                dst = int(dst)
                self.emit(Op.AUIPC, dst, 0, 0, f"%pcrel_hi({label})", f"# Load high part of address for {label}")
                self.emit(Op.ADDI, dst, dst, 0, f"%pcrel_lo({label})", f"# Add low part of address for {label}")
                continue

            mli = re.match(r'(LI)\s+x(\d+),\s*(-?(?:0[xX][0-9A-Fa-f]+|\d+))', line)
            if mli:
                iop, dst, imm = mli.groups()
                dst,imm = int(dst), parse_imm(imm)
                if(imm > 4096):
                    self.emit(Op.LUI, dst, 0, 0, (imm >> 12) & 0xFFFFF, f"# Load high part of value for {imm}")
                    self.emit(Op.ADDI, dst, dst, 0, imm & 0xFFF, f"# Add low part of value for {imm}")
                else:
                    self.emit(Op.ADDI, dst, 0, 0, imm & 0xFFF, f"# Load {imm} to {dst}")
                print(iop, dst, imm)
                continue

            # Match operations that take three operands (ADD, SUB, MUL, DIV)
            mr = re.match(r'(ADD|SUB|MUL|DIV)\s+x(\d+),\s*x(\d+),\s*x(\d+)', line)
            if mr:
                rop, dst, src1, src2 = mr.groups()
                dst, src1, src2 = int(dst), int(src1), int(src2)
                op = Op[rop]
                self.emit(op, dst, src1, src2, 0, f"#{len(self.program)}")
                continue

            # Match operations that take two operands (ADDI)
            mi = re.match(r'(ADDI)\s+x(\d+),\s*x(\d+),\s*(-?(?:0[xX][0-9A-Fa-f]+|\d+))', line)
            if mi :
                iop, dst, src1, imm = mi.groups()
                dst, src1, imm = int(dst), int(src1), parse_imm(imm)
                op = Op[iop]
                self.emit(op, dst, src1, 0, imm, f"#{len(self.program)}")
                continue

            # Match branch operations that take two registers and a label (BEQ, BNE, BLT, BGT, BLE, BGE)
            mb = re.match(r'(BEQ|BNE|BLT|BGT|BLE|BGE)\s+x(\d+),\s*x(\d+),\s*(\S+)', line)
            if mb:
                branch_op, src1, src2, label = mb.groups()
                src1, src2 = int(src1), int(src2)
                branch_op_enum = Op[branch_op]  # Convert string to Op enum
                self.emit(branch_op_enum, 0, src1, src2, label, f"# Branch {branch_op} x{src1} with x{src2} to {label}")
                continue

            # Match Pseudo op JMP instruction (unconditional jump with a label)
            mj = re.match(r'(JMP|J)\s+(\S+)', line)
            if mj:
                jop, label = mj.groups()
                self.emit(Op.JMP, 0, 0, 0, label, f"# Unconditional jump to {label}")
                continue

            # Match CSR instructions (control/status register read/write)
            mcsr = re.match(r'(CSRRS|CSRRW)\s+x(\d+),\s*(\S+),\s*x(\d+)', line)
            if mcsr:
                csrop, dst, csr_name, src1 = mcsr.groups()
                dst, src1 = int(dst), int(src1)
                csr_enum = CSR[csr_name]  # Convert string to CSR enum
                csr_op_enum = Op[csrop]
                self.emit(csr_op_enum, dst, src1, 0, csr_enum, f"# Read CSR {csr_name}")
                continue

            # Match Load instruction
            mload = re.match(r'(LW)\s+x(\d+),\s*(-?(?:0[xX][0-9A-Fa-f]+|\d+))\(x(\d+)\)', line)
            if mload :
                iop, dst, imm, src1 = mload.groups()
                dst, src1, imm = int(dst), int(src1), parse_imm(imm)
                op = Op[iop]
                self.emit(op, dst, src1, 0, imm, f"#{len(self.program)}")
                continue

                
            # Match store instruction
            mstore = re.match(r'(SW)\s+x(\d+),\s*(-?(?:0[xX][0-9A-Fa-f]+|\d+))\(x(\d+)\)', line)
            if mstore :
                iop, src2, imm, src1 = mstore.groups()
                src2, src1, imm = int(src2), int(src1), parse_imm(imm)
                op = Op[iop]
                self.emit(op, 0, src1, src2, imm, f"#{len(self.program)}")
                continue

        self.resolve_labels()
        return self.program  # Return the list of Instr objects

    def resolve_labels(self):
        # Resolve label addresses by updating the instruction's imm field
        for i, instr in enumerate(self.program):
            if isinstance(instr.imm, str):  # If imm is a label
                # print(f"Searching for {instr.imm} in {self.labels}")
                lab = instr.imm
                if lab in self.labels:
                    label_index = self.labels[lab]
                    # Resolve label to absolute byte address
                    # instr.imm = label_index - self.base # Each instruction is 4 bytes
                    current_pc = i * 4 + self.base
                    instr.imm = self.labels[lab] - current_pc  # PC-relative offset
                    print(f"Resolving label '{lab}' (label index {label_index}) to byte offset {instr.imm}")
                else:
                    pcrel_hi = re.match(r'\%pcrel_hi\((\S+)\)',lab)
                    if pcrel_hi:
                        hlab = pcrel_hi.groups()[0]
                        label_index = self.labels[hlab]
                        # Resolve label to relative byte address
                        label_addr = self.labels[hlab]
                        current_pc = i * 4 + self.base - 4
                        offset = label_addr - current_pc 
                        instr.imm = (offset >> 12) & 0xFFFFF # Top 20 bits
                        print(f"Resolving (high) address '{lab}' (label index {label_index}) to byte offset {instr.imm}")
                        continue

                    pcrel_lo = re.match(r'\%pcrel_lo\((\S+)\)',lab)
                    if pcrel_lo:
                        llab = pcrel_lo.groups()[0]
                        # Resolve label to relative byte address
                        label_addr = self.labels[hlab]
                        current_pc = i * 4 + self.base - 4
                        offset = label_addr - current_pc 
                        instr.imm = offset & 0xFFF # Bottom 12 bits
                        print(f"Resolving (low) address '{lab}' (label index {label_index}) to byte offset {instr.imm}")
                        continue

                    hi = re.match(r'\%hi\((\S+)\)',lab)
                    if hi:
                        hlab = hi.groups()[0]
                        label_index = self.labels[hlab]
                        # Resolve label to absolute byte address
                        instr.imm = (label_index >> 12) & 0xFFFFF # Top 20 bits
                        print(f"Resolving (high) address '{lab}' (label index {label_index}) to byte offset {instr.imm}")
                        continue
                    lo = re.match(r'\%lo\((\S+)\)',lab)
                    if lo:
                        llab = lo.groups()[0]
                        label_index = self.labels[llab]
                        # Resolve label to absolute byte address
                        instr.imm = label_index & 0xFFF # Bottom 12 bits
                        print(f"Resolving (low) address '{lab}' (label index {label_index}) to byte offset {instr.imm}")
                        continue
                    raise ValueError(f"Unknown label: {instr.imm}")

# Example of usage
if __name__ == "__main__":
    sasm = SAsm()

    pseudo_asm = """
    .start:
    LA x5, .end # set x5 = [.end]
    CSRRW x0, reconvpc, x5
    CSRRS x4, mhartid, x0
    ADDI x1, x0, 1
    
    BGT x4, x1, .if
    .else:
    ADDI x10, x0, 10
    ADDI x11, x0, 11
    JMP .end

    .if:
    ADDI x12, x0, 12
    ADDI x13, x0, 13
    ADDI x0, x0, 0
    JMP .end

    .end:
    ADDI x14, x0, 14
    ADDI x15, x0, 15
    ADDI x16, x0, 16
    ADDI x17, x0, 17
    """

    prog = sasm.compile(pseudo_asm)  # This will return the list of Instr objects
    for instr in prog:
        print(instr)  # Prints the list of Instr objects