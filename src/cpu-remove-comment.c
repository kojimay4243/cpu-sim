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
   if (info.type != INST_LD && info.type != INST_ST && info.type != INST_Bbc && info.type != INST_JAL && info.type != INST_JR && 
       info.type != INST_NOP && info.type != INST_RCF && info.type != INST_SCF && 
       info.type != INST_HLT && info.type != INST_IN && info.type != INST_OUT) {
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

    switch (op_prefix) {
        case NOP_HLT_OPCODE_PREFIX:
            if (info->instruction_word_1st == 0x00) { info->type = INST_NOP; }
            else if (info->instruction_word_1st == 0x0F) { info->type = INST_HLT; }
            else if (info->instruction_word_1st == 0x10) { info->type = INST_OUT; }
            else if (info->instruction_word_1st == 0x1F) { info->type = INST_IN; }
            else if (info->instruction_word_1st == 0x0A) {
                info->type = INST_JAL; info->branch_cond = BRANCH_COND_NONE;
            }
            else if (info->instruction_word_1st == 0x0B) {
                 info->type = INST_JR; info->branch_cond = BRANCH_COND_NONE;
            }
            else { info->type = INST_UNKNOWN; }
            break;

        case RCF_SCF_OPCODE_PREFIX:
            if (info->instruction_word_1st == 0x20) { info->type = INST_RCF; }
            else if (info->instruction_word_1st == 0x2F) { info->type = INST_SCF; }
            else { info->type = INST_UNKNOWN; }
            break;

        case LD_OPCODE_PREFIX: { info->type = INST_LD; break; }
        case ST_OPCODE_PREFIX: { info->type = INST_ST; break; }
        case ADD_OPCODE_PREFIX: { info->type = INST_ADD; break; }
        case ADC_OPCODE_PREFIX: { info->type = INST_ADC; break; }
        case SUB_OPCODE_PREFIX: { info->type = INST_SUB; break; }
        case SBC_OPCODE_PREFIX: { info->type = INST_SBC; break; }
        case CMP_OPCODE_PREFIX: { info->type = INST_CMP; break; }
        case AND_OPCODE_PREFIX: { info->type = INST_AND; break; }
        case OR_OPCODE_PREFIX: { info->type = INST_OR; break; }
        case EOR_OPCODE_PREFIX: { info->type = INST_EOR; break; }
        case SHIFT_ROTATE_PREFIX: {
            info->type = (GET_A_FIELD(info->instruction_word_1st) == 0) ? INST_Ssm : INST_Rsm;
            info->shift_mode = GET_SHIFT_MODE(info->instruction_word_1st);
            break;
        }
        case BRANCH_OPCODE_PREFIX: {
            info->type = INST_Bbc;
            break;
        }
        default: {
            info->type = INST_UNKNOWN;
            break;
        }
    }
    // Determine addressing mode for operand B
    if (info->type == INST_HLT || info->type == INST_NOP || info->type == INST_OUT || info->type == INST_IN ||
        info->type == INST_RCF || info->type == INST_SCF || info->type == INST_JR ||
        info->type == INST_Ssm || info->type == INST_Rsm)
    {
        info->addr_mode_b = ADDR_MODE_NONE;
    } else if (info->type == INST_Bbc || info->type == INST_JAL) {
        info->addr_mode_b = ADDR_MODE_ABS_PROG;
    }
    else
    {
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
        }
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
   Uword old_val_a = info->operand_a_val;
   Uword old_val_b = info->operand_b_val;
   info->alu_result = 0;

   switch (info->type) {
       case INST_ADD:
           info->alu_result = old_val_a + old_val_b;
           update_flags_for_arith_logic(cpub, old_val_a, old_val_b, info->alu_result, 0);
           break;
       case INST_ADC:
           info->alu_result = old_val_a + old_val_b + cpub->cf;
           update_flags_for_arith_logic(cpub, old_val_a, old_val_b, info->alu_result, 0);
           break;
       case INST_SUB:
           info->alu_result = old_val_a - old_val_b;
           update_flags_for_arith_logic(cpub, old_val_a, old_val_b, info->alu_result, 1);
           break;
       case INST_SBC:
           info->alu_result = old_val_a - old_val_b - cpub->cf;
           update_flags_for_arith_logic(cpub, old_val_a, old_val_b, info->alu_result, 1);
           break;
       case INST_CMP:
           info->alu_result = old_val_a - old_val_b;
           update_flags_for_arith_logic(cpub, old_val_a, old_val_b, info->alu_result, 1);
           break;
       case INST_AND:
           info->alu_result = old_val_a & old_val_b;
           cpub->cf = 0; cpub->vf = 0;
           cpub->zf = (info->alu_result == 0) ? 1 : 0;
           cpub->nf = (info->alu_result & 0x80) ? 1 : 0;
           break;
       case INST_OR:
           info->alu_result = old_val_a | old_val_b;
           cpub->cf = 0; cpub->vf = 0;
           cpub->zf = (info->alu_result == 0) ? 1 : 0;
           cpub->nf = (info->alu_result & 0x80) ? 1 : 0;
           break;
       case INST_EOR:
           info->alu_result = old_val_a ^ old_val_b;
           cpub->cf = 0; cpub->vf = 0;
           cpub->zf = (info->alu_result == 0) ? 1 : 0;
           cpub->nf = (info->alu_result & 0x80) ? 1 : 0;
           break;
       case INST_Ssm:
            if (info->shift_mode == SHIFT_MODE_SRA) {
                cpub->cf = (old_val_a & 0x01) ? 1 : 0;
                info->alu_result = ((Sword)old_val_a >> 1);
                cpub->zf = (info->alu_result == 0) ? 1 : 0;
                cpub->nf = (info->alu_result & 0x80) ? 1 : 0;
            } else { fprintf(stderr, "Unsupported Shift Mode: %d\n", info->shift_mode); info->type = INST_UNKNOWN; }
            break;
       case INST_Rsm:
            if (info->shift_mode == SHIFT_MODE_RLL) {
                cpub->cf = (old_val_a & 0x80) ? 1 : 0;
                info->alu_result = (old_val_a << 1) | cpub->cf;
                cpub->zf = (info->alu_result == 0) ? 1 : 0;
                cpub->nf = (info->alu_result & 0x80) ? 1 : 0;
            } else { fprintf(stderr, "Unsupported Rotate Mode: %d\n", info->shift_mode); info->type = INST_UNKNOWN; }
            break;
       default:
           break;
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
   switch (info->type) {
       case INST_LD:
           // Load operand B value to register A
           if (info->result_dest_reg_ptr != NULL) {
               *(info->result_dest_reg_ptr) = info->operand_b_val;
           } else {
               fprintf(stderr, "Error: result_dest_reg_ptr is NULL for LD instruction.\n");
           }
           break;
       case INST_ADD:
       case INST_ADC:
       case INST_SUB:
       case INST_SBC:
       case INST_AND:
       case INST_OR:
       case INST_EOR:
       case INST_Ssm:
       case INST_Rsm:
           // Write ALU result to register A
           if (info->result_dest_reg_ptr != NULL) {
               *(info->result_dest_reg_ptr) = info->alu_result;
           } else {
               fprintf(stderr, "Error: result_dest_reg_ptr is NULL for type %d.\n", info->type);
           }
           break;
       case INST_ST:
           // Store register A contents to memory
           if (info->result_dest_reg_ptr != NULL) {
                Uword data_to_store = *(info->result_dest_reg_ptr);
                cpub->mem[info->effective_addr] = data_to_store;
           } else {
               fprintf(stderr, "Error: result_dest_reg_ptr is NULL for ST instruction.\n");
           }
           break;
       case INST_CMP:
           // CMP does not write back to register/memory
           break;
       case INST_RCF:
           // Reset Carry Flag
           cpub->cf = 0;
           break;
       case INST_SCF:
           // Set Carry Flag
           cpub->cf = 1;
           break;
       case INST_JAL:
           // Store PC+2 to ACC
           if (info->result_dest_reg_ptr != NULL) {
                *(info->result_dest_reg_ptr) = info->pc_at_fetch + 2;
           } else {
                fprintf(stderr, "Error: result_dest_reg_ptr is NULL for JAL instruction.\n");
           }
           break;
       default:
           break;
   }
}


// Phase 6: Update Program Counter
static void update_program_counter(Cpub *cpub, InstructionInfo *info) {
    switch (info->type) {
        case INST_Bbc:
            switch (GET_BRANCH_CONDITION(info->instruction_word_1st)) {
                case 0x00: info->is_branch_taken = 1; break; // BA (Always)
                case 0x08: info->is_branch_taken = cpub->vf; break; // BVF (on oVerFlow)
                case 0x09: info->is_branch_taken = (cpub->zf == 1); break; // BZ (on Zero)
                case 0x04: info->is_branch_taken = (cpub->nf == 0); break; // BZP (on Zero or Positive)
                case 0x03: // BNZ from sample (0x31)
                case 0x02: // BNZ (on Not Zero)
                    info->is_branch_taken = (cpub->zf == 0);
                    break;
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
