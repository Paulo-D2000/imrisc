#ifndef CPU_H
#define CPU_H

#include "breakpoints.h"
#include "config.h"
#include "isa.h"
#include "mmio.h"


#include <stdio.h>

extern void cpu_debug(const CPU *cpu);
extern bool cpu_error(const uint32_t pc, enum Error_t code);
extern void cpu_disasm(const CPU *cpu, const enum DISASEM_OP op,
                       uint32_t param0, uint32_t param1, uint32_t param2);

inline int32_t sign_extend(const uint32_t num, const uint8_t nbits) {
  int32_t sign_bit = (num >> (nbits - 1)) & 1; // Extract sign bit
  int32_t mask =
      -sign_bit; // Mask will be 0 if sign bit is 0, -1 if sign bit is 1
  return (num & ((1 << nbits) - 1)) |
         (mask << nbits); // Combine original number with sign extension
}

inline void trap(CPU *cpu) {
  uint32_t index = cpu->csrs.mcause & 0x7fffffff;
  bool isinterrupt = cpu->csrs.mcause & 0x80000000;
  uint32_t offset = isinterrupt ? 0 : 48;

  uint32_t addr = (cpu->csrs.mtvec & 0b11)
                      ? (cpu->csrs.mtvec & 0xfffffffc) + offset + (index << 2)
                      : cpu->csrs.mtvec;
  printf("[TRAP] Trap at %08x, jumping to %08x, Interrupt: %d\n", cpu->pc, addr,
         isinterrupt);

  // Switch index and print ExceptionCode/InterruptCode enum name for all enums!
  printf("[TRAP] Cause Code: %d\n", index);
  if (isinterrupt) {
    switch (index) {
    case Supervisor_software_interrupt:
      printf("  Interrupt: Supervisor_software_interrupt\n");
      break;
    case Machine_software_interrupt:
      printf("  Interrupt: Machine_software_interrupt\n");
      break;
    case Supervisor_timer_interrupt:
      printf("  Interrupt: Supervisor_timer_interrupt\n");
      break;
    case Machine_timer_interrupt:
      printf("  Interrupt: Machine_timer_interrupt\n");
      break;
    case Supervisor_external_interrupt:
      printf("  Interrupt: Supervisor_external_interrupt\n");
      break;
    case Machine_external_interrupt:
      printf("  Interrupt: Machine_external_interrupt\n");
      break;
    case Counter_overflow_interrupt:
      printf("  Interrupt: Counter_overflow_interrupt\n");
      break;
    default:
      printf("  Interrupt: Unknown (%d)\n", index);
      break;
    }
  } else {
    switch (index) {
    case Instruction_address_misaligned:
      printf("  Exception: Instruction_address_misaligned\n");
      break;
    case Instruction_access_fault:
      printf("  Exception: Instruction_access_fault\n");
      break;
    case Illegal_instruction:
      printf("  Exception: Illegal_instruction\n");
      break;
    case Breakpoint:
      printf("  Exception: Breakpoint\n");
      break;
    case Load_address_misaligned:
      printf("  Exception: Load_address_misaligned\n");
      break;
    case Load_access_fault:
      printf("  Exception: Load_access_fault\n");
      break;
    case Store_AMO_address_misaligned:
      printf("  Exception: Store_AMO_address_misaligned\n");
      break;
    case Store_AMO_access_fault:
      printf("  Exception: Store_AMO_access_fault\n");
      break;
    case Environment_call_from_U_mode:
      printf("  Exception: Environment_call_from_U_mode\n");
      break;
    case Environment_call_from_S_mode:
      printf("  Exception: Environment_call_from_S_mode\n");
      break;
    case Environment_call_from_M_mode:
      printf("  Exception: Environment_call_from_M_mode\n");
      break;
    case Instruction_page_fault:
      printf("  Exception: Instruction_page_fault\n");
      break;
    case Load_page_fault:
      printf("  Exception: Load_page_fault\n");
      break;
    case Store_AMO_page_fault:
      printf("  Exception: Store_AMO_page_fault\n");
      break;
    case Double_trap:
      printf("  Exception: Double_trap\n");
      break;
    case Software_check:
      printf("  Exception: Software_check\n");
      break;
    case Hardware_error:
      printf("  Exception: Hardware_error\n");
      break;
    default:
      printf("  Exception: Unknown (%d)\n", index);
      break;
    }
  }

  cpu->pc = addr;
}

static uint8_t last_opc;
static uint32_t last_inst;
static bool bcpu = false;

inline bool cpu_cycle(CPU *cpu) {
  cpu_debug(cpu);
#ifdef BREAKPOINTS_ENABLED
  for (size_t i = 0; i < ACTIVE_BREAKPOINTS; i++) {
    if (BREAKPOINTS[i].enabled && (BREAKPOINTS[i].address) == cpu->pc) {
      // BREAKPOINTS[i].enabled = false;
      printf("BREAKPOINT: %08x\n", cpu->pc);
      cpu_debug(cpu);
    }
  }
#endif

  bool jmp_exec = false;
  uint32_t new_pc = cpu->pc;

  bool writeback_reg = false;
  uint32_t new_reg = 0;

  enum MEM_OP_T mem_op = MEM_OP_NONE;
  uint32_t mem_ptr = 0;
  uint32_t mem_op_sz = 8;

  bool eq = false;
  bool alu_exec = false;
  bool alu_imm = false;
  bool alu_sub = false;
  enum ALU_OP alu_op = ALU_ADD;

  // fetch //
  mmio_result_t res = mmio_read(cpu->pc, _WORD);
  if (!res.valid) {
    return cpu_error(cpu->pc, INVALID_PC);
  }
  uint32_t inst = *(uint32_t *)(&res.data[0]);
  last_inst = inst;

  // decode //
  uint8_t opcode = inst & 0x7F;
  uint8_t rd = (inst >> 7) & 0x1F;
  uint32_t imm_31_12 = (inst >> 12);
  uint32_t imm_u;
  last_opc = opcode;

  uint8_t imm_4_0 = rd;
  uint8_t funct3 = imm_31_12 & 0x07;
  uint8_t rs1 = (imm_31_12 >> 3) & 0x1F;
  uint16_t imm_11_0 = imm_31_12 >> 8;
  uint8_t rs2 = imm_11_0 & 0x1F;
  uint8_t imm_11_5 = imm_11_0 >> 5;
  uint8_t funct7 = imm_11_5;
  uint8_t funct5 = funct7 >> 2;
  mmio_result_t t;
  switch (opcode) {
  case LUI:
    cpu_disasm(cpu, D_lui, rd, (int32_t)(imm_31_12 << 12), 0);
    new_reg = sign_extend(imm_31_12, 20) << 12;
    writeback_reg = true;
    break;

  case AUIPC:
    cpu_disasm(cpu, D_auipc, rd, (int32_t)(imm_31_12 << 12), 0);
    new_reg = (sign_extend(imm_31_12, 20) << 12) + cpu->pc;
    writeback_reg = true;
    break;

  case JAL:
    imm_u = ((inst & 0x7FE00000) >> 20) + ((inst & 0x00100000) >> 9) +
            (inst & 0x000FF800) + ((inst & 0x80000000) >> 11);
    cpu_disasm(cpu, D_jal, rd, sign_extend(imm_u, 20), 0);
    new_reg = cpu->pc + 4;
    new_pc += sign_extend(imm_u, 20);
    writeback_reg = true;
    jmp_exec = true;
    break;

  case JALR:
    cpu_disasm(cpu, D_jalr, rd, rs1, sign_extend(imm_11_0, 12));
    new_reg = cpu->pc + 4;
    new_pc = (sign_extend(imm_11_0, 12) + cpu->regs[rs1]) & (0xFFFFFFFF - 1);
    writeback_reg = true;
    jmp_exec = true;
    break;

  case BRANCH:
    imm_u = ((inst & 0x00000F00) >> 7) + ((inst & 0x7E000000) >> 20) +
            ((inst & 0x00000080) << 4) + ((inst & 0x80000000) >> 19);
    switch (funct3) {
    case FN3_BEQ:
      cpu_disasm(cpu, D_beq, rs1, rs2, sign_extend(imm_u, 12));
      eq = cpu->regs[rs1] == cpu->regs[rs2];
      new_pc = eq ? new_pc + sign_extend(imm_u, 13) : new_pc;
      jmp_exec = eq;
      break;

    case FN3_BNE:
      cpu_disasm(cpu, D_bne, rs1, rs2, sign_extend(imm_u, 12));
      eq = cpu->regs[rs1] != cpu->regs[rs2];
      new_pc = eq ? new_pc + sign_extend(imm_u, 13) : new_pc;
      jmp_exec = eq;
      break;

    case FN3_BLT:
      cpu_disasm(cpu, D_blt, rs1, rs2, sign_extend(imm_u, 12));
      eq = (int32_t)cpu->regs[rs1] < (int32_t)cpu->regs[rs2];
      new_pc = eq ? new_pc + sign_extend(imm_u, 13) : new_pc;
      jmp_exec = eq;
      break;

    case FN3_BLTU:
      cpu_disasm(cpu, D_bltu, rs1, rs2, sign_extend(imm_u, 12));
      eq = cpu->regs[rs1] < cpu->regs[rs2];
      new_pc = eq ? new_pc + sign_extend(imm_u, 13) : new_pc;
      jmp_exec = eq;
      break;

    case FN3_BGE:
      cpu_disasm(cpu, D_bge, rs1, rs2, sign_extend(imm_u, 12));
      eq = (int32_t)cpu->regs[rs1] >= (int32_t)cpu->regs[rs2];
      new_pc = eq ? new_pc + sign_extend(imm_u, 13) : new_pc;
      jmp_exec = eq;
      break;

    case FN3_BGEU:
      cpu_disasm(cpu, D_bgeu, rs1, rs2, sign_extend(imm_u, 12));
      eq = cpu->regs[rs1] >= cpu->regs[rs2];
      new_pc = eq ? new_pc + sign_extend(imm_u, 13) : new_pc;
      jmp_exec = eq;
      break;

    default:
      break;
    }
    break;

  case LOAD:
    switch (funct3) {
    case FN3_LB:
      cpu_disasm(cpu, D_lb, rd, sign_extend(imm_11_0, 12), rs1);
      mem_op = MEM_OP_READ_S;
      mem_op_sz = 8;
      mem_ptr = cpu->regs[rs1] + sign_extend(imm_11_0, 12);
      break;

    case FN3_LBU:
      cpu_disasm(cpu, D_lbu, rd, sign_extend(imm_11_0, 12), rs1);
      mem_op = MEM_OP_READ_U;
      mem_op_sz = 8;
      mem_ptr = cpu->regs[rs1] + sign_extend(imm_11_0, 12);
      break;

    case FN3_LH:
      cpu_disasm(cpu, D_lh, rd, sign_extend(imm_11_0, 12), rs1);
      mem_op = MEM_OP_READ_S;
      mem_op_sz = 16;
      mem_ptr = cpu->regs[rs1] + sign_extend(imm_11_0, 12);
      break;

    case FN3_LHU:
      cpu_disasm(cpu, D_lhu, rd, sign_extend(imm_11_0, 12), rs1);
      mem_op = MEM_OP_READ_U;
      mem_op_sz = 16;
      mem_ptr = cpu->regs[rs1] + sign_extend(imm_11_0, 12);
      break;

    case FN3_LW:
      cpu_disasm(cpu, D_lw, rd, sign_extend(imm_11_0, 12), rs1);
      mem_op = MEM_OP_READ_S;
      mem_op_sz = 32;
      mem_ptr = cpu->regs[rs1] + sign_extend(imm_11_0, 12);
      break;
    default:
      break;
    }
    break;

  case STORE:
    switch (funct3) {
    case FN3_SB:
      cpu_disasm(cpu, D_sb, rs1, rs2, sign_extend(imm_11_5 << 5 | imm_4_0, 12));
      mem_op = MEM_OP_WRITE;
      mem_op_sz = 8;
      mem_ptr = cpu->regs[rs1] + sign_extend(imm_11_5 << 5 | imm_4_0, 12);
      new_reg = cpu->regs[rs2];
      break;

    case FN3_SH:
      cpu_disasm(cpu, D_sh, rs1, rs2, sign_extend(imm_11_5 << 5 | imm_4_0, 12));
      mem_op = MEM_OP_WRITE;
      mem_op_sz = 16;
      mem_ptr = cpu->regs[rs1] + sign_extend(imm_11_5 << 5 | imm_4_0, 12);
      new_reg = cpu->regs[rs2];
      break;

    case FN3_SW:
      cpu_disasm(cpu, D_sw, rs1, rs2, sign_extend(imm_11_5 << 5 | imm_4_0, 12));
      mem_op = MEM_OP_WRITE;
      mem_op_sz = 32;
      mem_ptr = cpu->regs[rs1] + sign_extend(imm_11_5 << 5 | imm_4_0, 12);
      new_reg = cpu->regs[rs2];
      break;
    default:
      break;
    }
    break;

  case OP_IMM:
    switch (funct3) {
    case FN3_ADDI:
      cpu_disasm(cpu, D_addi, rd, rs1, sign_extend(imm_11_0, 12));
      alu_op = ALU_ADD;
      break;

    case FN3_SLTI:
      cpu_disasm(cpu, D_slti, rd, rs1, sign_extend(imm_11_0, 12));
      alu_op = ALU_SLT;
      break;

    case FN3_SLTIU:
      cpu_disasm(cpu, D_sltiu, rd, rs1, sign_extend(imm_11_0, 12));
      alu_op = ALU_SLTU;
      break;

    case FN3_XORI:
      cpu_disasm(cpu, D_xori, rd, rs1, sign_extend(imm_11_0, 12));
      alu_op = ALU_XOR;
      break;

    case FN3_ORI:
      cpu_disasm(cpu, D_ori, rd, rs1, sign_extend(imm_11_0, 12));
      alu_op = ALU_OR;
      break;

    case FN3_ANDI:
      cpu_disasm(cpu, D_andi, rd, rs1, sign_extend(imm_11_0, 12));
      alu_op = ALU_AND;
      break;

    case FN3_SLLI:
      cpu_disasm(cpu, D_slli, rd, rs1, sign_extend(imm_11_0, 12));
      alu_op = ALU_SLL;
      break;

    case FN3_SRLI:
      switch (imm_11_5) {
      case FN7_SRLI:
        cpu_disasm(cpu, D_srli, rd, rs1, rs2);
        alu_op = ALU_SRL;
        break;

      case FN7_SRAI:
        cpu_disasm(cpu, D_srai, rd, rs1, (int8_t)rs2);
        alu_op = ALU_SRA;
        break;

      default:
        break;
      }
      break;

    default:
      break;
    }
    alu_exec = true;
    alu_imm = true;
    break;

  case OP:
    alu_exec = true;
    switch (funct7) {
    case FUNCT7_MUL_DIV_REM:
      switch (funct3) {
      case FN3_MUL:
        cpu_disasm(cpu, D_mul, rd, rs1, rs2);
        alu_op = ALU_MUL;
        break;

      case FN3_MULH:
        cpu_disasm(cpu, D_mulh, rd, rs1, rs2);
        alu_op = ALU_MULH;
        break;

      case FN3_MULHSU:
        cpu_disasm(cpu, D_mulhsu, rd, rs1, rs2);
        alu_op = ALU_MULHSU;
        break;

      case FN3_MULHU:
        cpu_disasm(cpu, D_mulhu, rd, rs1, rs2);
        alu_op = ALU_MULHU;
        break;

      case FN3_DIV:
        cpu_disasm(cpu, D_div, rd, rs1, rs2);
        alu_op = ALU_DIV;
        break;

      case FN3_DIVU:
        cpu_disasm(cpu, D_divu, rd, rs1, rs2);
        alu_op = ALU_DIVU;
        break;

      case FN3_REM:
        cpu_disasm(cpu, D_rem, rd, rs1, rs2);
        alu_op = ALU_REM;
        break;

      case FN3_REMU:
        cpu_disasm(cpu, D_remu, rd, rs1, rs2);
        alu_op = ALU_REMU;
        break;

      default:
        break;
      }
      break;

    default:
      switch (funct3) {
      case FN3_ADD:
        switch (funct7) {
        case FN7_ADD:
          cpu_disasm(cpu, D_add, rd, rs1, rs2);
          alu_op = ALU_ADD;
          break;

        case FN7_SUB:
          cpu_disasm(cpu, D_sub, rd, rs1, rs2);
          alu_op = ALU_ADD;
          alu_sub = true;
          break;

        default:
          break;
        }
        break;

      case FN3_SLL:
        cpu_disasm(cpu, D_sll, rd, rs1, rs2);
        alu_op = ALU_SLL;
        break;

      case FN3_SLT:
        cpu_disasm(cpu, D_slt, rd, rs1, rs2);
        alu_op = ALU_SLT;
        break;

      case FN3_SLTU:
        cpu_disasm(cpu, D_sltu, rd, rs1, rs2);
        alu_op = ALU_SLTU;
        break;

      case FN3_XOR:
        cpu_disasm(cpu, D_xor, rd, rs1, rs2);
        alu_op = ALU_XOR;
        break;

      case FN3_SRL:
        switch (funct7) {
        case FN7_SRL:
          cpu_disasm(cpu, D_srl, rd, rs1, rs2);
          alu_op = ALU_SRL;
          break;

        case FN7_SRA:
          cpu_disasm(cpu, D_sra, rd, rs1, rs2);
          alu_op = ALU_SRA;
          break;

        default:
          break;
        }
        break;

      case FN3_OR:
        cpu_disasm(cpu, D_or, rd, rs1, rs2);
        alu_op = ALU_OR;
        break;

      case FN3_AND:
        cpu_disasm(cpu, D_and, rd, rs1, rs2);
        alu_op = ALU_AND;
        break;

      default:
        break;
      }
      break;
    }
    break;

  case MISC_MEM:
    cpu_disasm(cpu, D_fence, 0, 0, 0);
    break;

  case SYSTEM:
    switch (funct3) {
    case 0:
      switch (imm_11_0) {
      case FN12_ECALL:
        cpu_disasm(cpu, D_ecall, 0, 0, 0);
        cpu->csrs.mepc = cpu->pc;
        cpu->csrs.mcause = 11;
        cpu->csrs.mtval = 0;
        trap(cpu);
        return 1;
        break;

      case FN12_EBREAK:
        cpu_disasm(cpu, D_ebreak, rd, rs1, imm_11_0);
        cpu->csrs.mepc = cpu->pc;
        cpu->csrs.mcause = 3;
        cpu->csrs.mtval = 0;
        trap(cpu);
        return 1;
        break;

      case FN12_SRET:
        cpu_disasm(cpu, D_sret, 0, 0, 0);
        cpu->csrs.mepc = cpu->pc;
        cpu->csrs.mcause = 3;
        cpu->csrs.mtval = 0;
        trap(cpu);
        return 1;
        break;

      default:
        break;
      }
      break;
    case FN3_CSRRW:
      cpu_disasm(cpu, D_csrrw, rd, rs1, imm_11_0);
      new_reg = *get_csr_ptr(cpu, imm_11_0);
      *get_csr_ptr(cpu, imm_11_0) = cpu->regs[rs1];
      writeback_reg = true;
      break;
    case FN3_CSRRS:
      cpu_disasm(cpu, D_csrrs, rd, rs1, imm_11_0);
      new_reg = *get_csr_ptr(cpu, imm_11_0);
      *get_csr_ptr(cpu, imm_11_0) = new_reg | cpu->regs[rs1];
      writeback_reg = true;
      break;
    case FN3_CSRRC:
      cpu_disasm(cpu, D_csrrc, rd, rs1, imm_11_0);
      new_reg = *get_csr_ptr(cpu, imm_11_0);
      *get_csr_ptr(cpu, imm_11_0) = new_reg & !cpu->regs[rs1];
      writeback_reg = true;
      break;
    case FN3_CSRRWI:
      cpu_disasm(cpu, D_csrrwi, rd, rs1, imm_11_0);
      new_reg = *get_csr_ptr(cpu, imm_11_0);
      *get_csr_ptr(cpu, imm_11_0) = rs1;
      writeback_reg = true;
      break;
    case FN3_CSRRSI:
      cpu_disasm(cpu, D_csrrsi, rd, rs1, imm_11_0);
      new_reg = *get_csr_ptr(cpu, imm_11_0);
      *get_csr_ptr(cpu, imm_11_0) = new_reg | rs1;
      writeback_reg = true;
      break;
    case FN3_CSRRCI:
      cpu_disasm(cpu, D_csrrci, rd, rs1, imm_11_0);
      new_reg = *get_csr_ptr(cpu, imm_11_0);
      *get_csr_ptr(cpu, imm_11_0) = new_reg & !rs1;
      writeback_reg = true;
      break;

    default:
      break;
    }
    break;

  case AMO:
    switch (funct5) {
    case FN5_AMOSWAP:
      // x[rd] = AMO32(M[x[rs1]] SWAP x[rs2])
      if (funct7 & 0b10)
        cpu_disasm(cpu, D_amoswap_aq, rd, rs1, rs2);
      else if (funct7 & 0b01)
        cpu_disasm(cpu, D_amoswap_rl, rd, rs1, rs2);
      else
        return cpu_error(cpu->pc, INVALID_EXECUTE);
      res = mmio_read(cpu->regs[rs1], _WORD);
      if (!res.valid) {
        return cpu_error(cpu->pc, INVALID_MEM_READ);
      }
      new_reg = mmio_to_word(res);
      res = mmio_write_32(cpu->regs[rs1], cpu->regs[rs2]);
      if (!res.valid) {
        return cpu_error(cpu->pc, INVALID_MEM_READ);
      }
      writeback_reg = true;
      break;

    default:
      break;
    }
    break;

  default:
    return cpu_error(cpu->pc, INVALID_OPCODE);
  }
#ifdef BREAKPOINTS_ENABLED
  if (bcpu) {
    last_opc = last_opc;
  }
#endif
  // memory-access //
  switch (mem_op) {
  case MEM_OP_NONE:
    break;

  case MEM_OP_READ_S:
    res = mmio_read(mem_ptr, (mmio_size_t)(mem_op_sz / 8));
    if (!res.valid) {
      return cpu_error(cpu->pc, INVALID_MEM_READ);
    }
    switch (mem_op_sz) {
    case 8:
      new_reg = sign_extend(res.data[0] & 0xFF, 8);
      break;

    case 16:
      new_reg = sign_extend(mmio_to_short(res) & 0xFFFF, 16);
      break;

    case 32:
      new_reg = (int32_t)mmio_to_word(res);
      break;

    default:
      break;
    }
    writeback_reg = true;
    break;

  case MEM_OP_READ_U:
    res = mmio_read(mem_ptr, (mmio_size_t)(mem_op_sz / 8));
    if (!res.valid) {
      return cpu_error(cpu->pc, INVALID_MEM_READ);
    }
    new_reg = mmio_to_word(res) & ((1 << mem_op_sz) - 1);
    writeback_reg = true;
    break;

  case MEM_OP_WRITE:
    switch (mem_op_sz) {
    case 8:
      res = mmio_write_8(mem_ptr, new_reg & 0xFF);
      if (!res.valid) {
        return cpu_error(cpu->pc, INVALID_MEM_WRITE);
      }
      break;

    case 16:
      res = mmio_write_16(mem_ptr, new_reg & 0xFFFF);
      if (!res.valid) {
        return cpu_error(cpu->pc, INVALID_MEM_WRITE);
      }
      break;

    case 32:
      res = mmio_write_32(mem_ptr, new_reg);
      if (!res.valid) {
        return cpu_error(cpu->pc, INVALID_MEM_WRITE);
      }
      break;

    default:
      break;
    }
    break;

  default:
    break;
  }

  if (cpu->pc == 0x4010005c || new_pc == 0x4010005c) {
    printf("FOUND <exit>... stopping...\n");
    return false;
  }

  // execute //
  if (jmp_exec) {
    if (new_pc == 0) {
      printf("ERROR JUMPING  TO 0\n");
    }
    cpu->pc = new_pc;
  } else {
    if (alu_exec) {
      switch (alu_op) {
      case ALU_ADD:
        if (alu_imm) {
          new_reg = sign_extend(imm_11_0, 12) + cpu->regs[rs1];
        } else {
          if (alu_sub) {
            new_reg = cpu->regs[rs1] - cpu->regs[rs2];
          } else {
            new_reg = cpu->regs[rs1] + cpu->regs[rs2];
          }
        }
        break;

      case ALU_SLL:
        if (alu_imm) {
          new_reg = cpu->regs[rs1] << rs2;
        } else {
          new_reg = cpu->regs[rs1] << cpu->regs[rs2];
        }
        break;

      case ALU_SLT:
        if (alu_imm) {
          new_reg = (int32_t)cpu->regs[rs1] < sign_extend(imm_11_0, 12);
        } else {
          new_reg = (int32_t)cpu->regs[rs1] < (int32_t)cpu->regs[rs2];
        }
        break;

      case ALU_SLTU:
        if (alu_imm) {
          new_reg = cpu->regs[rs1] < (uint32_t)sign_extend(imm_11_0, 12);
        } else {
          new_reg = cpu->regs[rs1] < cpu->regs[rs2];
        }
        break;

      case ALU_XOR:
        if (alu_imm) {
          new_reg = cpu->regs[rs1] ^ sign_extend(imm_11_0, 12);
        } else {
          new_reg = cpu->regs[rs1] ^ cpu->regs[rs2];
        }
        break;

      case ALU_SRL:
        if (alu_imm) {
          new_reg = cpu->regs[rs1] >> rs2;
        } else {
          new_reg = cpu->regs[rs1] >> cpu->regs[rs2];
        }
        break;

      case ALU_SRA:
        if (alu_imm) {
          new_reg = (uint32_t)((int32_t)cpu->regs[rs1] >> (int8_t)rs2);
        } else {
          new_reg =
              (uint32_t)((int32_t)cpu->regs[rs1] >> (int32_t)cpu->regs[rs2]);
        }
        break;

      case ALU_OR:
        if (alu_imm) {
          new_reg = cpu->regs[rs1] | sign_extend(imm_11_0, 12);
        } else {
          new_reg = cpu->regs[rs1] | cpu->regs[rs2];
        }
        break;

      case ALU_AND:
        if (alu_imm) {
          new_reg = cpu->regs[rs1] & sign_extend(imm_11_0, 12);
        } else {
          new_reg = cpu->regs[rs1] & cpu->regs[rs2];
        }
        break;

      case ALU_MUL:
        new_reg = (int32_t)cpu->regs[rs1] * (int32_t)cpu->regs[rs2];
        break;

      case ALU_MULHSU:
        new_reg = ((int64_t)((int32_t)cpu->regs[rs1]) *
                   (uint64_t)((uint32_t)cpu->regs[rs2])) >>
                  32;
        break;

      case ALU_MULHU:
        new_reg = ((uint64_t)cpu->regs[rs1] * (uint64_t)cpu->regs[rs2]) >> 32;
        break;

      case ALU_MULH:
        new_reg = ((int64_t)((int32_t)cpu->regs[rs1]) *
                   (int64_t)((int32_t)cpu->regs[rs2])) >>
                  32;
        break;

      case ALU_DIV:
        new_reg = (int32_t)cpu->regs[rs1] / (int32_t)cpu->regs[rs2];
        break;

      case ALU_DIVU:
        new_reg = cpu->regs[rs1] / cpu->regs[rs2];
        break;

      case ALU_REM:
        new_reg = (int32_t)cpu->regs[rs1] % (int32_t)cpu->regs[rs2];
        break;

      case ALU_REMU:
        new_reg = cpu->regs[rs1] % cpu->regs[rs2];
        break;

      default:
        break;
      }
      writeback_reg = true;
    }
    cpu->pc += 4;
  }

  // write-back-regs //
  if (writeback_reg && rd != 0) {
    cpu->regs[rd] = new_reg;
  }

  return true;
}

#endif // CPU_H