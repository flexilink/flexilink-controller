// VM31.h : header file for VM3.1

/*#import <limits>
#define INT64T_MIN	(std::numeric_limits<int64_t>::min())
#define INT64T_MAX	(std::numeric_limits<int64_t>::max())
#define UINT64T_MAX (std::numeric_limits<uint64_t>::max())
#define INT32T_MIN	(std::numeric_limits<int32_t>::min())
#define INT32T_MAX	(std::numeric_limits<int32_t>::max())
#define UINT32T_MAX (std::numeric_limits<uint32_t>::max())
*/

// definitions for VM3.1, to be used by both compiler and interpreter; must be 
//		kept compatible with the VHDL code

// op codes and low-level formats include a '3' in the name; other definitions are 
//		the same as for the original, non-VHDL VM3

// VM's word length; must be a power of 2, and some code may assume you can fit 
//		two in a uint64_t without overflow
// entrypoint values are assumed to be 1 word long
// CodeFragment::IsShort and CodeFragment::IsShortSigned assume the elements of 
//		CodeFragment::param are the same size as VM words
// +++ <run_time_word_length> assumes VM_WORD_LENGTH = ROUTINE_SIZE
#define WORD_ADDR_ALIGN 5	// amount to shift bit address down to get word address
#define VM_WORD_LENGTH (1 << WORD_ADDR_ALIGN)
#define BIT_ADDR_MASK (VM_WORD_LENGTH - 1) // bit addr bits below the word addr
#define MAX_ARITH_SIZE 65535	// don't treat types longer than 64K words as arithmetic
#define MAX_ARITH_LEN (MAX_ARITH_SIZE * VM_WORD_LENGTH) // as above but as # bits
// size of "short" integer representation on stack: values are from MIN_SHORT_INT 
//		to MAX_SHORT_INT inclusive
#define MIN_SHORT_INT INT32T_MIN
#define MAX_SHORT_INT 0x5FFFFFFF

// other fixed values
// +++ <run_time_word_length> assumes ROUTINE_SIZE = VM_WORD_LENGTH
#define POINTER_SIZE			32	// bits in a pointer
#define ROUTINE_SIZE			32	// bits in an entrypoint (special signed int)
#define FIELDSPEC_SIZE			64	// bits in a fieldspec
#define MAX_PTRS_COUNT	  (1 << 22)	// max pointers size in a field = 128K words
#define MAX_PTRS_OFFSET	  (1 << 23)	// max pointers size in a record = 256K words
#define MAX_BITS_LENGTH	  (1 << 23)	// max bitstring length in a field = 1MB
#define MAX_BITS_OFFSET	  (1 << 27)	// max bitstring length in a record = 16MB

// addresses of the top bit of the operand stack and the bottom and top bits of 
//		the heap, including the "area" flags
// the dRAM contains, in increasing order of address: code; statics; frame stack; 
//		operand stack; heap; buffers for routing logic
// the areas occupied by the code and statics are sized after the program is 
//		compiled, so that they exactly fit; the code cache can only address the 
//		bottom 4MB, and the loader assumes code + statics fits in 14MB and 2MB 
//		is enough for its own stacks
// setting up of the heap and buffers is done by the VM code; we leave some 
//		version number etc information in the first few words of the heap area
// we leave 64 words between the stack and the heap, 16 as a "guard band" so we 
//		can see if stack underflow ever occurs and 48 for diagnostic information 
//		from the loader code, including the information that was previously output 
//		on the serial console port
// +++ currently in writes to SP, LP, GP, and FP, we check the area and that the 
//		value is in the bottom 8KB; we also check that a new value for FP is 
//		between LP and SP, and one for SP is above FP; also should check that YLB 
//		etc are within bounds when they point to the operand stack, and that any 
//		new value for LP is between GP and FP
#define VM3_LOADER_ADDRESS		0x01FFC000	// start of VM code (initial PC << 3)
#define VM3_FRAME_STACK_AREA	0x40000000	// address 0 in frame stack area
#define VM3_LOADER_STACK		0x47000000	// frame stack base for initial loader
#define VM3_STATICS_AREA		0x20000000	// address 0 in statics area
#define VM3_STATICS_CACHE_SIZE	  1024	// bytes in statics area of the cache
#define VM3_OPD_STACK_TOP	0x67FFFFE0	// word below heap area
#define VM3_HEAP_BASE		0x88000000	// 8MB (64Mb) for stacks, statics, & code

/* MicroBoard values
#define VM3_LOADER_ADDRESS		0x01FFC000	// start of VM code (initial PC << 3)
#define VM3_FRAME_STACK_AREA	0x40000000	// frame stack base for initial loader
#define VM3_LOADER_STACK		0x42000000	// frame stack base for initial loader
#define VM3_STATICS_AREA		0x20000000	// address 0 in statics area
#define VM3_STATICS_CACHE_SIZE	  1024	// bytes in statics area of the cache
#define VM3_OPD_STACK_TOP	0x63FFFE00	// 16 words below heap base
#define VM3_HEAP_BASE		0x84000000	// 8MB (64Mb) for stacks, statics, & code
#define VM3_HEAP_TOP		0x9FFFFFFF	// 56MB (448Mb) for heap
*/ 

// VM3.1 operations consist of a 6-bit op code and 2 to 34 bits which define n as 
//		either register m or an immediate value which is 32 bits + sign, though 
//		not all such values can be represented. The op code is looked up in the 
//		microcode ROM to find a sequence of instructions to be obeyed by the 
//		hardware.
// The processor contains an arithmetic and logic unit which implements instructions 
//		of the form x := y op (z << s) and x := z * s a 32-bit word at a time, and 
//		a register file. If the operands x and y are both in memory they must be at 
//		the same address; this supports op-and-becomes and the case where they are 
//		both on the stack. If the operation has an immediate value, that value is 
//		the y operand. Operand x can also be register m; if y is in the "statics" 
//		area then x must be register m.

// Note that an lvalue consists of one or two fieldspecs, so where the variable is 
//		a field of a record it points into the record but is not included in its 
//		use counts. There are some pathological cases where this could be unsafe, 
//		for instance "p->v +:= f(x)" if evaluation of f changes p and there are 
//		no other pointers to the record.
// +++ in VM3.1 we can get around this by evaluating it as: push f(x); push &(p->v); 
//		pop lvalue to x and y; add


// VM3.1 operations: op code values

// The ls 2 bits of the op code show what the "immediate" value is; see 
//		<ImmediateCoding> and <EmitOpn>.
// If d1=1 the immediate value is in 1 to 4 following bytes, and its sign is in 
//		d0; the n value during execution is the immediate value.
// If d1=0 the immediate value is 0 if d0=0, -1 else (so d0 is still its sign); 
//		the n value may be the immediate value or register m.
// Options for the immediate value are [0 -1 + - m]; the first four are the d1d0 
//		values from 0 to 3 and the last is an alternative for d1=0

// In the descriptions, x, y, z, and s are the ALU operands, and m is register m. 
// The '*' prefix indicates the contents of the word addressed by a register.

// P indicates pointers component
// B indicates bitstring component
// 2 indicates both components


// op codes which can be used with any coding in the ls 2 bits: [0 -1 + -]
// coded here with d1d0=11
// versions for [m] are generated by adding 0x60 to codes < 0x80, subtracting 
//		0x60 from codes > 0x80 (after clearing the ls 2 bits)
#define ADJUST_NO_IMM	0x60
// for an ALU operation, operand y is the immediate value if there is one; 
//		the [m] option indicates that y is in memory (on the stack if YMB has 
//		not been set)
// for other operations, n is register m if there is no immediate value
// MakefileDoc::PutFragment assumes ADD to XOR are related to the abstract VM op 
//		code (see ARITH_OP_MASK)
#define VM3_ADD			0x63 // x := y + (z << s); [0] not used
#define VM3_SUB			0x67 // x := y - (z << s)
#define VM3_AND			0x6B // x := y & (z << s); [0] not used
#define VM3_OR			0x6F // x := y | (z << s)
#define VM3_XOR			0x73 // x := y ^ (z << s)
#define VM3_MUL			0x77 // x := z * n
#define VM3_SETLEN_ZLP	0x7B // ZLP := ZFP + n unless < current ZLP; [-1 +] not used
#define VM3_SETLEN_YLP	0x7F // YLP := YFP + n unless < current YLP; [-1 +] not used
#define VM3_ZLP_ZFP		0xE3 // ZLP := ZFP + n; [-1 +] not used
#define VM3_YLP_YFP		0xE7 // YLP := YFP + n; [-1 +] not used
#define VM3_ZLB_ZMB		0xEB // ZLB := ZMB + n; [-1 -] not used
#define VM3_YLB_YMB		0xEF // YLB := YMB + n; [-1 -] not used
#define VM3_ZMB_PLING_N	0xF3 // ZMB := z + n; z must be an aligned word
// +++ NOTE: VM3_SET_S currently does the wrong thing if n > (64 + size of y) 
//		and x and z are on the stack; the end effect is to push zero.
#define VM3_SET_S		0xF7 // s := n; [0] not used
#define VM3_M_N			0xFB // m := n; [m] not used
#define VM3_PUSH_WORD	0xFF // *--SP := n
// for VM3_PUSH_WORD: OP_LITERAL uses all, SetEmpty uses [0], PushNewRecord uses 
//		[0 + m], rest only use [m]


// op codes for which d0 must be 0: [m +]
// coded here with d1d0=00
#define VM3_ILLEGAL_OPN 0x00 // causes a VM error trap
#define VM3_FPUSH		0x04 // FP +:= n
#define VM3_Q_LP		0x08 // Q := LP + n
#define VM3_ZMB_P		0x0C // ZMB := p + n
#define VM3_ZMB_LP		0x10 // ZMB := LP + n
#define VM3_ZMB_GP		0x14 // ZMB := GP + n
#define VM3_ZMB_P_U		0x18 // ZMB := p + n; set z unsigned
#define VM3_ZMB_LP_U	0x1C // ZMB := LP + n; set z unsigned
#define VM3_ZMB_GP_U	0x20 // ZMB := GP + n; set z unsigned
#define VM3_OFFSET_ZMB	0x24 // ZMB +:= n unless n < 0
#define VM3_SETLEN_ZLB	0x28 // ZLB := ZMB + n unless > current ZLB
#define VM3_YMB_P		0x2C // YMB := p + n
#define VM3_YMB_LP		0x30 // YMB := LP + n
#define VM3_YMB_GP		0x34 // YMB := GP + n
#define VM3_OFFSET_YMB	0x38 // YMB +:= n unless n < 0
#define VM3_SETLEN_YLB	0x3C // YLB := YMB + n unless > current YLB
#define VM3_OFFSET_Q_X	0x40 // *Q +:= n if n>0 (*Q is an address; resets ALU)
#define VM3_Q_GP		0x44 // Q := GP + n
#define VM3_Q_P			0x48 // Q := p + n
#define VM3_LOW_LEVEL	0x4C // I/O operation n, data in m [0 +]
#define VM3_CLR_MEMORY	0x50 // pop y; x := 2 & 0 +++ n must be (2 * wordlength)


// op codes for which d0 must be 1: [m -]
// coded here with d1d0=01
#define VM3_ZFP_P		0x01 // ZFP := p + n
#define VM3_ZFP_LP		0x05 // ZFP := LP + n
#define VM3_ZFP_GP		0x09 // ZFP := GP + n
#define VM3_OFFSET_ZFP	0x0D // ZFP +:= n unless n >= 0

#define VM3_YFP_P		0x15 // YFP := p + n
#define VM3_YFP_LP		0x19 // YFP := LP + n
#define VM3_YFP_GP		0x1D // YFP := GP + n
#define VM3_OFFSET_YFP	0x21 // YFP +:= n unless n >= 0

#define VM3_Q_Q			0x29 // Q := Q + n
#define VM3_P_P			0x2D // p := p + n
#define VM3_P_LP		0x31 // p := LP + n
#define VM3_P_GP		0x35 // p := GP + n
// 0x51 not available (occupied by end of VM3_CLR_MEMORY)

// op codes for which d1 must be 1: [+ -]
// coded here with d1d0=10
// MakefileDoc::PutFragment assumes conditional branch codings are are related to 
//		the abstract VM op code (see ARITH_OP_MASK)
// codes AE-DA can use 8 instructions instead of 4; _PUSH_INDEX is 8 instructions 
//		so needs to be here and hence needs 1 byte immediate value, which is the 
//		only reason we allow the register to be chosen
#define VM3_BR_EQ		0x82 // if m = 0 then PC +:= n
#define VM3_BR_LT		0x86 // if m < 0 then PC +:= n
#define VM3_BR_NE		0x8A // if m != 0 then PC +:= n
#define VM3_BR_GE		0x8E // if m >= 0 then PC +:= n
#define VM3_BR			0x92 // PC +:= n
#define VM3_CALL		0x96 // m := PC; PC +:= n; *(FP++) := m
#define VM3_PRE_CALL_1	0x9A // if m = 0 push one zero word and branch
#define VM3_PRE_CALL_2	0x9E // if m = 0 push two zero words and branch
#define VM3_MAKE_ENTRY	0xA2 // m := PC + n (make entrypoint)
#define VM3_COPY_TO_P	0xA6 // p := register (n & 31)
#define VM3_COPY_TO_Q	0xAA // Q := register (n & 31)
#define VM3_PUSH_REG	0xAE // *--SP := register (n & 31)
#define VM3_POP_REG		0xB2 // register (n & 31) := *(SP++)
#define VM3_ZMB_IP		0xB6 // ZMB := IP + n
#define VM3_YMB_IP		0xBA // YMB := IP + n
#define VM3_P_IP		0xBE // p := IP + n
#define VM3_FREE_RECORD	0xC2 // if TP -> heap: pop record, push p, and do VM3_CALL
#define VM3_FPUSH_CLR	0xC6 // *(FP++) := 0 (n/32 times) (uses ALU; n>0 always)
#define VM3_M_TO_REG	0xCA // register (n & 31) := m
#define VM3_REG_TO_M	0xCE // m := register (n & 31)
#define VM3_PUSH_INDEX	0xD2 // last part of OP_NEW_STRING code, using reg (n & 31)
	// +++ I don't think these are needed
//#define VM3_INC_STK_CT	0xD2 // increment stack count for pointer in reg (n & 31)
//#define VM3_DEC_STK_CT	0xD6 // decrement stack count for pointer in reg (n & 31)
#define VM3_PUSH_IP_N	0xDA // p := IP + n; *--SP := p


// op codes that don't use immediate values: treated by the VM as [m]
// coded as is; all have d1=0
#define VM3_X_IS_M		0x81 // ALU destination is register m
#define VM3_PUSH_LB_Z	0x85 // *--SP := ZLB; *--SP := ZMB; clear ALU state
#define VM3_PUSH_LP_Z	0x89 // *--SP := ZFP; *--SP := ZLP; clear PTRS state
#define VM3_DEREF_P		0x8D // p := *p
#define VM3_DEREF_Q		0x91 // p := *(Q - 1 word)
#define VM3_PUSH_L_Q	0x95 // *--SP := *--Q twice
#define VM3_POP_L_Q		0x99 // *(Q++) := *(SP++) twice
#define VM3_SET_Y_Q		0x9D // YLB := *--Q; YMB := *--Q
#define VM3_SET_YP_Q	0xA0 // YFP := *--Q; YLP := *--Q
#define VM3_SET_ZS_Q	0xA1 // ZLB := *--Q; ZMB := *--Q
#define VM3_SET_ZU_Q	0xA4 // ZLB := *--Q; ZMB := *--Q; set z unsigned
#define VM3_SET_ZP_Q	0xA5 // ZFP := *--Q; ZLP := *--Q
#define VM3_POP_M		0xA8 // m := *(SP++)
#define VM3_FPUSH_M		0xA9 // *(FP++) := m
#define VM3_COPY_P		0xAC // x := z (pointers)
#define VM3_ENTER		0xAD // xchg *(FP-1) with PC
#define VM3_NEW_FRAME	0xB0 // *FP := LP; LP := FP; FP +:= 1
// +++ current microcode for opcode B1 deletes pointers from the frame stack
//#define VM3_POP_PTRS	0xB1 // delete ptrs component from operand stack
#define VM3_RETURN		0xB4 // return from routine (including pop frame stack)
#define VM3_POP_LVALUE	0xB5 // pop bitstring, set Q -> word after last *
	// * assumes either empty (Q := NIL) or correct size (2 or 4 words); uses m
#define VM3_SWAP_STK_RP	0xB8 // xchg *SP with RP
#define VM3_SET_PQ_Q	0xB9 // q -> word after lvalue of index; sets (p,q) -> index
#define VM3_YMB_PLING_M	0xBC // YMB := y + m; y must be an aligned word
#define VM3_PLING_YMB	0xBD // YMB := !YMB
#define VM3_ADJ_COUNTS	0xC1 // adjust use counts for UP, ADJ_COUNT value on stack
#define VM3_INC_STK_P	0xC5 // increment stack count for pointer in reg p if to heap
#define VM3_LOADWORD_M	0xC9 // m := *ZMB; clear ALU state
#define VM3_STOREWORD_M	0xCD // *YMB := m; clear ALU state


// extracode numbers: index into <sys_rt_name> and <sys_rt_number>
// +++ may have to change the way the tables for switch instructions are built
// see declarations of the routines for more detail
#define VM3X_MIN		 0 // assumed in PutFragment
#define VM3X_MAX		 1 // assumed OP_MAX - OP_MIN in PutFragment
#define VM3X_DIVREM		 2
#define VM3X_DIV		 3 // assumed OP_DIV - OP_MIN in PutFragment
#define VM3X_REM		 4 // assumed OP_REM - OP_MIN in PutFragment
#define VM3X_ABS		 5
#define VM3X_NEW_REC_CLR 6 // Int(Int ptrs, Int bits) SysExNewRecClear
#define VM3X_FREE_REC	 7 // frees fragment processed by VM3_FREE_RECORD
#define VM3X_DEREF_X_VBL 8 // Int(Int p, Int q) SysExDerefIndexVble
#define VM3X_D_INC_X_VBL 9 // Int(Int p, q, len) SysExDerefIncIndexVble
#define VM3X_FROM_SUB_X	10 // (lv, Int p,q,len,inc) SysExCopyFromSubstrMoveIndex
#define VM3X_FROM_SUB	11 // (lv dest,indx, Int len) SysExCopyFromSubstring
#define VM3X_FIX_INDEX	12 // Int(lv) SysExFixIndex [returns M addr]
#define VM3X_TO_SUBSTR	13 // (lv srce,indx, Int len) SysExCopyToSubstring
#define VM3X_TO_SBSTR_X	14 // (lv, Int p,q,len,inc) SysExCopyToSubstrMoveIndex
#define VM3X_STKRESERVE	15 // §int[](Int len) SysExReserveOnStack
#define VM3X_COPY_SBSTR	16 // (§int[] slix, Int slen, dlix,dlen) SysExCopySubstring
#define VM3X_PREPEND_X	17 // void(Int p,q,len) SysExMoveIndexBack
#define VM3X_SUB_INDEX	18 // Int(lv p,q) SysExSubtractIndexes [p - q]
#define VM3X_LOOKUP_SW	19 // void(Int table) SysExLookupSwitch
#define VM3X_STACK_TOP	20 // §int[]() SysExTopOfStack
#define VM3X_POP_STACK	21 // void() SysExPopStack
//#define VM3X_LV_TO_INDX	?? // index void(lv indx) SysExLvalueToIndex [add hdr & ptr]

#define VM3_EXTRACODES	22 // size of table of extracodes
#define VM3_MAX_SYS_RT	12 // valid <systemroutine> numbers are 1 to 12


//			 d1						0 0/m	1 -1/m	 2 + 	 3 -
//
//		00-5F	 d7d6d5d4d3d2d0 	00-17R	00-17M	00-17R	00-17M
//		60-7F	 d7d6d5d4d3d2 0 	18-1FR	18-1FR	18-1FR	18-1FR
//		80-9D 0	 1 1 1 d4d3d2d0		38-3FR	38-3FM
//		A0-BD 0	 1 0 d0d4d3d2 0		20-27R	28-2FR
//		C0-DD 0	 0 1 1 d4d3d2d0		18-1FR	18-1FM
//		82-DF 1	 d7d6d5d4d3d2 1						20-37M	20-37M
//		E0-FF	 d7d6d5d4d3d2 0 	38-3FR	38-3FR	38-3FR	38-3FR

// register numbers
#define PTR_RECORD		 1	// pointer to the current record
#define PTR_GLOBAL		 2	// the global stack frame
#define PTR_LOCAL		 3	// current frame (local variables etc)

#define VM3REG_RP	PTR_RECORD
#define VM3REG_GP	PTR_GLOBAL
#define VM3REG_LP	PTR_LOCAL
#define VM3REG_IP		 4	// pivot of statics area
#define VM3REG_W		 7	// working-space; -> "addr of next" in OP_NEW_STRING
#define VM3REG_ZLP	   -16
#define VM3REG_ZMB	   -15
#define VM3REG_YLP	   -14
#define VM3REG_YMB	   -13
#define VM3REG_ZFP	   -12
#define VM3REG_ZLB	   -11
#define VM3REG_YFP	   -10
#define VM3REG_YLB		-9
#define VM3REG_TP		-8 // head of list heap fragments to be freed
#define VM3REG_FP		-7 // frame stack pointer
#define VM3REG_P		-6 // register p
#define VM3REG_SP		-5 // operand stack pointer
#define VM3REG_UP		-4
#define VM3REG_Q		-1


// I/O addresses which use 32-bit values that are not necessarily short form: in each 
//		case bit n applies to I/O address n
#define VM3_OUT_32_MASK_1	0x400  // ) outputs with d10 and one of d9d8 set
#define VM3_OUT_32_MASK_2	0x300  // )
#define VM3_IN_32_MASK		0x0080 // inputs: just 7 (flash data)
