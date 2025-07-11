#include "cpuboard.h"
#include <stdio.h>

// Instruction field extraction macros
#define GET_OPCODE_PREFIX(inst)    ((inst) & 0xF0)
#define GET_A_FIELD(inst)          (((inst) >> 3) & 0x01)
#define GET_B_FIELD(inst)          ((inst) & 0x07)
#define GET_SHIFT_MODE(inst)       ((inst) & 0x03)
#define GET_BRANCH_CONDITION(inst) (((inst) >> 4) & 0x0F)

// Opcode prefixes
#define NOP_HLT_OPCODE_PREFIX  0x00
#define OUT_IN_OPCODE_PREFIX   0x10
#define RCF_SCF_OPCODE_PREFIX  0x20
#define LD_OPCODE_PREFIX       0x60
#define ST_OPCODE_PREFIX       0x70
#define ADD_OPCODE_PREFIX      0xB0
#define ADC_OPCODE_PREFIX      0x90
#define SUB_OPCODE_PREFIX      0xA0
#define SBC_OPCODE_PREFIX      0x80
#define CMP_OPCODE_PREFIX      0xF0
#define AND_OPCODE_PREFIX      0xE0
#define OR_OPCODE_PREFIX       0xD0
#define EOR_OPCODE_PREFIX      0xC0
#define SHIFT_ROTATE_PREFIX    0x40
#define BRANCH_OPCODE_PREFIX   0x30
#define JAL_JR_OPCODE_PREFIX   0x00

// Function prototypes
static void fetch_instruction(Cpub *cpub, InstructionInfo *info);
static void decode_instruction(Cpub *cpub, InstructionInfo *info);
static void fetch_operands(Cpub *cpub, InstructionInfo *info);
static void execute_alu_operation(Cpub *cpub, InstructionInfo *info);
static void write_back_result(Cpub *cpub, InstructionInfo *info);
static void update_program_counter(Cpub *cpub, InstructionInfo *info);
static void update_flags_for_arith_logic(Cpub *cpub, Uword old_val_a, Uword old_val_b, Uword alu_result, int is_sub);


// Main instruction execution function
int step(Cpub *cpub)
{
   InstructionInfo info = {0};

   // 1. Instruction fetch
   fetch_instruction(cpub, &info);

   // 2. Instruction decode
   decode_instruction(cpub, &info);
   if (info.type == INST_UNKNOWN) {
       fprintf(stderr, "Error: Unknown instruction 0x%02x at 0x%03x\n", info.instruction_word_1st, info.pc_at_fetch);
       return RUN_HALT;
   }

   // Check for HLT instruction
   if (info.type == INST_HLT) {
       printf("HLT instruction executed. Program Halted.\n");
       return RUN_HALT;
   }

   // 3. Operand fetch (if needed)
   if (info.addr_mode_b != ADDR_MODE_NONE) {
       fetch_operands(cpub, &info);
   } else if (info.type == INST_Bbc || info.type == INST_JAL || info.type == INST_JR) {
       if (info.type != INST_JR) {
           info.instruction_word_2nd = cpub->mem[cpub->pc];
           cpub->pc++;
           info.effective_addr = info.instruction_word_2nd;
       } else {
           info.operand_a_val = cpub->acc;
       }
   }

   // 4. ALU execution (for instructions that need it)
   if (info.type != INST_ST && info.type != INST_Bbc && info.type != INST_JAL && info.type != INST_JR && info.type != INST_NOP) {
       execute_alu_operation(cpub, &info);
   }

   // 5. Write back results
   write_back_result(cpub, &info);

   // 6. Update program counter
   update_program_counter(cpub, &info);

   return RUN_STEP;
}

// Phase 1: Instruction Fetch
static void fetch_instruction(Cpub *cpub, InstructionInfo *info) {
   info->pc_at_fetch = cpub->pc;
   info->instruction_word_1st = cpub->mem[cpub->pc];
   cpub->pc++;

   info->type = INST_UNKNOWN;
   info->addr_mode_b = ADDR_MODE_NONE;
   info->a_field = GET_A_FIELD(info->instruction_word_1st);
   info->b_field = GET_B_FIELD(info->instruction_word_1st);
}
 
// Phase 2: Instruction Decode
static void decode_instruction(Cpub *cpub, InstructionInfo *info)
{
    Uword op_prefix = GET_OPCODE_PREFIX(info->instruction_word_1st);

    if (op_prefix == ST_OPCODE_PREFIX) {
        info->type = INST_ST;
    } else {
        info->type = INST_UNKNOWN;
    }

    // Determine addressing mode for operand B
    switch (info->b_field) {
        case 0x00: info->addr_mode_b = ADDR_MODE_REG_ACC; break;
        case 0x01: info->addr_mode_b = ADDR_MODE_REG_IX; break;
        case 0x02: info->addr_mode_b = ADDR_MODE_IMMEDIATE; break;
        case 0x04: info->addr_mode_b = ADDR_MODE_ABS_PROG; break;
        case 0x05: info->addr_mode_b = ADDR_MODE_ABS_DATA; break;
        case 0x06: info->addr_mode_b = ADDR_MODE_IX_PROG; break;
        case 0x07: info->addr_mode_b = ADDR_MODE_IX_DATA; break;
        default: 
            info->addr_mode_b = ADDR_MODE_NONE; 
            fprintf(stderr, "ERROR: Unexpected B_Field 0x%x for instruction type %d.\n", info->b_field, info->type); 
            break;
    }
}

// Phase 3: Operand Fetch
static void fetch_operands(Cpub *cpub, InstructionInfo *info) {
   // Read operand A value (always from register)
   info->operand_a_val = (info->a_field == 0) ? cpub->acc : cpub->ix;
   info->result_dest_reg_ptr = (info->a_field == 0) ? &(cpub->acc) : &(cpub->ix);

   switch (info->addr_mode_b) {
       case ADDR_MODE_REG_ACC:
           info->operand_b_val = cpub->acc;
           break;
       case ADDR_MODE_REG_IX:
           info->operand_b_val = cpub->ix;
           break;
       case ADDR_MODE_IMMEDIATE:
           info->instruction_word_2nd = cpub->mem[cpub->pc];
           cpub->pc++;
           info->operand_b_val = info->instruction_word_2nd;
           break;
       case ADDR_MODE_ABS_PROG:
           info->instruction_word_2nd = cpub->mem[cpub->pc];
           cpub->pc++;
           info->effective_addr = info->instruction_word_2nd;
           info->operand_b_val = cpub->mem[info->effective_addr];
           break;
       case ADDR_MODE_ABS_DATA:
           info->instruction_word_2nd = cpub->mem[cpub->pc];
           cpub->pc++;
           info->effective_addr = 0x100 | info->instruction_word_2nd;
           info->operand_b_val = cpub->mem[info->effective_addr];
           break;
       case ADDR_MODE_IX_PROG:
           info->instruction_word_2nd = cpub->mem[cpub->pc];
           cpub->pc++;
           info->effective_addr = (cpub->ix + info->instruction_word_2nd) & 0xFF;
           info->effective_addr = info->effective_addr & 0x1FF;
           info->operand_b_val = cpub->mem[info->effective_addr];
           break;
       case ADDR_MODE_IX_DATA:
           info->instruction_word_2nd = cpub->mem[cpub->pc];
           cpub->pc++;
           info->effective_addr = (cpub->ix + info->instruction_word_2nd) & 0xFF;
           info->effective_addr = 0x100 | info->effective_addr;
           info->operand_b_val = cpub->mem[info->effective_addr];
           break;
       case ADDR_MODE_NONE:
           info->operand_b_val = 0;
           break;
       default:
           fprintf(stderr, "Operand Fetch Error: Unsupported addressing mode: %d\n", info->addr_mode_b);
           info->operand_b_val = 0;
           info->type = INST_UNKNOWN;
           break;
   }
}


// Phase 4: Execute ALU Operation
static void execute_alu_operation(Cpub *cpub, InstructionInfo *info) {
   if (info->type == INST_ST) {
       // ST instruction does not require ALU operation
   } else {
       info->type = INST_UNKNOWN;
   }
}


// Helper: Update flags for arithmetic/logic operations
static void update_flags_for_arith_logic(Cpub *cpub, Uword old_val_a, Uword old_val_b, Uword alu_result, int is_sub) {
   // Zero Flag
   cpub->zf = (alu_result == 0) ? 1 : 0;

   // Negative Flag
   cpub->nf = (alu_result & 0x80) ? 1 : 0;

   // Carry/Borrow Flag
   if (!is_sub) {
       cpub->cf = (((unsigned int)old_val_a + (unsigned int)old_val_b) > 0xFF) ? 1 : 0;
   } else {
       unsigned int temp_b = (unsigned int)~old_val_b;
       unsigned int sum = (unsigned int)old_val_a + temp_b + 1;
       cpub->cf = !((sum >> 8) & 0x01);
   }

   // Overflow Flag
   if (!is_sub) {
       cpub->vf = (((old_val_a & 0x80) == (old_val_b & 0x80)) && ((old_val_a & 0x80) != (alu_result & 0x80))) ? 1 : 0;
   } else {
       Uword neg_b = ~old_val_b + 1;
       cpub->vf = (((old_val_a & 0x80) == (neg_b & 0x80)) && ((old_val_a & 0x80) != (alu_result & 0x80))) ? 1 : 0;
   }
}


// Phase 5: Write Back Result
static void write_back_result(Cpub *cpub, InstructionInfo *info) {
    if (info->type == INST_ST) {
        // Store register A contents to memory
        if (info->result_dest_reg_ptr != NULL) {
            Uword data_to_store = *(info->result_dest_reg_ptr);
            cpub->mem[info->effective_addr] = data_to_store;
        } else {
            fprintf(stderr, "Error: result_dest_reg_ptr is NULL for ST instruction.\n");
        }
    }
}


// Phase 6: Update Program Counter
static void update_program_counter(Cpub *cpub, InstructionInfo *info) {
    // Branch condition evaluation and PC update logic
    switch (info->type) {
        case INST_Bbc:
            switch (GET_BRANCH_CONDITION(info->instruction_word_1st)) {
                case 0x00: info->is_branch_taken = 1; break; // BA (Always)
                case 0x08: info->is_branch_taken = cpub->vf; break; // BVF (on oVerFlow)
                case 0x02: info->is_branch_taken = (cpub->zf == 0); break; // BNZ (on Not Zero)
                case 0x09: info->is_branch_taken = (cpub->zf == 1); break; // BZ (on Zero)
                case 0x04: info->is_branch_taken = (cpub->nf == 0); break; // BZP (on Zero or Positive)
                case 0x01: info->is_branch_taken = (cpub->nf == 1); break; // BN (on Negative)
                case 0x06: info->is_branch_taken = ((cpub->nf == 0) && (cpub->zf == 0)); break; // BP (on Positive)
                case 0x0B: info->is_branch_taken = ((cpub->nf == 1) || (cpub->zf == 1)); break; // BZN (on Zero or Negative)
                case 0x05: info->is_branch_taken = (cpub->cf == 0); break; // BNC (on No Carry)
                case 0x0D: info->is_branch_taken = (cpub->cf == 1); break; // BC (on Carry)
                case 0x0A: info->is_branch_taken = ((cpub->vf ^ cpub->nf) == 0); break; // BGE (on Greater than or Equal)
                case 0x0E: info->is_branch_taken = ((cpub->vf ^ cpub->nf) == 1); break; // BLT (on Less Than)
                case 0x07: info->is_branch_taken = (((cpub->vf ^ cpub->nf) == 0) && (cpub->zf == 0)); break; // BGT (on Greater Than)
                case 0x0F: info->is_branch_taken = (((cpub->vf ^ cpub->nf) == 1) || (cpub->zf == 1)); break; // BLE (on Less Than or Equal)
                default: info->is_branch_taken = 0; break;
            }

            if (info->is_branch_taken) {
                cpub->pc = info->effective_addr;
            }
            break;
        case INST_JAL:
            cpub->pc = info->effective_addr;
            break;
        case INST_JR:
            cpub->pc = cpub->acc;
            break;
        default:
            // PC already updated by fetch for other instructions
            break;
    }
}