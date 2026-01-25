#ifndef ISA_H
#define ISA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum Error_t {
  INVALID_PC = 0,
  INVALID_OPCODE,
  INVALID_MEM_READ,
  INVALID_MEM_WRITE,
  INVALID_EXECUTE
};

enum OPCODE {
  LUI = 0b0110111,
  AUIPC = 0b0010111,
  JAL = 0b1101111,
  JALR = 0b1100111,
  BRANCH = 0b1100011,
  LOAD = 0b0000011,
  STORE = 0b0100011,
  OP_IMM = 0b0010011,
  OP = 0b0110011,
  MISC_MEM = 0b0001111,
  SYSTEM = 0b1110011,
  AMO = 0b0101111
};

enum BRANCH_FUNC3 {
  FN3_BEQ = 0b000,
  FN3_BNE = 0b001,
  FN3_BLT = 0b100,
  FN3_BGE = 0b101,
  FN3_BLTU = 0b110,
  FN3_BGEU = 0b111
};

enum LOAD_FUNC3 {
  FN3_LB = 0b000,
  FN3_LH = 0b001,
  FN3_LW = 0b010,
  FN3_LBU = 0b100,
  FN3_LHU = 0b101
};

enum STORE_FUNC3 { FN3_SB = 0b000, FN3_SH = 0b001, FN3_SW = 0b010 };

enum OP_IMM_FUNC3 {
  FN3_ADDI = 0b000,
  FN3_SLTI = 0b010,
  FN3_SLTIU = 0b011,
  FN3_XORI = 0b100,
  FN3_ORI = 0b110,
  FN3_ANDI = 0b111,
  FN3_SLLI = 0b001,
  FN3_SRLI = 0b101
};

enum SRLI_FUNCT7 { FN7_SRLI = 0b0000000, FN7_SRAI = 0b0100000 };

enum OP_FUNC3 {
  FN3_ADD = 0b000,
  FN3_SLL = 0b001,
  FN3_SLT = 0b010,
  FN3_SLTU = 0b011,
  FN3_XOR = 0b100,
  FN3_SRL = 0b101,
  FN3_OR = 0b110,
  FN3_AND = 0b111
};

enum ADD_FUNC7 { FN7_ADD = 0b0000000, FN7_SUB = 0b0100000 };

enum SRL_FUNC7 { FN7_SRL = 0b0000000, FN7_SRA = 0b0100000 };

enum ECALL_FN12 {
  FN12_ECALL = 0b000000000000,
  FN12_EBREAK = 0b000000000001,
  FN12_SRET = 0b000100000010
};

enum CSRR_FN3 {
  FN3_CSRRW = 0b001,
  FN3_CSRRS = 0b010,
  FN3_CSRRC = 0b011,
  FN3_CSRRWI = 0b101,
  FN3_CSRRSI = 0b110,
  FN3_CSRRCI = 0b111
};

/* M Extension */
// M Extension (Reusing the funct3 bits)
#define FUNCT7_MUL_DIV_REM 0b0000001
#define FN3_MUL FN3_ADD
#define FN3_MULH FN3_SLL
#define FN3_MULHSU FN3_SLT
#define FN3_MULHU FN3_SLTU
#define FN3_DIV FN3_XOR
#define FN3_DIVU FN3_SRL
#define FN3_REM FN3_OR
#define FN3_REMU FN3_AND

/* A Extension */
#define FN5_AMOSWAP 0b00001

/* F Extension */

/* D Extension */

/* C Extension */

/* V Extension*/

enum MEM_OP_T {
  MEM_OP_NONE = 0,
  MEM_OP_WRITE,
  MEM_OP_READ_S,
  MEM_OP_READ_U,
};

typedef struct CSRs_t {
  uint32_t misa;
  uint32_t mvendorid;
  uint32_t marchid;
  uint32_t mimpid;
  uint32_t mhartid;
  uint32_t mstatus;
  uint32_t mtvec;
  uint32_t mie;
  uint32_t mip;
  uint32_t mcause;
  uint32_t mepc;
  uint32_t mscratch;
  uint32_t mtval;
} CSRs_t;

typedef struct CPU {
  uint32_t pc;
  uint32_t regs[32];
  CSRs_t csrs;
} CPU;

enum DISASEM_OP {
  D_lui = 0,
  D_auipc,
  D_addi,
  D_slti,
  D_sltiu,
  D_xori,
  D_ori,
  D_andi,
  D_slli,
  D_srli,
  D_srai,
  D_add,
  D_sub,
  D_sll,
  D_slt,
  D_sltu,
  D_xor,
  D_srl,
  D_sra,
  D_or,
  D_and,
  D_mul,
  D_mulh,
  D_mulhsu,
  D_mulhu,
  D_div,
  D_divu,
  D_rem,
  D_remu,
  D_fence,
  D_ecall,
  D_ebreak,
  D_sret,
  D_csrrw,
  D_csrrs,
  D_csrrc,
  D_csrrwi,
  D_csrrsi,
  D_csrrci,
  D_lb,
  D_lh,
  D_lw,
  D_lbu,
  D_lhu,
  D_sb,
  D_sh,
  D_sw,
  D_jal,
  D_jalr,
  D_beq,
  D_bne,
  D_blt,
  D_bge,
  D_bltu,
  D_bgeu,
  D_amoswap_aq,
  D_amoswap_rl,
};

static const char *reg_names[] = {
    "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
    "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
    "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

static const int _zero = 0;
static const int _ra = 1;
static const int _sp = 2;
static const int _gp = 3;
static const int _tp = 4;
static const int _t0 = 5;
static const int _t1 = 6;
static const int _t2 = 7;
static const int _s0 = 8;
static const int _s1 = 9;
static const int _a0 = 10;
static const int _a1 = 11;
static const int _a2 = 12;
static const int _a3 = 13;
static const int _a4 = 14;
static const int _a5 = 15;
static const int _a6 = 16;
static const int _a7 = 17;
static const int _s2 = 18;
static const int _s3 = 19;
static const int _s4 = 20;
static const int _s5 = 21;
static const int _s6 = 22;
static const int _s7 = 23;
static const int _s8 = 24;
static const int _s9 = 25;
static const int _s10 = 26;
static const int _s11 = 27;
static const int _t3 = 28;
static const int _t4 = 29;
static const int _t5 = 30;
static const int _t6 = 31;

typedef enum ALU_OP {
  ALU_ADD,
  ALU_SLL,
  ALU_SLT,
  ALU_SLTU,
  ALU_XOR,
  ALU_SRL,
  ALU_SRA,
  ALU_OR,
  ALU_AND,

  /* M Extension */
  ALU_MUL,
  ALU_MULH,
  ALU_MULHSU,
  ALU_MULHU,
  ALU_DIV,
  ALU_DIVU,
  ALU_REM,
  ALU_REMU,
} ALU_OP;

typedef enum CSRMMode {
  misa = 0x301,
  mvendorid = 0xF11,
  marchid = 0xF12,
  mimpid = 0xF13,
  mhartid = 0xF14,
  mstatus = 0x300,
  mtvec = 0x305,
  mie = 0x304,
  mip = 0x344,
  mcause = 0x342,
  mepc = 0x341,
  mscratch = 0x340,
  mtval = 0x343,
};

typedef enum InterruptCode {
  Supervisor_software_interrupt = 1,
  Machine_software_interrupt = 3,
  Supervisor_timer_interrupt = 5,
  Machine_timer_interrupt = 7,
  Supervisor_external_interrupt = 9,
  Machine_external_interrupt = 11,
  Counter_overflow_interrupt = 13,
};

typedef enum ExceptionCode {
  Instruction_address_misaligned = 0,
  Instruction_access_fault,
  Illegal_instruction,
  Breakpoint,
  Load_address_misaligned,
  Load_access_fault,
  Store_AMO_address_misaligned,
  Store_AMO_access_fault,
  Environment_call_from_U_mode,
  Environment_call_from_S_mode,
  Environment_call_from_M_mode = 11,
  Instruction_page_fault,
  Load_page_fault,
  Store_AMO_page_fault = 15,
  Double_trap,
  Software_check = 18,
  Hardware_error,
};

typedef struct cpu_history_t {
  CPU cpu;
  enum DISASEM_OP op;
  uint32_t params[3];
  bool valid;
} cpu_history_t;

static inline uint32_t *get_csr_ptr(CPU *cpu, uint16_t addr) {
  static uint32_t _unused_csr;
  _unused_csr = 0;
  switch (addr) {
  case misa:
    return &cpu->csrs.misa;
  case mvendorid:
    return &cpu->csrs.mvendorid;
  case marchid:
    return &cpu->csrs.marchid;
  case mimpid:
    return &cpu->csrs.mimpid;
  case mhartid:
    return &cpu->csrs.mhartid;
  case mstatus:
    return &cpu->csrs.mstatus;
  case mtvec:
    return &cpu->csrs.mtvec;
  case mie:
    return &cpu->csrs.mie;
  case mip:
    return &cpu->csrs.mip;
  case mcause:
    return &cpu->csrs.mcause;
  case mepc:
    return &cpu->csrs.mepc;
  case mscratch:
    return &cpu->csrs.mscratch;
  case mtval:
    return &cpu->csrs.mtval;
  default:
    return &_unused_csr;
  }
}

#endif // ISA_H