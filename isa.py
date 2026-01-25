from enum import Enum, auto
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
    ADD = auto()
    SUB = auto()
    MUL = auto()
    DIV = auto()
    BEQ = auto()
    BNE = auto()
    BLT = auto()
    BGT = auto()
    BLE = auto()
    BGE = auto()
    JMP = auto()
    ADDI = auto()
    CSRRS = auto()
    CSRRW = auto()
    AUIPC = auto()
    LUI = auto()

@dataclass
class Instr:
    op: Op = Op.ADD
    dst: int = 0
    src1: int = 0
    src2: int = 0
    imm: int = 0
    comment: str = ""