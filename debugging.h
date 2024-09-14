#include "cpu.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "symbol_parser.h"
#include "timer.h"

static bool jump_cycles = true;

void uart_printf(){
    static bool start = true;
    static char buffer[0xFF] = {0};
    static size_t i = 0;
    if(start){
        i = 0;
        start = false;
        if(strlen(buffer) > 0){
            printf("[UART]: %s", buffer);
        }
        memset(buffer, 0, 0xFF);
    }
    if(UART[0] != 0  && uart_newbyte){
        uart_newbyte = false;
        buffer[i++] = UART[0];
        if(UART[0] == '\n') start = true;
    }
}

void spi_printf(uint32_t addr, bool rw){
    uint32_t value = SPI[addr - SPI_ADDR];
    printf("[SPI] ");
    if(rw == 0){

        switch(addr - SPI_ADDR)
        {
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
    }
    else {
        switch(addr - SPI_ADDR)
        {
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

typedef struct cpu_history_t{
    CPU cpu;
    enum DISASEM_OP op;
    uint32_t params[3];
    bool valid;
}cpu_history_t;


#ifndef DEBUG_HIST_SIZE
#define DEBUG_HIST_SIZE 64
#endif
static cpu_history_t DebugHistory[DEBUG_HIST_SIZE] = {0};
static size_t History_PTR = 0;

void update_history(const CPU* cpu, const enum DISASEM_OP op, uint32_t param0, uint32_t param1, uint32_t param2){
    DebugHistory[History_PTR].cpu = *cpu;
    DebugHistory[History_PTR].op = op;
    DebugHistory[History_PTR].params[0] = param0;
    DebugHistory[History_PTR].params[1] = param1;
    DebugHistory[History_PTR].params[2] = param2;
    DebugHistory[History_PTR].valid = true;
    History_PTR = (History_PTR + 1) % DEBUG_HIST_SIZE;
}

void cpu_print_regs(const CPU* cpu){
    printf("-------- REGS at %08x --------\n  ", cpu->pc);
    for (size_t i = 0; i < 32; i++)
    {
        char* space = "  ";
        if(i%8 == 0 && i>0) printf("\n  ");
        if(i/8 + 4*(i%8)<10){
            space = "   ";
        }
        if(i/8 + 4*(i%8) == 0 || strlen(reg_names[i/8 + 4*(i%8)]) == 3) space = " ";
        printf("X%lu(%s):%s0x%08x ", i/8 + 4*(i%8), reg_names[i/8 + 4*(i%8)], space, cpu->regs[i/8 + 4*(i%8)]);
    }
    printf("\n-------------------------------\n");
}

inline bool symbol_table_lookup(uint32_t address, char** symbol){
    for (size_t i = 0; i < SYMBOL_TABLE_SIZE; i++)
    {
        if(SYMBOL_TABLE[i].address == address){
            *symbol = SYMBOL_TABLE[i].name;
            return 1;
        }
    }
    return 0;
}

void cpu_debug_sym(const CPU* cpu){
    //if(cpu->pc < 0x40100050){ return; }
    char* symname = "";
    if(symbol_table_lookup(cpu->pc, &symname)){
        printf("%08x %s\n", cpu->pc, symname);
    }
    return; 
}


void cpu_print_asm(const CPU* cpu, const enum DISASEM_OP op, uint32_t param0, uint32_t param1, uint32_t param2){
    cpu_debug_sym(cpu);
    printf("  %08x: ", cpu->pc);
    switch (op)
    {
    case D_lui:
        printf("lui %s, %d '%x'\n", reg_names[param0], param1, param1);
        break;
    case D_auipc:
        printf("auipc %s, %d\n", reg_names[param0], param1);
        break;
    case D_addi:
        printf("addi %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_slti:
        printf("slti %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_sltiu:
        printf("sltiu %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_xori:
        printf("xori %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_ori:
        printf("ori %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_andi:
        printf("andi %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_slli:
        printf("slli %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_srli:
        printf("srli %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_srai:
        printf("srai %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_add:
        printf("add %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_sub:
        printf("sub %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_sll:
        printf("sll %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_slt:
        printf("slt %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_sltu:
        printf("sltu %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_xor:
        printf("xor %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_srl:
        printf("srl %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_sra:
        printf("sra %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_or:
        printf("or %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_and:
        printf("and %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_mul:
        printf("mul %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_mulh:
        printf("mulh %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_mulhsu:
        printf("mulhsu %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_mulhu:
        printf("mulhu %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_div:
        printf("div %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break; 
    case D_divu:
        printf("divu %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_rem:
        printf("rem %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_remu:
        printf("remu %s, %s, %s\n", reg_names[param0],  reg_names[param1], reg_names[param2]);
        break;
    case D_fence:
        printf("fence %s, %d\n", reg_names[param0], param1);
        break;
    case D_ecall:
        printf("ecall\n");
        break;
    case D_ebreak:
        printf("ebreak\n");
        break;
    case D_sret:
        printf("sret\n");
        break;
    case D_csrrw:       
        printf("csrrw %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2); 
        break;
    case D_csrrs:       
        printf("csrrs %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);  
        break;
    case D_csrrc:       
        printf("csrrc %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2); 
        break;
    case D_csrrwi:  
        printf("csrrwi %s, %d, %d\n", reg_names[param0],  param1, param2); 
        break;
    case D_csrrsi:  
        printf("csrrsi %s, %d, %d\n", reg_names[param0],  param1, param2);  
        break;
    case D_csrrci:  
        printf("csrrci %s, %d, %d\n", reg_names[param0],  param1, param2); 
        break;
    case D_lb:
        printf("lb %s, %d(%s)\n", reg_names[param0], param1, reg_names[param2]);
        break;
    case D_lh:
        printf("lh %s, %d(%s)\n", reg_names[param0], param1, reg_names[param2]);
        break;
    case D_lw:
        printf("lw %s, %d(%s)\n", reg_names[param0], param1, reg_names[param2]);
        break;
    case D_lbu:
        printf("lbu %s, %d(%s)\n", reg_names[param0], param1, reg_names[param2]);
        break;
    case D_lhu:
        printf("lhu %s, %d(%s)\n", reg_names[param0], param1, reg_names[param2]);
        break;
    case D_sb:
        printf("sb %s, %d(%s)\n", reg_names[param1], param2, reg_names[param0]);
        break;
    case D_sh:
        printf("sh %s, %d(%s)\n", reg_names[param1], param2, reg_names[param0]);
        break;
    case D_sw:
        printf("sw %s, %d(%s)\n", reg_names[param1], param2, reg_names[param0]);
        break;
    case D_jal:
        printf("jal %s, %d\n", reg_names[param0], param1);
        break;
    case D_jalr:
        printf("jalr %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_beq:
        printf("beq %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_bne:
        printf("bne %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_blt:
        printf("blt %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_bge:
        printf("bge %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_bltu:
        printf("bltu %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    case D_bgeu:
        printf("bgeu %s, %s, %d\n", reg_names[param0],  reg_names[param1], param2);
        break;
    
    default:
        break;
    }
}

#define op_special(op) op != D_ebreak&& op != D_ecall&& op != D_fence && op != D_csrrw && op != D_csrrs && op != D_csrrc && op != D_csrrwi && op != D_csrrsi && op != D_csrrci 

#ifdef DEBUG_REGS
    void cpu_debug(const CPU* cpu){if(jump_cycles){return;} bcpu=true; /*if(cpu->pc < 0x40100050){ return; }*/ cpu_print_regs(cpu); }
#else 
    void cpu_debug(const CPU* cpu){
        if(jump_cycles){return;}
        //if(cpu->pc < 0x40100050){ return; }
        //symbol_table_lookup(cpu->pc);
        cpu_debug_sym(cpu);
    }
#endif

void enable_Debug(){
    jump_cycles = false;
}

#ifdef DEBUG_ASM
    void cpu_disasm(const CPU* cpu, const enum DISASEM_OP op, uint32_t param0, uint32_t param1, uint32_t param2){ if(jump_cycles && op_special(op)){return;} /*if(cpu->pc < 0x40100050){ return; }*/ cpu_print_asm(cpu, op, param0, param1, param2); }
#else
    void cpu_disasm(const CPU* cpu, const enum DISASEM_OP op, uint32_t param0, uint32_t param1, uint32_t param2){} //update_history(cpu, op, param0, param1, param2); }
#endif 

bool cpu_error(const uint32_t pc, enum Error_t code){
    size_t end = History_PTR;
    if(end == 0){
        end = DEBUG_HIST_SIZE - 1;
    }
    for (size_t i = 0; i < end; i++)
    {
        if(!DebugHistory[i].valid) continue;
        cpu_print_regs(&DebugHistory[i].cpu);
        cpu_print_asm(&DebugHistory[i].cpu, DebugHistory[i].op, DebugHistory[i].params[0], DebugHistory[i].params[1], DebugHistory[i].params[2]);
    }
    printf("[PC: 0x%08x][ERROR] ", pc);
    switch (code)
    {
    case INVALID_PC:
        printf("Invalid PC! Pc (0x%08x) was outside the memory region!\n",pc);
        break;

    case INVALID_OPCODE:
        printf("Invalid OPCODE! The opcode (%d) of instruction (%08x) isn't known!\n", last_opc, last_inst);
        break;
        
    case INVALID_MEM_READ:
        printf("Invalid Memory Read! Address [0x%08x] was outside the memory region!\n", last_addr);
        break;
        
    case INVALID_MEM_WRITE:
        printf("Invalid Memory Write! Address [0x%08x] was outside the memory region!\n", last_addr);
        break;
        
    case INVALID_EXECUTE:
        printf("Invalid Execute! Something went wrong executing the instruction!\n");
        break;
    }
    return false;
}