/*
 *	Project-based Learning II (CPU)
 *
 *	Program:	instruction set simulator of the Educational CPU Board
 *	File Name:	cpuboard.c
 *	Descrioption:	simulation(emulation) of an instruction
 */

 #include	"cpuboard.h"
 #include	<stdio.h> // printfなどデバッグ出力用

/*=============================================================================
  * Macros for Instruction Decoding
  *===========================================================================*/
 // 命令語のビットパターンからOPコード、Aフィールド、Bフィールドを抽出するマクロ
 #define GET_OPCODE_PREFIX(inst)    ((inst) & 0xF0) // 上位4ビットのOPコードプレフィックス
 #define GET_A_FIELD(inst)          (((inst) >> 3) & 0x01) // Aフィールド (1ビット)
 #define GET_B_FIELD(inst)          ((inst) & 0x07)        // Bフィールド (3ビット)
 #define GET_SHIFT_MODE(inst)       ((inst) & 0x03)        // Shift/Rotate 命令の下位2ビット (sm)
 #define GET_BRANCH_CONDITION(inst) (((inst) >> 4) & 0x0F) // Branch 命令の上位4ビット (bc)

 // OPコードのプレフィックス定義 (表4より抜粋)
 #define NOP_HLT_OPCODE_PREFIX  0x00 // NOP, HLT
 #define OUT_IN_OPCODE_PREFIX   0x10 // OUT, IN
 #define RCF_SCF_OPCODE_PREFIX  0x20 // RCF, SCF
 #define LD_OPCODE_PREFIX       0x60 // LD
 #define ST_OPCODE_PREFIX       0x70 // ST
 #define ADD_OPCODE_PREFIX      0xB0 // ADD
 #define ADC_OPCODE_PREFIX      0x90 // ADC
 #define SUB_OPCODE_PREFIX      0xA0 // SUB
 #define SBC_OPCODE_PREFIX      0x80 // SBC
 #define CMP_OPCODE_PREFIX      0xF0 // CMP
 #define AND_OPCODE_PREFIX      0xE0 // AND
 #define OR_OPCODE_PREFIX       0xD0 // OR
 #define EOR_OPCODE_PREFIX      0xC0 // EOR
 #define SHIFT_ROTATE_PREFIX    0x40 // Ssm, Rsm
 #define BRANCH_OPCODE_PREFIX   0x30 // Bbc
 #define JAL_JR_OPCODE_PREFIX   0x00 // JAL, JR (NOP/HLTと同じプレフィックスなので、全ビットで判別が必要)
 
 /*=============================================================================
  * Private Function Prototypes (フェーズごとの関数)
  *===========================================================================*/
 static void fetch_instruction(Cpub *cpub, InstructionInfo *info);
 static void decode_instruction(Cpub *cpub, InstructionInfo *info);
 static void fetch_operands(Cpub *cpub, InstructionInfo *info);
 static void execute_alu_operation(Cpub *cpub, InstructionInfo *info);
 static void write_back_result(Cpub *cpub, InstructionInfo *info);
 static void update_program_counter(Cpub *cpub, InstructionInfo *info);
 static void update_flags_for_arith_logic(Cpub *cpub, Uword old_val_a, Uword old_val_b, Uword alu_result, int is_sub);


 /*=============================================================================
  *   Simulation of a Single Instruction
  *===========================================================================*/

int
step(Cpub *cpub)
{
   InstructionInfo info = {0};  // 命令に関する情報を保持する構造体 (初期化)

   printf("DEBUG: --- Starting new instruction cycle ---\n");
   printf("DEBUG: Initial PC: 0x%03x\n", cpub->pc);

   // 1. 命令フェッチ
   fetch_instruction(cpub, &info);

   // 2. 命令解読
   decode_instruction(cpub, &info);
   if (info.type == INST_UNKNOWN) { // 命令解読段階で不明な命令と判断された場合
       fprintf(stderr, "Error: Unknown instruction decoded from 0x%02x at 0x%03x\n", info.instruction_word_1st, info.pc_at_fetch);
       return RUN_HALT; // 未知の命令は停止
   }
   printf("DEBUG: Instruction decoded as Type=%d (A_Field=%d, B_Field=%d, AddrModeB=%d)\n", info.type, info.a_field, info.b_field, info.addr_mode_b);

   // HLT命令の場合、ここで終了
   if (info.type == INST_HLT) {
       printf("HLT instruction executed. Program Halted.\n");
       return RUN_HALT;
   }
   printf("DEBUG: Instruction is not HLT. Continuing...\n");

   // 3. オペランドフェッチ (必要な場合のみ)
   if (info.addr_mode_b != ADDR_MODE_NONE) { // オペランドが必要な命令
       fetch_operands(cpub, &info);
       printf("DEBUG: Operand fetched. OpA=0x%02x, OpB=0x%02x, Effective Address=0x%03x\n", info.operand_a_val, info.operand_b_val, info.effective_addr);
   } else if (info.type == INST_Bbc || info.type == INST_JAL || info.type == INST_JR) { // Branch, JAL, JRは2語目がアドレスまたはレジスタ参照
       printf("DEBUG: Special operand/address handling for Branch/JAL/JR.\n");
       if (info.type != INST_JR) { // JRは2語命令ではない
           info.instruction_word_2nd = cpub->mem[cpub->pc];
           printf("DEBUG: Fetched 2nd word: 0x%02x\n", info.instruction_word_2nd);
           cpub->pc++; // PCをさらにインクリメント

           // 分岐先アドレスの実効アドレス計算 (Pgm領域なので上位ビット0)
           info.effective_addr = info.instruction_word_2nd;
           printf("DEBUG: Effective address from 2nd word: 0x%03x\n", info.effective_addr);
       } else { // JRの場合、ACCがアドレス
           info.operand_a_val = cpub->acc; // JRはACCの内容をPCにする
           printf("DEBUG: JR: Uses ACC (0x%02x) for jump address.\n", info.operand_a_val);
       }
   } else {
       printf("DEBUG: No specific operand fetch for this instruction type (e.g., NOP, single-word instructions with register operands).\n");
   }

   // 4. 演算実行 (ALUを使用する命令のみ)
   // ... (変更なし、以前の回答で提示した内容) ...
   if (info.type != INST_ST && info.type != INST_Bbc && info.type != INST_JAL && info.type != INST_JR && info.type != INST_NOP) {
       execute_alu_operation(cpub, &info);
   } else {
       printf("DEBUG: ALU operation skipped for this instruction type: %d\n", info.type);
   }

   // 5. 結果書き込み
   write_back_result(cpub, &info);
   printf("DEBUG: Result write back completed.\n");

   // 6. PC更新 (分岐命令など、特別なPC更新が必要な場合)
   update_program_counter(cpub, &info);
   printf("DEBUG: Program Counter updated. Current PC: 0x%03x\n", cpub->pc);

   printf("DEBUG: --- Instruction cycle completed. Final PC: 0x%03x ---\n", cpub->pc);

   return RUN_STEP; // HLT命令でなければ次のサイクルへ進む
}

  /*=============================================================================
  * Phase 1: Instruction Fetch (命令フェッチ)
  *===========================================================================*/
  static void fetch_instruction(Cpub *cpub, InstructionInfo *info) {
    printf("DEBUG(Phase 1): Entering fetch_instruction.\n"); // フェーズ入り口
    info->pc_at_fetch = cpub->pc; // 命令フェッチ時のPCを保存
    info->instruction_word_1st = cpub->mem[cpub->pc];
    printf("DEBUG(Phase 1): Fetched PC=0x%03x, Instruction=0x%02x\n", info->pc_at_fetch, info->instruction_word_1st);
    cpub->pc++; // PCを次の語へインクリメント
    printf("DEBUG(Phase 1): PC incremented to 0x%03x.\n", cpub->pc);

    info->type = INST_UNKNOWN; // デフォルトは不明な命令
    info->addr_mode_b = ADDR_MODE_NONE; // オペランドBのアドレッシングモード
    info->a_field = GET_A_FIELD(info->instruction_word_1st); // Aフィールドはここで抽出可能
    info->b_field = GET_B_FIELD(info->instruction_word_1st); // Bフィールドもここで抽出可能
    printf("DEBUG(Phase 1): A_Field=%d, B_Field=%d. Exiting fetch_instruction.\n", info->a_field, info->b_field);
}
 
 /*=============================================================================
  * Phase 2: Instruction Decode (命令解読)
  *===========================================================================*/
  static void decode_instruction(Cpub *cpub, InstructionInfo *info)
  {
      printf("DEBUG(Phase 2): Entering decode_instruction.\n"); // フェーズ入り口
      // 命令の一語目をビットシフトして扱いやすくする
       Uword op_prefix = GET_OPCODE_PREFIX(info->instruction_word_1st);
      printf("DEBUG(Phase 2): Decoding instruction 0x%02x (OP_PREFIX=0x%02x)\n", info->instruction_word_1st, op_prefix);
  
       if (op_prefix == ST_OPCODE_PREFIX) {
           info->type = INST_ST;
           printf("DEBUG(Phase 2): Matched ST_OPCODE_PREFIX.\n");
       } else {
           info->type = INST_UNKNOWN; // ST以外のOPコードプレフィックス
           printf("DEBUG(Phase 2): Default case hit (Unknown OP Prefix). info->type set to INST_UNKNOWN.\n");
       }
  
       // オペランドBのアドレッシングモードの判定 (命令タイプに応じてBフィールドを解釈)
       switch (info->b_field) {
           case 0x00: info->addr_mode_b = ADDR_MODE_REG_ACC; printf("DEBUG(Phase 2): AddrModeB=REG_ACC.\n"); break;
           case 0x01: info->addr_mode_b = ADDR_MODE_REG_IX;  printf("DEBUG(Phase 2): AddrModeB=REG_IX.\n"); break;
           case 0x02: info->addr_mode_b = ADDR_MODE_IMMEDIATE; printf("DEBUG(Phase 2): AddrModeB=IMMEDIATE.\n"); break; // d (即値)
           case 0x04: info->addr_mode_b = ADDR_MODE_ABS_PROG;  printf("DEBUG(Phase 2): AddrModeB=ABS_PROG.\n"); break;  // [d] (プログラム領域絶対アドレス)
           case 0x05: info->addr_mode_b = ADDR_MODE_ABS_DATA;  printf("DEBUG(Phase 2): AddrModeB=ABS_DATA.\n"); break;  // (d) (データ領域絶対アドレス)
           case 0x06: info->addr_mode_b = ADDR_MODE_IX_PROG;   printf("DEBUG(Phase 2): AddrModeB=IX_PROG.\n"); break;   // [IX+d] (プログラム領域IX修飾)
           case 0x07: info->addr_mode_b = ADDR_MODE_IX_DATA;   printf("DEBUG(Phase 2): AddrModeB=IX_DATA.\n"); break;   // (IX+d) (データ領域IX修飾)
           default: info->addr_mode_b = ADDR_MODE_NONE; fprintf(stderr, "DEBUG(Phase 2): ERROR: Unexpected B_Field 0x%x for instruction type %d.\n", info->b_field, info->type); // エラー、または予期せぬBフィールド
       }
      
      printf("DEBUG(Phase 2): Exiting decode_instruction. Final Type=%d, AddrModeB=%d\n", info->type, info->addr_mode_b);
  }

  /*=============================================================================
  * Phase 3: Operand Fetch (オペランドフェッチ)
  *===========================================================================*/
 static void fetch_operands(Cpub *cpub, InstructionInfo *info) {
    printf("DEBUG(Phase 3): Entering fetch_operands.\n"); // フェーズ入り口
    // オペランドAの値を読み出す (常にレジスタから)
    info->operand_a_val = (info->a_field == 0) ? cpub->acc : cpub->ix;
    info->result_dest_reg_ptr = (info->a_field == 0) ? &(cpub->acc) : &(cpub->ix);
    printf("DEBUG(Phase 3): Operand A value: 0x%02x (from %s).\n", info->operand_a_val, (info->a_field == 0) ? "ACC" : "IX");

    // オペランドBの値をアドレッシングモードに応じて読み出す
    printf("DEBUG(Phase 3): Fetching operand B with AddrModeB=%d (B_Field=0x%x).\n", info->addr_mode_b, info->b_field);
    switch (info->addr_mode_b) {
        case ADDR_MODE_REG_ACC:
            info->operand_b_val = cpub->acc;
            printf("DEBUG(Phase 3): OpB from ACC: 0x%02x.\n", info->operand_b_val);
            break;
        case ADDR_MODE_REG_IX:
            info->operand_b_val = cpub->ix;
            printf("DEBUG(Phase 3): OpB from IX: 0x%02x.\n", info->operand_b_val);
            break;
        case ADDR_MODE_IMMEDIATE: // 即値 (2語目)
            info->instruction_word_2nd = cpub->mem[cpub->pc]; // 2語目をデータとして読み込む
            printf("DEBUG(Phase 3): Fetched Immediate 2nd word: 0x%02x.\n", info->instruction_word_2nd);
            cpub->pc++; // PCをさらにインクリメント
            info->operand_b_val = info->instruction_word_2nd;
            break;
        case ADDR_MODE_ABS_PROG: // 絶対アドレス (プログラム領域) [d]
            info->instruction_word_2nd = cpub->mem[cpub->pc]; // 2語目をアドレスとして読み込む
            printf("DEBUG(Phase 3): Fetched Abs Prog 2nd word: 0x%02x.\n", info->instruction_word_2nd);
            cpub->pc++; // PCをさらにインクリメント
            info->effective_addr = info->instruction_word_2nd; // プログラム領域0xxH
            info->operand_b_val = cpub->mem[info->effective_addr]; // そのアドレスからデータを読み込む
            printf("DEBUG(Phase 3): OpB from Abs Prog 0x%03x: 0x%02x.\n", info->effective_addr, info->operand_b_val);
            break;
        case ADDR_MODE_ABS_DATA: // 絶対アドレス (データ領域) (d)
            info->instruction_word_2nd = cpub->mem[cpub->pc]; // 2語目をアドレスとして読み込む
            printf("DEBUG(Phase 3): Fetched Abs Data 2nd word: 0x%02x.\n", info->instruction_word_2nd);
            cpub->pc++; // PCをさらにインクリメント
            info->effective_addr = 0x100 | info->instruction_word_2nd; // データ領域1xxH
            info->operand_b_val = cpub->mem[info->effective_addr]; // そのアドレスからデータを読み込む
            printf("DEBUG(Phase 3): OpB from Abs Data 0x%03x: 0x%02x.\n", info->effective_addr, info->operand_b_val);
            break;
        case ADDR_MODE_IX_PROG: // インデックス修飾アドレス (プログラム領域) [IX+d]
            info->instruction_word_2nd = cpub->mem[cpub->pc];
            cpub->pc++;
            info->effective_addr = (cpub->ix + info->instruction_word_2nd) & 0xFF; // IX + d (桁上げ無視し8bitアドレスにする)
            info->effective_addr = info->effective_addr & 0x1FF; // 9ビットアドレス化は後ほど。ここはプログラム領域なので、0x0xxで考える。
            info->operand_b_val = cpub->mem[info->effective_addr];
            printf("DEBUG(Phase 3): OpB from IX Prog 0x%03x: 0x%02x (IX=0x%02x, d=0x%02x).\n", info->effective_addr, info->operand_b_val, cpub->ix, info->instruction_word_2nd);
            break;
        case ADDR_MODE_IX_DATA: // インデックス修飾アドレス (データ領域) (IX+d)
            info->instruction_word_2nd = cpub->mem[cpub->pc];
            cpub->pc++;
            info->effective_addr = (cpub->ix + info->instruction_word_2nd) & 0xFF; // IX + d (桁上げ無視し8bitアドレスにする)
            info->effective_addr = 0x100 | info->effective_addr; // データ領域1xxH
            info->operand_b_val = cpub->mem[info->effective_addr];
            printf("DEBUG(Phase 3): OpB from IX Data 0x%03x: 0x%02x (IX=0x%02x, d=0x%02x).\n", info->effective_addr, info->operand_b_val, cpub->ix, info->instruction_word_2nd);
            break;
        case ADDR_MODE_NONE:
            printf("DEBUG(Phase 3): No operand B to fetch for this addressing mode.\n");
            info->operand_b_val = 0; // デフォルト値
            break;
        default:
            fprintf(stderr, "Operand Fetch Error: Unsupported addressing mode: %d\n", info->addr_mode_b);
            info->operand_b_val = 0; // エラー値
            info->type = INST_UNKNOWN; // 命令を無効化
            break;
    }
    printf("DEBUG(Phase 3): Exiting fetch_operands.\n");
}


/*=============================================================================
 * Phase 4: Execute ALU Operation (演算実行)
 *===========================================================================*/
static void execute_alu_operation(Cpub *cpub, InstructionInfo *info) {
    printf("DEBUG(Phase 4): Entering execute_alu_operation.\n"); // フェーズ入り口

    if (info->type == INST_ST) {
        printf("DEBUG(Phase 4): ST instruction does not require ALU operation.\n");
    } else {
        printf("DEBUG(Phase 4): Unsupported instruction type %d.\n", info->type);
        info->type = INST_UNKNOWN;
    }

    printf("DEBUG(Phase 4): Exiting execute_alu_operation.\n");
}


/*=============================================================================
 * Helper: Update Flags for Arithmetic/Logic Operations
 *===========================================================================*/
// 算術/論理演算後のフラグ (CF, VF, NF, ZF) 更新の共通ロジック
// is_sub: 0 for Add/ADC, 1 for Sub/SBC/CMP
static void update_flags_for_arith_logic(Cpub *cpub, Uword old_val_a, Uword old_val_b, Uword alu_result, int is_sub) {
    printf("DEBUG(Flags Update): Calculating flags for OpA=0x%02x, OpB=0x%02x, Result=0x%02x, IsSub=%d\n", old_val_a, old_val_b, alu_result, is_sub);

    // ZF (Zero Flag): 結果が0ならセット
    cpub->zf = (alu_result == 0) ? 1 : 0;
    printf("DEBUG(Flags Update): ZF=%d\n", cpub->zf);

    // NF (Negative Flag): 結果の最上位ビットが1ならセット
    cpub->nf = (alu_result & 0x80) ? 1 : 0;
    printf("DEBUG(Flags Update): NF=%d\n", cpub->nf);

    // CF (Carry Flag / Borrow Flag): 符号なし演算の桁上げ/借り
    if (!is_sub) { // 加算 (ADD/ADC)
        cpub->cf = (((unsigned int)old_val_a + (unsigned int)old_val_b) > 0xFF) ? 1 : 0;
        printf("DEBUG(Flags Update): CF (Add)=%d\n", cpub->cf);
    } else { // 減算 (SUB/SBC/CMP)
        // 借りが発生した場合 (old_val_a < old_val_b + CF(if SBC)) にCF=1 (Borrow)
        // PDFの図2のように、減算を2の補数での加算として処理する場合、
        // キャリーが発生したらCF=0、しなかったらCF=1（借りが発生した）
        // 正確な2の補数減算のCFは、(A + ~B + 1) のキャリーアウトの反転
        unsigned int temp_b = (unsigned int)~old_val_b; // ~B
        unsigned int sum = (unsigned int)old_val_a + temp_b + 1; // A + (~B) + 1
        cpub->cf = !((sum >> 8) & 0x01); // キャリーアウトの反転
        printf("DEBUG(Flags Update): CF (Sub/Borrow)=%d (sum=0x%x)\n", cpub->cf, sum);
    }

    // VF (Overflow Flag): 符号あり演算のオーバーフロー
    if (!is_sub) { // 加算
        // 正+正 = 負 になる場合 OR 負+負 = 正 になる場合
        cpub->vf = (((old_val_a & 0x80) == (old_val_b & 0x80)) && ((old_val_a & 0x80) != (alu_result & 0x80))) ? 1 : 0;
        printf("DEBUG(Flags Update): VF (Add)=%d (OpA_sign=%d, OpB_sign=%d, Result_sign=%d)\n", cpub->vf, (old_val_a & 0x80) != 0, (old_val_b & 0x80) != 0, (alu_result & 0x80) != 0);
    } else { // 減算 (A - B = A + (-B))
        // 減算をA + (-B)と見なし、-Bの符号を考慮して加算オーバーフローと同じロジックを適用
        Uword neg_b = ~old_val_b + 1; // 2の補数
        cpub->vf = (((old_val_a & 0x80) == (neg_b & 0x80)) && ((old_val_a & 0x80) != (alu_result & 0x80))) ? 1 : 0;
        printf("DEBUG(Flags Update): VF (Sub)=%d (OpA_sign=%d, NegB_sign=%d, Result_sign=%d)\n", cpub->vf, (old_val_a & 0x80) != 0, (neg_b & 0x80) != 0, (alu_result & 0x80) != 0);
    }

    printf("DEBUG(Flags Update): Final Flags: CF=%d, VF=%d, NF=%d, ZF=%d\n", cpub->cf, cpub->vf, cpub->nf, cpub->zf);
}


/*=============================================================================
 * Phase 5: Write Back Result (結果書き込み)
 *===========================================================================*/
static void write_back_result(Cpub *cpub, InstructionInfo *info) {
    printf("DEBUG(Phase 5): Entering write_back_result for type %d.\n", info->type); // フェーズ入り口

    if (info->type == INST_ST) {
        // Reg Aの内容をメモリに書き込む (effective_addrはfetch_operandsで計算済み)
        if (info->result_dest_reg_ptr != NULL) {
            Uword data_to_store = *(info->result_dest_reg_ptr);
            cpub->mem[info->effective_addr] = data_to_store;
            printf("DEBUG(Phase 5): Stored 0x%02x (from Reg A) to Memory address 0x%03x.\n", data_to_store, info->effective_addr);
        } else {
            fprintf(stderr, "DEBUG(Phase 5): Error: result_dest_reg_ptr is NULL for ST instruction.\n");
        }
    } else {
        printf("DEBUG(Phase 5): Unsupported instruction type %d for write back.\n", info->type);
    }

    printf("DEBUG(Phase 5): Exiting write_back_result.\n");
}


/*=============================================================================
 * Phase 6: Update Program Counter (PC更新)
 *===========================================================================*/
static void update_program_counter(Cpub *cpub, InstructionInfo *info) {
    printf("DEBUG(Phase 6): Entering update_program_counter for type %d. PC before update: 0x%03x.\n", info->type, cpub->pc); // フェーズ入り口
    // 分岐命令の条件判定とPC更新ロジック
    switch (info->type) {
        case INST_Bbc:
            printf("DEBUG(Phase 6): Evaluating Branch Condition (0x%x).\n", GET_BRANCH_CONDITION(info->instruction_word_1st));
            switch (GET_BRANCH_CONDITION(info->instruction_word_1st)) { // 表4のbcフィールド
                case 0x00: info->is_branch_taken = 1; printf("DEBUG(Phase 6): BA (Always) condition met.\n"); break; // BA (Always)
                case 0x08: info->is_branch_taken = cpub->vf; printf("DEBUG(Phase 6): BVF (on oVerFlow) condition: %d.\n", cpub->vf); break; // BVF (on oVerFlow)
                case 0x02: info->is_branch_taken = (cpub->zf == 0); printf("DEBUG(Phase 6): BNZ (on Not Zero) condition: ZF=%d (is_taken=%d).\n", cpub->zf, info->is_branch_taken); break; // BNZ (on Not Zero)
                case 0x09: info->is_branch_taken = (cpub->zf == 1); printf("DEBUG(Phase 6): BZ (on Zero) condition: ZF=%d (is_taken=%d).\n", cpub->zf, info->is_branch_taken); break; // BZ (on Zero)
                case 0x04: info->is_branch_taken = (cpub->nf == 0); printf("DEBUG(Phase 6): BZP (on Zero or Positive) condition: NF=%d (is_taken=%d).\n", cpub->nf, info->is_branch_taken); break; // BZP (on Zero or Positive)
                case 0x01: info->is_branch_taken = (cpub->nf == 1); printf("DEBUG(Phase 6): BN (on Negative) condition: NF=%d (is_taken=%d).\n", cpub->nf, info->is_branch_taken); break; // BN (on Negative)
                case 0x06: info->is_branch_taken = ((cpub->nf == 0) && (cpub->zf == 0)); printf("DEBUG(Phase 6): BP (on Positive) condition: NF=%d, ZF=%d (is_taken=%d).\n", cpub->nf, cpub->zf, info->is_branch_taken); break; // BP (on Positive) - NF=0 AND ZF=0
                case 0x0B: info->is_branch_taken = ((cpub->nf == 1) || (cpub->zf == 1)); printf("DEBUG(Phase 6): BZN (on Zero or Negative) condition: NF=%d, ZF=%d (is_taken=%d).\n", cpub->nf, cpub->zf, info->is_branch_taken); break; // BZN (on Zero or Negative) - NF=1 OR ZF=1
                // NI, NO はIOBUF_FLG_IN/OUTの状態を見る。ここでは未実装。
                case 0x05: info->is_branch_taken = (cpub->cf == 0); printf("DEBUG(Phase 6): BNC (on No Carry) condition: CF=%d (is_taken=%d).\n", cpub->cf, info->is_branch_taken); break; // BNC (on No Carry)
                case 0x0D: info->is_branch_taken = (cpub->cf == 1); printf("DEBUG(Phase 6): BC (on Carry) condition: CF=%d (is_taken=%d).\n", cpub->cf, info->is_branch_taken); break; // BC (on Carry)
                case 0x0A: info->is_branch_taken = ((cpub->vf ^ cpub->nf) == 0); printf("DEBUG(Phase 6): BGE (on Greater than or Equal) condition: VF=%d, NF=%d (is_taken=%d).\n", cpub->vf, cpub->nf, info->is_branch_taken); break; // BGE (on Greater than or Equal)
                case 0x0E: info->is_branch_taken = ((cpub->vf ^ cpub->nf) == 1); printf("DEBUG(Phase 6): BLT (on Less Than) condition: VF=%d, NF=%d (is_taken=%d).\n", cpub->vf, cpub->nf, info->is_branch_taken); break; // BLT (on Less Than)
                case 0x07: info->is_branch_taken = (((cpub->vf ^ cpub->nf) == 0) && (cpub->zf == 0)); printf("DEBUG(Phase 6): BGT (on Greater Than) condition: VF=%d, NF=%d, ZF=%d (is_taken=%d).\n", cpub->vf, cpub->nf, cpub->zf, info->is_branch_taken); break; // BGT (on Greater Than)
                case 0x0F: info->is_branch_taken = (((cpub->vf ^ cpub->nf) == 1) || (cpub->zf == 1)); printf("DEBUG(Phase 6): BLE (on Less Than or Equal) condition: VF=%d, NF=%d, ZF=%d (is_taken=%d).\n", cpub->vf, cpub->nf, cpub->zf, info->is_branch_taken); break; // BLE (on Less Than or Equal)
                default: info->is_branch_taken = 0; fprintf(stderr, "DEBUG(Phase 6): Branch: Unknown condition 0x%x\n", GET_BRANCH_CONDITION(info->instruction_word_1st)); break;
            }

            if (info->is_branch_taken) {
                cpub->pc = info->effective_addr; // フェッチ済みの分岐先アドレスにPCを設定
                printf("DEBUG(Phase 6): Branch taken to 0x%03x.\n", cpub->pc);
            } else {
                // 分岐しない場合、PCは既に命令フェッチと2語目フェッチで更新済みなので何もしない
                printf("DEBUG(Phase 6): Branch NOT taken. PC remains at 0x%03x.\n", cpub->pc);
            }
            break;
        case INST_JAL:
            // JAL命令は常に分岐
            // 戻りアドレス (PC+2) はWriteBackでACCに書き込まれる
            cpub->pc = info->effective_addr; // フェッチ済みの分岐先アドレスにPCを設定
            printf("DEBUG(Phase 6): JAL: Jumped to 0x%03x. Return address 0x%02x stored in ACC.\n", cpub->pc, info->pc_at_fetch + 2);
            break;
        case INST_JR:
            // ACCの内容をPCに転送
            cpub->pc = cpub->acc;
            printf("DEBUG(Phase 6): JR: Jumped to 0x%03x (from ACC 0x%02x).\n", cpub->pc, cpub->acc);
            break;
        // NOP, HLT, LD, ST, ADD などは命令フェッチ時にPCが既に更新済み
        default:
            printf("DEBUG(Phase 6): PC already updated by fetch for instruction type: %d. Final PC: 0x%03x.\n", info->type, cpub->pc);
            break;
    }
    printf("DEBUG(Phase 6): Exiting update_program_counter.\n");
}

