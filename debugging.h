#ifndef DEBUG_H
#define DEBUG_H

#include "cpu.h"
#include "symbol_parser.h"
#include "timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool jump_cycles = true;

inline void uart_printf() {
  static bool start = true;
  static char buffer[0xFF] = {0};
  static size_t i = 0;
  if (start) {
    i = 0;
    start = false;
    if (strlen(buffer) > 0) {
      printf("[UART]: %s", buffer);
    }
    memset(buffer, 0, 0xFF);
  }
  if (UART[0] != 0 && uart_newbyte) {
    uart_newbyte = false;
    // bug fix
    if (i == 255) {
      i = 0;
    }
    buffer[i++] = UART[0];
    if (UART[0] == '\n')
      start = true;
  }
}

inline void spi_printf(uint32_t addr, bool rw) {
  uint32_t value = SPI[addr - SPI_ADDR];
  printf("[SPI] ");
  if (rw == 0) {

    switch (addr - SPI_ADDR) {
    case 0: // SPI_CSR
      printf("Read SPI_CSR\n");
      break;

    case 0xC: // SPI_RF
      printf("Write SPI_RF\n");
      break;

    case 0x40:
      printf("Read SPI Mode\n");
      break;

    case 0x74:
      printf("Read QSPI \n");
      break;

    default:
      printf("Read %08x\n", addr);
      break;
    }
  } else {
    switch (addr - SPI_ADDR) {
    case 0: // SPI_CSR
      printf("Write SPI_CSR = %08x\n", value);
      break;
    case 0xC: // SPI_RF
      printf("Write SPI_RF = %08x\n", value);
      break;
    case 0x40:
      printf("Set SPI Mode %08x\n", value);
      break;
    case 0x74:
      printf("Set QSPI Parameters %08x\n", value);
      break;
    default:
      printf("Write %08x %08x %02x\n", addr, value, 0xFF);
      break;
    }
  }
}

#ifndef DEBUG_HIST_SIZE
#define DEBUG_HIST_SIZE 32
#endif
extern cpu_history_t DebugHistory[DEBUG_HIST_SIZE];
extern size_t History_PTR;

inline void update_history(const CPU *cpu, const enum DISASEM_OP op,
                           uint32_t param0, uint32_t param1, uint32_t param2) {
  DebugHistory[History_PTR].cpu = *cpu;
  DebugHistory[History_PTR].op = op;
  DebugHistory[History_PTR].params[0] = param0;
  DebugHistory[History_PTR].params[1] = param1;
  DebugHistory[History_PTR].params[2] = param2;
  DebugHistory[History_PTR].valid = true;
  History_PTR = (History_PTR + 1) % DEBUG_HIST_SIZE;
}

inline void cpu_print_regs(const CPU *cpu) {
  printf("-------- REGS at %08x --------\n  ", cpu->pc);
  for (size_t i = 0; i < 32; i++) {
    char *space = "  ";
    if (i % 8 == 0 && i > 0)
      printf("\n  ");
    if (i / 8 + 4 * (i % 8) < 10) {
      space = "   ";
    }
    if (i / 8 + 4 * (i % 8) == 0 || strlen(reg_names[i / 8 + 4 * (i % 8)]) == 3)
      space = " ";
    printf("X%lu(%s):%s0x%08x ", i / 8 + 4 * (i % 8),
           reg_names[i / 8 + 4 * (i % 8)], space,
           cpu->regs[i / 8 + 4 * (i % 8)]);
  }
  printf("\n-------------------------------\n");
}

inline bool symbol_table_lookup(uint32_t address, char **symbol) {
  for (size_t i = 0; i < SYMBOL_TABLE_SIZE; i++) {
    if (SYMBOL_TABLE[i].address == address) {
      *symbol = SYMBOL_TABLE[i].name;
      return 1;
    }
  }
  return 0;
}

inline void cpu_debug_sym(const CPU *cpu) {
  // if(cpu->pc < 0x40100050){ return; }
  char *symname = "";
  if (symbol_table_lookup(cpu->pc, &symname)) {
    printf("%08x %s\n", cpu->pc, symname);
  }
  return;
}

inline const char *format(const char *fmt, ...) {
  static __thread char buffer[1024];

  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  return buffer;
}

inline const char *cpu_fmt_asm_internal(const enum DISASEM_OP op,
                                        uint32_t param0, uint32_t param1,
                                        uint32_t param2) {
  const char *ret = "";
  switch (op) {
  case D_lui:
    ret = format("lui %s, %d '%x'\n", reg_names[param0], param1, param1);
    break;
  case D_auipc:
    ret = format("auipc %s, %d\n", reg_names[param0], param1);
    break;
  case D_addi:
    ret = format("addi %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_slti:
    ret = format("slti %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_sltiu:
    ret = format("sltiu %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_xori:
    ret = format("xori %s, %s, %d\n", reg_names[param0], reg_names[param1], param2);
    break;
  case D_ori:
    ret = format("ori %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_andi:
    ret = format("andi %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_slli:
    ret = format("slli %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_srli:
    ret = format("srli %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_srai:
    ret = format("srai %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_add:
    ret = format("add %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_sub:
    ret = format("sub %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_sll:
    ret = format("sll %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_slt:
    ret = format("slt %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_sltu:
    ret = format("sltu %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_xor:
    ret = format("xor %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_srl:
    ret = format("srl %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_sra:
    ret = format("sra %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_or:
    ret = format("or %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_and:
    ret = format("and %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_mul:
    ret = format("mul %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_mulh:
    ret = format("mulh %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_mulhsu:
    ret = format("mulhsu %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_mulhu:
    ret = format("mulhu %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_div:
    ret = format("div %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_divu:
    ret = format("divu %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_rem:
    ret = format("rem %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_remu:
    ret = format("remu %s, %s, %s\n", reg_names[param0], reg_names[param1],
                 reg_names[param2]);
    break;
  case D_fence:
    ret = format("fence %s, %d\n", reg_names[param0], param1);
    break;
  case D_ecall:
    ret = format("ecall\n");
    break;
  case D_ebreak:
    ret = format("ebreak\n");
    break;
  case D_sret:
    ret = format("sret\n");
    break;
  case D_csrrw:
    ret = format("csrrw %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_csrrs:
    ret = format("csrrs %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_csrrc:
    ret = format("csrrc %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_csrrwi:
    ret = format("csrrwi %s, %d, %d\n", reg_names[param0], param1, param2);
    break;
  case D_csrrsi:
    ret = format("csrrsi %s, %d, %d\n", reg_names[param0], param1, param2);
    break;
  case D_csrrci:
    ret = format("csrrci %s, %d, %d\n", reg_names[param0], param1, param2);
    break;
  case D_lb:
    ret =
        format("lb %s, %d(%s)\n", reg_names[param0], param1, reg_names[param2]);
    break;
  case D_lh:
    ret =
        format("lh %s, %d(%s)\n", reg_names[param0], param1, reg_names[param2]);
    break;
  case D_lw:
    ret =
        format("lw %s, %d(%s)\n", reg_names[param0], param1, reg_names[param2]);
    break;
  case D_lbu:
    ret = format("lbu %s, %d(%s)\n", reg_names[param0], param1,
                 reg_names[param2]);
    break;
  case D_lhu:
    ret = format("lhu %s, %d(%s)\n", reg_names[param0], param1,
                 reg_names[param2]);
    break;
  case D_sb:
    ret =
        format("sb %s, %d(%s)\n", reg_names[param1], param2, reg_names[param0]);
    break;
  case D_sh:
    ret =
        format("sh %s, %d(%s)\n", reg_names[param1], param2, reg_names[param0]);
    break;
  case D_sw:
    ret =
        format("sw %s, %d(%s)\n", reg_names[param1], param2, reg_names[param0]);
    break;
  case D_jal:
    ret = format("jal %s, %d\n", reg_names[param0], param1);
    break;
  case D_jalr:
    ret = format("jalr %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_beq:
    ret = format("beq %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_bne:
    ret = format("bne %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_blt:
    ret = format("blt %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_bge:
    ret = format("bge %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_bltu:
    ret = format("bltu %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_bgeu:
    ret = format("bgeu %s, %s, %d\n", reg_names[param0], reg_names[param1],
                 param2);
    break;
  case D_amoswap_aq:
    ret = format("amoswap.w.aq %s, %s, 0(%s)\n", reg_names[param0],
                 reg_names[param1], reg_names[param2]);
    break;
  case D_amoswap_rl:
    ret = format("amoswap.w.rl %s, %s, 0(%s)\n", reg_names[param0],
                 reg_names[param1], reg_names[param2]);
    break;

  default:
    break;
  }

  return ret;
}

inline void cpu_print_asm(const CPU *cpu, const enum DISASEM_OP op,
                          uint32_t param0, uint32_t param1, uint32_t param2) {
  cpu_debug_sym(cpu);
  printf("  %08x: ", cpu->pc);
  printf("%s", cpu_fmt_asm_internal(op, param0, param1, param2));
}

#define op_special(op)                                                         \
  op != D_ebreak &&op != D_ecall &&op != D_fence &&op != D_amoswap_aq &&op !=  \
      D_amoswap_rl
/*
op != D_csrrw  && \
op != D_csrrs  && \
op != D_csrrc  && \
op != D_csrrwi && \
op != D_csrrsi && \
op != D_csrrci && \
*/

#ifdef DEBUG_REGS
inline void cpu_debug(const CPU *cpu) {
  if (jump_cycles) {
    return;
  }
  bcpu = true; /*if(cpu->pc < 0x40100050){ return; }*/
  cpu_print_regs(cpu);
}
#else
inline void cpu_debug(const CPU *cpu) {
  if (jump_cycles) {
    return;
  }
  // if(cpu->pc < 0x40100050){ return; }
  // symbol_table_lookup(cpu->pc);
  cpu_debug_sym(cpu);
}
#endif

inline void enable_Debug() { jump_cycles = false; }

#ifdef DEBUG_ASM
inline void cpu_disasm(const CPU *cpu, const enum DISASEM_OP op,
                       uint32_t param0, uint32_t param1, uint32_t param2) {
  if (jump_cycles && op_special(op)) {
    return;
  } /*if(cpu->pc < 0x40100050){ return; }*/
  cpu_print_asm(cpu, op, param0, param1, param2);
}
#else
inline void cpu_disasm(const CPU *cpu, const enum DISASEM_OP op,
                       uint32_t param0, uint32_t param1, uint32_t param2) {
  update_history(cpu, op, param0, param1, param2);
}
#endif

inline bool cpu_error(const uint32_t pc, enum Error_t code) {
  size_t end = History_PTR;
  if (end == 0) {
    end = DEBUG_HIST_SIZE - 1;
  }
  for (size_t i = 0; i < end; i++) {
    if (!DebugHistory[i].valid)
      continue;
    cpu_print_regs(&DebugHistory[i].cpu);
    cpu_print_asm(&DebugHistory[i].cpu, DebugHistory[i].op,
                  DebugHistory[i].params[0], DebugHistory[i].params[1],
                  DebugHistory[i].params[2]);
  }
  printf("[PC: 0x%08x][ERROR] ", pc);
  switch (code) {
  case INVALID_PC:
    printf("Invalid PC! Pc (0x%08x) was outside the memory region!\n", pc);
    break;

  case INVALID_OPCODE:
    printf(
        "Invalid OPCODE! The opcode (%d) of instruction (%08x) isn't known!\n",
        last_opc, last_inst);
    break;

  case INVALID_MEM_READ:
    printf("Invalid Memory Read! Address [0x%08x] was outside the memory "
           "region!\n",
           last_addr);
    break;

  case INVALID_MEM_WRITE:
    printf("Invalid Memory Write! Address [0x%08x] was outside the memory "
           "region!\n",
           last_addr);
    break;

  case INVALID_EXECUTE:
    printf(
        "Invalid Execute! Something went wrong executing the instruction!\n");
    break;
  }
  return false;
}

#endif // DEBUG_H