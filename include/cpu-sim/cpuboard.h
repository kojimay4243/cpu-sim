/*
 *	Project-based Learning II (CPU)
 *
 *	Program:	instruction set simulator of the Educational CPU Board
 *	File Name:	cpuboard.h
 *	Descrioption:	resource definition of the educational computer board
 */

/*=============================================================================
 * Architectural Data Types
 *===========================================================================*/
 typedef signed char	Sword;
 typedef unsigned char	Uword;
 typedef unsigned short	Addr;
 typedef unsigned char	Bit;
 
 
 /*=============================================================================
  * CPU Board Resources
  *===========================================================================*/
 #define	MEMORY_SIZE	256*2
 #define	IMEMORY_SIZE	256
 
 typedef struct iobuf {
	 Bit	flag;
	 Uword	buf;
 } IOBuf;
 
 typedef struct cpuboard {
	 Uword	pc;
	 Uword	acc;
	 Uword	ix;
	 Bit	cf, vf, nf, zf;
	 IOBuf	*ibuf;
	 IOBuf	obuf;
	 /*
	  * [ add here the other CPU resources if necessary ]
	  */
	 Uword	mem[MEMORY_SIZE];	/* 0XX:Program, 1XX:Data */
 } Cpub;
 
 
 /*=============================================================================
  * Instruction Information Structure (新規追加)
  *===========================================================================*/
 
 // 命令の種類を定義する列挙型 (表1のマシン命令分類を参考に、主要なものを抜粋)
 typedef enum {
	 INST_NOP,
	 INST_HLT,
	 INST_OUT,
	 INST_IN,
	 INST_RCF,
	 INST_SCF,
	 INST_LD,
	 INST_ST,
	 INST_ADD,
	 INST_ADC,
	 INST_SUB,
	 INST_SBC,
	 INST_CMP,
	 INST_AND,
	 INST_OR,
	 INST_EOR,
	 INST_Ssm, // Shift 命令全般
	 INST_Rsm, // Rotate 命令全般
	 INST_Bbc, // Branch 命令全般
	 INST_JAL,
	 INST_JR,
	 INST_UNKNOWN // 未定義または未サポートの命令
 } InstructionType;
 
 // アドレッシングモードを定義する列挙型 (表2を参考に)
 typedef enum {
	 ADDR_MODE_REG_ACC,  // レジスタ指定 (ACC)
	 ADDR_MODE_REG_IX,   // レジスタ指定 (IX)
	 ADDR_MODE_IMMEDIATE, // 即値アドレス (d)
	 ADDR_MODE_ABS_PROG,  // 絶対アドレス (プログラム領域) ([d])
	 ADDR_MODE_ABS_DATA,  // 絶対アドレス (データ領域) ((d))
	 ADDR_MODE_IX_PROG,   // インデックス修飾アドレス (プログラム領域) ([IX+d])
	 ADDR_MODE_IX_DATA,   // インデックス修飾アドレス (データ領域) ((IX+d))
	 ADDR_MODE_NONE       // オペランドなし (例: NOP, HLT, RCF, SCF)
 } AddressingMode;
 
 // シフト/ローテートモードを定義する列挙型 (表3を参考に)
 typedef enum {
	 SHIFT_MODE_SRA,
	 SHIFT_MODE_SLA,
	 SHIFT_MODE_SRL,
	 SHIFT_MODE_SLL,
	 SHIFT_MODE_RRA,
	 SHIFT_MODE_RLA,
	 SHIFT_MODE_RRL,
	 SHIFT_MODE_RLL,
	 SHIFT_MODE_NONE
 } ShiftRotateMode;
 
 // 分岐条件を定義する列挙型 (表1を参考に)
 typedef enum {
	 BRANCH_COND_A,   // Always
	 BRANCH_COND_VF,  // on oVerFlow
	 BRANCH_COND_NZ,  // on Not Zero
	 BRANCH_COND_Z,   // on Zero
	 BRANCH_COND_ZP,  // on Zero or Positive
	 BRANCH_COND_N,   // on Negative
	 BRANCH_COND_P,   // on Positive
	 BRANCH_COND_ZN,  // on Zero or Negative
	 BRANCH_COND_NI,  // on No Input
	 BRANCH_COND_NO,  // on No Output
	 BRANCH_COND_NC,  // on No Carry
	 BRANCH_COND_C,   // on Carry
	 BRANCH_COND_GE,  // on Greater than or Equal
	 BRANCH_COND_LT,  // on Less Than
	 BRANCH_COND_GT,  // on Greater Than
	 BRANCH_COND_LE,  // on Less than or Equal
	 BRANCH_COND_NONE // 分岐条件なし (JAL, JRなど)
 } BranchCondition;
 
 
 // 命令の解読結果や実行に必要な情報を保持する構造体
 typedef struct {
	 InstructionType type;      // 命令の種類
	 Uword instruction_word_1st; // フェッチした命令の1語目
	 Uword instruction_word_2nd; // フェッチした命令の2語目 (2語命令の場合)
	 Addr pc_at_fetch;          // 命令フェッチ時のPCの値 (分岐計算などで使用)
 
	 // 命令のフィールド情報
	 Bit a_field;  // Aフィールドの値 (0:ACC, 1:IX)
	 Bit b_field;  // Bフィールドの値 (0-7:レジスタ, 8-15:即値, 16-23:絶対アドレス, 24-31:IX修飾)
	 AddressingMode addr_mode_b; // オペランドBのアドレッシングモード (Bフィールドから決定)
	 ShiftRotateMode shift_mode; // シフト/ローテート命令の場合のモード
	 BranchCondition branch_cond; // 分岐命令の場合の条件
 
	 // オペランドフェッチ後のデータ/アドレス情報
	 Uword operand_a_val;       // オペランドAの値 (レジスタから読み出し後)
	 Uword operand_b_val;       // オペランドBの値 (フェッチ後)
	 Addr effective_addr;       // メモリにアクセスするための実効アドレス (オペランドフェッチで計算)
 
	 // 演算実行後の結果情報
	 Uword alu_result;          // ALU演算の結果
	 Uword *result_dest_reg_ptr; // 結果を書き込むレジスタへのポインタ (NULLならメモリ)
	 Addr result_dest_mem_addr;  // 結果を書き込むメモリのアドレス (ST命令など)
 
	 // その他、特別な命令処理のためのフラグなど
	 int is_branch_taken; // 分岐が成立したか (BA, Bbc命令用)
 
 } InstructionInfo;
 
 
 /*=============================================================================
  * Top Function of an Instruction Simulation
  *===========================================================================*/
 #define	RUN_HALT	0
 #define	RUN_STEP	1
 int	step(Cpub *);
