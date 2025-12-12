from enum import Enum
from dataclasses import dataclass

class CSR(Enum):
    mhartid = 0xF14
    reconvpc = 0xBC0

class RegisterFile:
    def __init__(self, tid):
        self.regs = [0 for _ in range(32)]
        self.csrs = [0 for _ in range(4096)]
        self.csrs[CSR.mhartid.value] = tid

class Op(Enum):
    ADD = 0
    SUB = 1
    MUL = 2
    DIV = 3
    BEQ = 4
    BNE = 5
    BLT = 6
    BGT = 7
    BLE = 8
    BGE = 9
    JMP = 10
    ADDI = 11
    CSRRS = 12
    AUIPC = 13
    LUI = 14

@dataclass
class Instr:
    op: Op = Op.ADD
    dst: int = 0
    src1: int = 0
    src2: int = 0
    imm: int = 0
    comment: str = ""