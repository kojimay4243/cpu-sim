/*
 *	Project-based Learning II (CPU)
 *
 *	Program:	instruction set simulator of the Educational CPU Board
 *	File Name:	cpuboard.c
 *	Descrioption:	simulation(emulation) of an instruction
 */

 #include	"cpuboard.h"
 #include	<stdio.h> // printfなどを使用する場合に必要
 
 
 /*=============================================================================
  * Macros for Instruction Decoding
  *===========================================================================*/
 // 命令語のビットパターンからOPコード、Aフィールド、Bフィールドを抽出するマクロ
 #define GET_OPCODE(inst)    (((inst) >> 4) & 0x0F) // 上位4ビットのOPコード
 #define GET_A_FIELD(inst)   (((inst) >> 3) & 0x01) // Aフィールド (1ビット)
 #define GET_B_FIELD(inst)   ((inst) & 0x07)        // Bフィールド (3ビット)
 
 // OPコードのマクロ定義 (表4より抜粋、一部例示)
 #define NOP_OPCODE_BYTE     0x00 // NOP命令の1語目全体
 #define HLT_OPCODE_BYTE     0x0F // HLT命令の1語目全体
 
 
 /*=============================================================================
  * Simulation of a Single Instruction
  *===========================================================================*/
 int
 step(Cpub *cpub)
 {
	 Uword instruction_word_1st; // 命令の1語目
	 Uword instruction_word_2nd; // 命令の2語目 (可変長命令の場合)
	 Uword operand_b_val;        // オペランドBの値 (データまたはアドレス)
	 Bit a_field;                // Aフィールドの値
	 Bit b_field;                // Bフィールドの値
	 Uword *reg_a_ptr = NULL;    // オペランドAのレジスタへのポインタ
	 Uword *reg_b_ptr = NULL;    // オペランドBのレジスタへのポインタ (レジスタ指定の場合)
	 Uword data_b;               // オペランドBから取得したデータ
	 Addr effective_addr;        // 実効アドレス
 
 
	 // 1. 命令フェッチ (P0, P1フェーズ)
	 // 現在のPCが指すメモリから1語目を読み込む
	 instruction_word_1st = cpub->mem[cpub->pc];
	 printf("DEBUG: PC=0x%03x, Instruction=0x%02x\n", cpub->pc, instruction_word_1st); // デバッグ出力
	 cpub->pc++; // PCを次のアドレスへインクリメント
 
	 // 2. 命令解読 (OPコード、A/Bフィールドの抽出)
	 a_field = GET_A_FIELD(instruction_word_1st);
	 b_field = GET_B_FIELD(instruction_word_1st);
 
	 // オペランドAのレジスタ選択 (Aフィールドが0ならACC、1ならIX)
	 if (a_field == 0) {
		 reg_a_ptr = &(cpub->acc); // ACCを選択
		 printf("DEBUG: Operand A is ACC\n");
	 } else {
		 reg_a_ptr = &(cpub->ix);  // IXを選択
		 printf("DEBUG: Operand A is IX\n");
	 }
 
	 // 命令の種類とアドレッシングモードによる処理の分岐
	 // 主に1語目のOPコードとBフィールドで判断
	 switch (instruction_word_1st & 0xF0) { // 上位4ビット (OPコードのプレフィックス) で大まかに分類
		 case NOP_OPCODE_BYTE & 0xF0: // NOP, HLT
			 if (instruction_word_1st == NOP_OPCODE_BYTE) {
				 printf("NOP instruction executed.\n");
				 // NOPは何もしない
			 } else if (instruction_word_1st == HLT_OPCODE_BYTE) {
				 printf("HLT instruction executed. Program Halted.\n");
				 return RUN_HALT; // HLT命令はシミュレータを停止させる
			 } else {
				 fprintf(stderr, "Unknown NOP/HLT related instruction: 0x%02x\n", instruction_word_1st);
				 return RUN_HALT; // エラーとして停止
			 }
			 break;
 
		 case LD_OPCODE_PREFIX: // LD 命令 (0110 A B)
			 // ここにLD命令のアドレッシングモードごとの処理を追加
			 // 今回の例ではレジスタ指定と絶対アドレス(データ領域)のみ考慮
 
			 // BフィールドによるオペランドBの解釈
			 if (b_field == 0x00) { // Bフィールド=000: ACC
				 printf("LD ACC/IX, ACC (not fully implemented)\n");
				 // LD ACC, ACC or LD IX, ACC (今回はACCからACCへのロードは意味がないが、パターンとして)
				 if (reg_a_ptr != NULL) {
					  *reg_a_ptr = cpub->acc; // 実際にはreg_b_ptrを使うべきだが、簡単のためACCを固定
				 }
			 } else if (b_field == 0x01) { // Bフィールド=001: IX
				 printf("LD ACC/IX, IX (not fully implemented)\n");
				 // LD ACC, IX or LD IX, IX
				  if (reg_a_ptr != NULL) {
					  *reg_a_ptr = cpub->ix; // 実際にはreg_b_ptrを使うべきだが、簡単のためIXを固定
				 }
			 } else if (b_field == 0x05) { // Bフィールド=101: (d) 絶対アドレス(データ領域)
				 // 2語目をフェッチ
				 instruction_word_2nd = cpub->mem[cpub->pc];
				 printf("DEBUG: 2nd Word=0x%02x\n", instruction_word_2nd);
				 cpub->pc++; // PCをさらにインクリメント
 
				 // 実効アドレスの計算 (データ領域のアドレスは上位1ビットが1)
				 effective_addr = 0x100 | instruction_word_2nd;
				 data_b = cpub->mem[effective_addr]; // メモリからデータを読み込む
 
				 // 結果をオペランドAのレジスタに書き込む
				 if (reg_a_ptr != NULL) {
					 *reg_a_ptr = data_b;
					 printf("LD instruction: Loaded 0x%02x from 0x%03x to Reg A.\n", data_b, effective_addr);
				 } else {
					  fprintf(stderr, "LD: Invalid Reg A ptr.\n");
				 }
			 } else {
				 fprintf(stderr, "LD: Unsupported B-field: 0x%x\n", b_field);
				 // 未サポートのアドレッシングモードはエラー
				 return RUN_HALT;
			 }
			 break;
 
		 case ST_OPCODE_PREFIX: // ST 命令 (0111 A B) 
			 // 今回の例ではACCから絶対アドレス(データ領域)のみ考慮 
 
			 if (b_field == 0x05) { // Bフィールド=101: (d) 絶対アドレス(データ領域) 
				 // 2語目をフェッチ 
				 instruction_word_2nd = cpub->mem[cpub->pc]; [cite: 5]
				 printf("DEBUG: 2nd Word=0x%02x\n", instruction_word_2nd);
				 cpub->pc++; // PCをさらにインクリメント
 
				 // 実効アドレスの計算 (データ領域のアドレスは上位1ビットが1) 
				 effective_addr = 0x100 | instruction_word_2nd; [cite: 10]
 
				 // オペランドAのレジスタの内容をメモリに書き込む 
				 if (reg_a_ptr != NULL) {
					 cpub->mem[effective_addr] = *reg_a_ptr; [cite: 15]
					 printf("ST instruction: Stored 0x%02x from Reg A to 0x%03x.\n", *reg_a_ptr, effective_addr);
				 } else {
					 fprintf(stderr, "ST: Invalid Reg A ptr.\n");
				 }
			 } else {
				 fprintf(stderr, "ST: Unsupported B-field or addressing mode: 0x%x\n", b_field);
				 return RUN_HALT;
			 }
			 break;
 
		 case ADD_OPCODE_PREFIX: // ADD 命令 (1011 A B)
			 // 今回はレジスタ指定のオペランドBのみ実装例とする
			 if (b_field == 0x00) { // Bフィールド=000: ACC
				 reg_b_ptr = &(cpub->acc);
				 printf("ADD instruction: Reg A (0x%02x) + ACC (0x%02x)\n", *reg_a_ptr, *reg_b_ptr);
				 // 演算実行
				 if (reg_a_ptr != NULL && reg_b_ptr != NULL) {
					 Uword result = *reg_a_ptr + *reg_b_ptr;
					 // フラグ更新 (CF/VF/NF/ZF) - ここに詳細なロジックを実装する
					 // 例: Z Flag (結果が0ならZF=1)
					 if (result == 0) cpub->zf = 1; else cpub->zf = 0;
					 // 例: Carry Flag (符号なし加算での桁上げ)
					 if ((unsigned int)*reg_a_ptr + (unsigned int)*reg_b_ptr > 0xFF) cpub->cf = 1; else cpub->cf = 0;
					 // 例: Negative Flag (結果の最上位ビットが1ならNF=1)
					 if ((result & 0x80) != 0) cpub->nf = 1; else cpub->nf = 0;
					 // Overflow Flag (VF) の計算は符号付き加算の場合で複雑なので省略
 
					 *reg_a_ptr = result; // 結果をReg Aに書き戻す
					 printf("ADD result: 0x%02x. Flags: CF=%d, VF=%d, NF=%d, ZF=%d\n", result, cpub->cf, cpub->vf, cpub->nf, cpub->zf);
				 } else {
					 fprintf(stderr, "ADD: Invalid Reg A/B ptr.\n");
				 }
			 } else if (b_field == 0x01) { // Bフィールド=001: IX
				  reg_b_ptr = &(cpub->ix);
				  printf("ADD instruction: Reg A (0x%02x) + IX (0x%02x)\n", *reg_a_ptr, *reg_b_ptr);
				  if (reg_a_ptr != NULL && reg_b_ptr != NULL) {
					 Uword result = *reg_a_ptr + *reg_b_ptr;
					 // フラグ更新
					 if (result == 0) cpub->zf = 1; else cpub->zf = 0;
					 if ((unsigned int)*reg_a_ptr + (unsigned int)*reg_b_ptr > 0xFF) cpub->cf = 1; else cpub->cf = 0;
					 if ((result & 0x80) != 0) cpub->nf = 1; else cpub->nf = 0;
					 *reg_a_ptr = result;
					 printf("ADD result: 0x%02x. Flags: CF=%d, VF=%d, NF=%d, ZF=%d\n", result, cpub->cf, cpub->vf, cpub->nf, cpub->zf);
				 } else {
					 fprintf(stderr, "ADD: Invalid Reg A/B ptr.\n");
				 }
			 } else if (b_field == 0x05) { // Bフィールド=101: (d) 絶対アドレス(データ領域) 
				 // 2語目をフェッチ 
				 instruction_word_2nd = cpub->mem[cpub->pc]; [cite: 5]
				 printf("DEBUG: 2nd Word=0x%02x\n", instruction_word_2nd);
				 cpub->pc++; // PCをさらにインクリメント
 
				 // 実効アドレスの計算 
				 effective_addr = 0x100 | instruction_word_2nd; [cite: 10]
				 data_b = cpub->mem[effective_addr]; // メモリからデータを読み込む 
 
				 printf("ADD instruction: Reg A (0x%02x) + (0x%03x) value (0x%02x)\n", *reg_a_ptr, effective_addr, data_b);
				 // 演算実行
				 if (reg_a_ptr != NULL) {
					 Uword result = *reg_a_ptr + data_b; [cite: 15]
					 // フラグ更新 (CF/VF/NF/ZF) - ここに詳細なロジックを実装する
					 if (result == 0) cpub->zf = 1; else cpub->zf = 0;
					 if ((unsigned int)*reg_a_ptr + (unsigned int)data_b > 0xFF) cpub->cf = 1; else cpub->cf = 0;
					 if ((result & 0x80) != 0) cpub->nf = 1; else cpub->nf = 0;
 
					 *reg_a_ptr = result; // 結果をReg Aに書き戻す 
					 printf("ADD result: 0x%02x. Flags: CF=%d, VF=%d, NF=%d, ZF=%d\n", result, cpub->cf, cpub->vf, cpub->nf, cpub->zf);
				 } else {
					 fprintf(stderr, "ADD: Invalid Reg A ptr.\n");
				 }
			 } else {
				 fprintf(stderr, "ADD: Unsupported B-field or addressing mode: 0x%x\n", b_field);
				 return RUN_HALT;
			 }
			 break;
			 
		 case EOR_OPCODE_PREFIX: // EOR 命令 (1100 A B) 
			  // 今回はレジスタ指定のオペランドBのみ実装例とする (EOR ACC, ACC)
			 if (b_field == 0x00) { // Bフィールド=000: ACC
				 reg_b_ptr = &(cpub->acc);
				 printf("EOR instruction: Reg A (0x%02x) ^ ACC (0x%02x)\n", *reg_a_ptr, *reg_b_ptr);
				 if (reg_a_ptr != NULL && reg_b_ptr != NULL) {
					 Uword result = *reg_a_ptr ^ *reg_b_ptr; [cite: 15]
					 // フラグ更新 (NF/ZF) 
					 if (result == 0) cpub->zf = 1; else cpub->zf = 0; [cite: 34]
					 if ((result & 0x80) != 0) cpub->nf = 1; else cpub->nf = 0; [cite: 34]
					 cpub->cf = 0; // EORはCFとVFに影響しないため、変化なしまたは0にリセット
					 cpub->vf = 0;
 
					 *reg_a_ptr = result; // 結果をReg Aに書き戻す 
					 printf("EOR result: 0x%02x. Flags: CF=%d, VF=%d, NF=%d, ZF=%d\n", result, cpub->cf, cpub->vf, cpub->nf, cpub->zf);
				 } else {
					 fprintf(stderr, "EOR: Invalid Reg A/B ptr.\n");
				 }
			 } else {
				 fprintf(stderr, "EOR: Unsupported B-field or addressing mode: 0x%x\n", b_field);
				 return RUN_HALT;
			 }
			 break;
 
		 // 他の命令のcase文をここに追加していく
		 // 例: SUB, CMP, AND, OR, Shift/Rotate, Branchなど
 
		 default:
			 fprintf(stderr, "Unknown instruction or unimplemented opcode: 0x%02x at 0x%03x\n", instruction_word_1st, cpub->pc - 1);
			 return RUN_HALT; // 未知の命令は停止させる
	 }
 
	 return RUN_STEP; // 1命令の実行が正常に完了した
 }
