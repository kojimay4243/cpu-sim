typedef signed char	Sword;
typedef unsigned char	Uword;
typedef unsigned short	Addr;
typedef unsigned char	Bit;
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
	Uword	mem[MEMORY_SIZE];
} Cpub;
 
 
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
	INST_Ssm,
	INST_Rsm,
	INST_Bbc,
	INST_JAL,
	INST_JR,
	INST_UNKNOWN
} InstructionType;
 
typedef enum {
	ADDR_MODE_REG_ACC,
	ADDR_MODE_REG_IX,
	ADDR_MODE_IMMEDIATE,
	ADDR_MODE_ABS_PROG,
	ADDR_MODE_ABS_DATA,
	ADDR_MODE_IX_PROG,
	ADDR_MODE_IX_DATA,
	ADDR_MODE_NONE
} AddressingMode;
 
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
 
typedef enum {
	BRANCH_COND_A,
	BRANCH_COND_VF,
	BRANCH_COND_NZ,
	BRANCH_COND_Z,
	BRANCH_COND_ZP,
	BRANCH_COND_N,
	BRANCH_COND_P,
	BRANCH_COND_ZN,
	BRANCH_COND_NI,
	BRANCH_COND_NO,
	BRANCH_COND_NC,
	BRANCH_COND_C,
	BRANCH_COND_GE,
	BRANCH_COND_LT,
	BRANCH_COND_GT,
	BRANCH_COND_LE,
	BRANCH_COND_NONE
} BranchCondition;
 
 
typedef struct {
	InstructionType type;
	Uword instruction_word_1st;
	Uword instruction_word_2nd;
	Addr pc_at_fetch;

	Bit a_field;
	Bit b_field;
	AddressingMode addr_mode_b;
	ShiftRotateMode shift_mode;
	BranchCondition branch_cond;

	Uword operand_a_val;
	Uword operand_b_val;
	Addr effective_addr;

	Uword alu_result;
	Uword *result_dest_reg_ptr;
	Addr result_dest_mem_addr;

	int is_branch_taken;

} InstructionInfo;
 
 
#define	RUN_HALT	0
#define	RUN_STEP	1
int	step(Cpub *);
