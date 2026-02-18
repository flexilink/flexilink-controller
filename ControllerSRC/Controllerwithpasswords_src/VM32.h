// VM32.h : header file for VM3.2
//
// Copyright (c) 2019-2020 Nine Tiles

// +++ this is a cut-down copy of the one in TeaLeaves/common; ideally this 
//		project should move into the TeaLeaves directory so it can share 
//		"common" code with the compiler

#pragma once

/*
// definitions for VM3.2, to be used by both compiler and interpreter; must 
//		be kept compatible with the VHDL code

// op codes and low-level formats include a '3' in the name; other 
//		definitions are the same as for the original, interpreted, VM3

// VM's word length; must be a power of 2, and some code may assume you can 
//		fit two in a uint64_t without overflow
// entrypoint values are assumed to be 1 word long
// +++ <run_time_word_length> assumes VM_WORD_LENGTH = ROUTINE_SIZE
// +++ there are quite a lot of places in the code where it's assumed there 
//		are 32 bits per word
#define WORD_ADDR_ALIGN 5 // amount to shift bit addr down to get word addr
#define VM_WORD_LENGTH (1 << WORD_ADDR_ALIGN)
#define BIT_ADDR_MASK (VM_WORD_LENGTH - 1) // bit addr bits below word addr
#define MAX_ARITH_SIZE 8192 // don't treat types > 8K words long as arith
#define MAX_ARITH_LEN (MAX_ARITH_SIZE * VM_WORD_LENGTH) // as above but #bits

// other fixed values
// +++ <run_time_word_length> assumes ROUTINE_SIZE = VM_WORD_LENGTH
// +++ in various places we assume POINTER_SIZE is the size of the pivot of 
//		a frame or record, hence the offset to the start of the bitstring
#define POINTER_SIZE			 32	// bits in a pointer
#define ROUTINE_SIZE			 32	// bits in an entrypoint
#define ROUTINE_SIZE_WORDS		  1	// words in an entrypoint (rounded up)
#define FIELDSPEC_PTRS_SIZE		 96	// ptrs fieldspec (3 words)
#define FIELDSPEC_BITS_SIZE		128	// bitstring fieldspec (4 words)
#define FIELDSPEC_BOTH_SIZE		224	// 2-component fieldspec (7 words)
#define STRINGSPEC_SIZE			 64	// bits per component in a stringspec
// max sizes are such that offsets are always codable as an immediate value
#define MAX_PTRS_LENGTH	  (1 << 22)	// max ptrs size in a field = 128K words
#define MAX_PTRS_OFFSET	  (1 << 23)	// max ptrs size in a record = 256K words
#define MAX_BITS_LENGTH	  (1 << 23)	// max bitstring length in a field = 1MB
#define MAX_BITS_OFFSET	  (1 << 27)	// max bitstring length in a record = 16MB
#define DEFAULT_STRING_LENGTH	120 // elements

// "immediate" codings for some of the above
#define IMM_WORD_LENGTH (0x282LL << 24) // word length (32, 0x20)
#define IMM_M_WORD_LEN  0x38EFFFFFFLL // minus word length (-32, 0xF...FE0)

// addresses of the top bit of the operand stack and the bottom and top bits 
//		of the heap, including the "area" flags
// the dRAM contains, in increasing order of address: code; statics; frame 
//		stack; operand stack; heap; buffers for routing logic
// the areas occupied by the code and statics are sized after the program is 
//		compiled, so that they exactly fit; the code cache can only address 
//		the bottom 4MB, and the loader assumes code + statics fits in 14MB 
//		and 2MB is enough for its own stacks
// setting up of the heap and buffers is done by the VM code; we leave some 
//		version number etc information in the first few words of the heap 
//		area
// we leave 64 words between the stack and the heap, 16 as a "guard band" 
//		so we can see if stack underflow ever occurs and 48 for diagnostic 
//		information from the loader code, including the information that 
//		was previously output on the serial console port
// +++ currently in writes to SP, LP, GP, and FP, we check the area and that 
//		the value is in the bottom 8KB; we also check that a new value for 
//		FP is between LP and SP, and one for SP is above FP; also should 
//		check that YLB etc are within bounds when they point to the operand 
//		stack, and that any new value for LP is between GP and FP
#define VM3_LOADER_ADDRESS		0x01FFC000	// start of VM code (init PC<<3)
#define VM3_FRAME_STACK_AREA	0x40000000	// address 0 in frame stack area
#define VM3_LOADER_STACK		0x47000000	// fr stk base for init loader
#define VM3_STATICS_AREA		0x20000000	// address 0 in statics area
#define VM3_STATICS_CACHE_SIZE	  1024	// bytes in statics area of cache
#define VM3_OPD_STACK_TOP	0x67FFFFE0	// word below heap area
#define VM3_HEAP_BASE		0x88000000	// 8MB (64Mb) for stacks, code, etc
#define VM3_AREA_MASK		0xE0000000	// "area" field except for heap
#define VM3_ADDRESS_MASK	0x1FFFFFFF	// except for heap

// VM3.2 operations consist of a 8-bit op code and up to 4 further octets 
//		that code an immediate value n which is 32 bits + sign, though not 
//		all such values can be represented. The op code defines from where 
//		in the microcode ROM to read a sequence of instructions to be 
//		executed by the hardware.
// See the VM3.2 specification document for details of the instruction set.

// Note that an lvalue consists of one or two fieldspecs, so where the 
//		variable is a field of a record it points into the record but is 
//		not included in its use counts. There are some pathological cases 
//		where this could be unsafe, for instance "p->v +:= f(x)" if 
//		evaluation of f changes p and there are no other pointers to the 
//		record.


// VM3.2 operations: op code values

// The ls 2 bits of the op code show what n (the "immediate" value) is; see 
//		the VM3.2 specification document for details.
// Here we regard the opcode as being made up of four 2-bit fields, each of 
//		which either can either have a "fixed" value or hold a parameter:
//
//	  d7-6: always fixed
//	  d5-4: base register or components: PTR_ code (_NIL = P) or VALUE_HAS_
//	  d3-2: destination or condition: DEST_ or COND_ code
//	  d1-0: immediate value: as required by the VM's logic (d1 = 0 if 
//				single-byte op code, 1 if "immediate" value follows)
//
// The VM3_ values #defined below are coded as:
//
//	   d25: implement VALUE_HAS_BOTH as _BITS followed by _PTRS
//	   d24: implement VALUE_HAS_BOTH as _PTRS followed by _BITS
//	d23-20: permissible values for "base register" parameter
//	d19-16: permissible values for "destination" parameter
//	d15-12: permissible values for "immediate value" parameter
//	   d11: "base register" field may be fixed
//	   d10: "destination" field may be fixed
//		d9: "immediate value" field may be fixed (with d1=0)
//		d8: instruction is a branch
//	  d7-0: opcode: any field containing a parameter is ignored
//
// "Permissible values" settings are 1 in bit n of the field if the 
//		parameter can take the value n, 0 else; parameter values are 0 to 3 
//		or -1 for "fixed". See <EmitOpn> for more detail.
// +++ currently bits 25 and 24 are never set; they're available for use if 
//		we run out of opcodes and need to withdraw the '2' option

#define VM_OP_BITS_FIRST (1<<25) // ignored if d23 or d24 set
#define VM_OP_PTRS_FIRST (1<<24) // ignored if d23 set
#define VM_OP_REG_P_SHIFT	20 // bit (20+n) is permission for base reg n
#define VM_OP_DEST_P_SHIFT	16 // bit (16+n) is permission for dest code n
#define VM_OP_IMM_P_SHIFT	12 // bit (12+n) is permission for imm code n
#define VM_OP_REG_FIXED	 0x800 // set if "base reg" field can be fixed
#define VM_OP_DEST_FIXED 0x400 // set if "dest" field can be fixed
#define VM_OP_IMM_FIXED	 0x200 // set if "imm value" field can be fixed
#define VM_OP_BRANCH	 0x100 // set for EmitBranch, clear for EmitOpn
#define VM_OP_REG_SHIFT		 4 // base reg field is d5-4
#define VM_OP_DEST_SHIFT	 2 // dest field is d3-2
#define VM_OP_BR_REQD	0xC902 // bits required set for <EmitBranch>

// op codes for VM3.2
// +++ 41 of the op codes begin with an instruction to pop the stack and 
//		trap if long form; we can save 40 locations in the microcode ROM 
//		by making that a separate operation, at the expense of adding a 
//		byte to the VM code for each occurrence
// note that the 's' option for VM3_SET_xx (not surrently used, I think) 
//		will be needed when <parameter> is implemented
//
// 01+-si are options for d1-0 (1 = -1; s = immediate data not used, 
//			d1 must be 0; i = immediate data required, currently is + 
//			but could redefine as - if required); EmitBranch requires +- 
//			though for _PRE_CALL_ the offset will always be +ve
// wyxz	  are options for d3-2 as dest reg (w = DEST_REGS)
// lnge	  are options for d3-2 as cond codes (respectively LT,NE,GE,EQ)
// PRGL   are options for d5-4 as base reg
// vpb2   are options for d5-4 as cpts (respectively void,ptrs,bits,both)
// The '*' prefix indicates the contents of the word addressed by a register.
// 'n' is a word popped from the stack (trap if not short form) if 's' 
//		specified, immediate data else
#define VM3_NO_OPN		0x0000E00 // s			no operation
#define VM3_ERROR_TRAP	0x0000E01 // s			causes a VM error trap
#define VM3_FREE_RECORD	0x000CD02 // +-			if TP -> heap: pop rec, push p, do VM3_CALL
#define VM3_ALU			0x000DC04 // 0+-		for imm value see ALU_xxx below
#define VM3_GET_XB_LEN	0x0000E05 // s			set cache o/p to (xb len - 1)
#define VM3_OFFSET_FB	0x00C4A08 // +s xz		xFB +:= n unless n < 0
#define VM3_END_TRAP_4	0x0000E09 // s			return from trap 4 extracode
#define VM3_END_TRAP_1	0x0000E0D // s			return from trap 1 extracode
#define VM3_BR			0x000CD12 // +-			PC +:= n
#define VM3_SET_LB		0x00E4E1C // +s yxz		xLB := xFB + n (defaults to opd z)
#define VM3_SET_LP		0x00C9A19 // -s xz		xLP := xFP + n
#define VM3_SETLEN_ZB_M	0x0000E21 // s			_SETLEN_B for opd z with value from cache
#define VM3_PRE_CALL_1	0x000CD22 // +-			if EQZ push one zero word and branch
#define VM3_LOAD_P_IQ	0x0000E24 // s			P := *Q
#define VM3_LOAD_Q_IW	0x0000E25 // s			Q := *W
#define VM3_FPUSH_CLR	0x0004C26 // +			*(FP++) := 0 n/32 times (n > 64)
#define VM3_SETLEN_P	0x00C9A29 // 0-s xz		xLP := xFP + n unless < current xLP
#define VM3_SET_SP		0x0004C2A // +			SP := imm
#define VM3_SET_LP_FP	0x0004C2E // +			LP := imm; FP := LP+1
#define VM3_XB_FROM_Z	0x0000E30 // s			copy zb lvalue to xb
//#define VM3_ZB_FROM_X	0x0000E31 // s			copy xb lvalue to zb
#define VM3_PRE_CALL_2	0x000CD32 // +-			if EQZ push two zero words and branch
#define VM3_POP_QK_ZB	0x0000E34 // s
#define VM3_POP_QK_YB	0x0000E35 // s
#define VM3_FPUSH		0x0004C36 // +			FP +:= n
#define VM3_SHIFT_X		0x000CE40 // +-s		shift x by n
#define VM3_POP_TO_W	0x0000E41 // s			pop stack, set W -> ms bit or NIL if short form
#define VM3_PUSH_FS_Z	0x0E00650 // s pb2		push z on opd stk as fieldspec
#define VM3_Z_TO_FS		0x0E00651 // s pb2		store lvalue of z; W -> fieldspec (move to word after)
#define VM3_MAKE_ENTRY	0x000CD52 // +-			push PC + n (make entrypoint)
#define VM3_COPY_RVALUE	0x0E00654 // 0 pb2		pop opd stk to z & copy, chk x for substr
#define VM3_COPY_VBLE	0x0E00658 // 0 pb2		copy z to x; chk for substr
#define VM3_FPUSH_CLR_2	0x0000E59 // s			*(FP++) := 0 twice
#define VM3_STATUS_TO_Z	0x000DC5C // 0+-		set zb to result of low level opn
#define VM3_FPUSH_CLR_1	0x0000E5D // s			*(FP++) := 0
#define VM3_PUSH_IP_N	0x000CD62 // +-			push (IP + n) [for VM3X_LOOKUP_SW]
#define VM3_B_OPD_FS_S	0x04C0269 // s xz b		as VM3_OPD_FROM_FS with trap if substring
#define VM3_INPUT_TO_Z	0x000DC6C // 0+-		set zb to result of low level opn & acknowledge
#define VM3_W_W			0x0004C72 // +			W +:= n
#define VM3_SETLEN_B_S	0x00C4A79 // +s xz		as VM3_SETLEN_B with trap if substring
#define VM3_ERASE		0x0000E80 // s			pop stack, discard value
#define VM3_POP_ZB		0x0000E81 // s
#define VM3_CALL		0x000CD82 // +-			*(FP++) := PC; PC +:= n
#define VM3_POP_ZP		0x0000E84 // s
#define VM3_ENTER		0x0000E85 // s			xchg *FP with PC
#define VM3_PUSH_REG	0x0004C86 // i			*--r1 := r2
#define VM3_SETLEN_B	0x00C4A88 // +s xz		xLB := xMB + n unless > current xLB
#define VM3_OFFSET_FP	0x00C8A89 // -s xz		xFP +:= n unless n >= 0
#define VM3_PUSH_WORD	0x000FC90 // 01+-		*--SP := n
#define VM3_NEW_FRAME	0x0000E94 // s			*FP := LP; LP := FP; FP +:= 1
#define VM3_SET_Z		0x000EC95 // 1+-		set zb to be n
#define VM3_OPD_FROM_FS	0x0EC0298 // s xz pb2	W -> fieldspec; could use _PTRS_FIRST for 2
#define VM3_OPD_FROM_SS	0x0EC0299 // s xz pb2	W -> stringspec; could use _PTRS_FIRST for 2
#define VM3_POP_REG		0x0004C9A // i			r1 := *(SP++)
#define VM3_COPY_REG	0x0004C9E // i			r1 := r2
#define VM3_SET_J		0x000CEA0 // +-s		set j to be n
#define VM3_SWAP_STK_RP	0x0000EA1 // s			xchg *SP with RP
//#define VM3_COPY_P		0x0000EA4 // 0			x := z (pointers; imm must be zero)
#define VM3_RETURN		0x0000EA5 // s			return from routine (including pop frame stack)
#define VM3_SET_MB_IP	0x00AC9A6 // +- yz		xMB := IP + n
#define VM3_CONTROL		0x000CCAA // +-			<vm_operation>, "y" (if present) from mem
//#define VM3_COPY_BOTH	0x0000EB0 // 0			ALU followed by PTRS
#define VM3_RESET_ALU	0x0000EB1 // s			reset ALU
#define VM3_BR_COND		0x00FC9B2 // +- lnge	if [condition] then PC +:= n
#define VM3_ADJ_COUNTS	0x0000EB4 // s			adjust use counts for P, ADJ_COUNT value on stack
#define VM3_POP_WORD	0x0000EB5 // s			ls word if long form (for "y" to VM3_CONTROL)
#define VM3_SET_FB		0x0FF42C0 // +s wyxz PRGL xFB/W := reg + n
#define VM3_SET_FP		0x0FD82C1 // -s wxz PRGL  xFP/Q := reg + n
#define VM3_POP_TO_Q	0x0000EC5 // s			pop stack, set Q -> ms bit, trap if long form
#define VM3_STORE_P_IQ	0x0000ED5 // s			*Q := P, incrementing use count
#define VM3_X_ON_FSTK	0x0000EE5 // s			XFB := FP (as "set F") [for pre-call]
#define VM3_LOAD_W_IW	0x0000EF5 // s			W := *W

#define VM3OP_ERROR		0x01 // 8-bit op code for VM error trap
#define VM3OP_SET_XLB_S	0x18 // 8-bit op code for VM3_SET_LB for x from stack
#define VM3OP_SETLEN_XS	0x88 // 8-bit op code for VM3_SETLEN_B for x from stack
#define VM3OP_SET_XB	0xA8 // 8-bit op code for VM3_OPD_FROM_FS for xb

// components of the n value that accompanies an ALU operation; defaults are 
//		(signed) operand in memory; only include one each of _Y_ and _OP_
// for single-operand cases, we use _OR_NOT for "not", _SUB for "neg", _ADD 
//		for "copy"; y is zero except in the "copy" case where y can be 
//		immediate and z zero
// to copy from z to x we use the all-zero version, in which y is selected 
//		as "signed, as programmed" but defaults to <nil>; _COPY_Z is the 
//		value to pass to AddVmOpn() which in this case is zero but if (for 
//		instance) we find we need to set y explictly to zero would need to 
//		be the value returned by ImmediateCoding(ALU_Y_IMMEDIATE)
// +++ note that VM3_COPY_RVALUE and VM3_COPY_VBLE assume zero is the right 
//		imm value for both ALU and PTRS
#define ALU_IMM_MAX	(1 << 24) // n where -n to n-1 may be imm-codable for y
#define ALU_IMM_MIN	(1 << 20) // n where -n to n-1 are all imm-codable for y
#define ALU_IMM_SHIFT		8 // y value aligned at d8 when ALU_Y_IMMEDIATE
// flags in d7-4
#define ALU_Y_IMMEDIATE	 0xC0 // value in d32-8
#define ALU_Y_IS_COND	 0x80 // from condition flags (see below)
#define ALU_Y_UNSIGNED	 0x40
#define ALU_X_IGNORE	 0x20 // no output other than "condition" flags,
#define ALU_Z_UNSIGNED	 0x10
#define ALU_MULTIPLY		8 // i.e. z operand is (z * j)
// selection of the operation
#define ALU_OP_AND_NOT		7
#define ALU_OP_AND			6
#define ALU_OP_OR_NOT		5
#define ALU_OP_OR			4
#define ALU_OP_XOR_NOT		3
#define ALU_OP_XOR			2
#define ALU_OP_SUB			1
#define ALU_OP_ADD			0
#define ALU_OP_NEGATE		1 // bit that effectively inverts z
#define ALU_OP_COPY			0 // x:=y+z: see above
#define ALU_OPI_COPY_Z		0 // "immediate" coding for ALU_OP_COPY
#define ALU_OPI_COND  (0x282LL << 24) // "immediate" coding for ALU_X_IGNORE
#define ALU_OPI_SUB	IMM_CODING_ONE // "immediate" coding for (y - z)
//#define ALU_OPI_CMP	  (0x21102LL << 16) // SUB with ALU_X_IGNORE

// conditional branches consist of VM3_BR_COND with d3-2 holding one of the 
//		COND_xx codes, which are also the values to go in d9-8 of the 
//		immediate data for the ALU operation, which requires shifting them 
//		up ALU_IMM_SHIFT places
// for the VM opcode, the COND_ codes are treated as DEST_ codes (see 
//		VM_OP_DEST_SHIFT above)
// the condition is inverted by XORing with COND_FLAG_INV; <Context::Compare> 
//		assumes it is set for conditions satisfied by the value zero
#define COND_LT			0x00	// opd0 < 0
#define COND_NE			0x01	// opd0 != 0; opd0 != opd1
#define COND_GE			0x02	// opd0 >= 0
#define COND_EQ			0x03	// opd0 = 0; opd0 = opd1
#define COND_FLAG_EQ	0x01	// clear for an inequality
#define COND_FLAG_INV	0x02	// set for EQ and GE, clear for NE and LT

// all op codes of the form [reg] := [reg] + n add a value from the stack 
//		(which the VM checks is short form) instead of n if the immediate 
//		coding word is -ve

// register numbers for class 1 instructions: [+] only
// r1 is reg((n >> VM3_REG_NR_SHIFT) & 31), r2 is reg(n & 31)
// set d10 to trap if value being copied is an address in the operand stack
#define VM3_REG_TRAP	0x400
#define VM3_REG_NR_SHIFT  5
#define VM3_REG_NR_MASK  31 // register number is 5 bits
#define IMM_PUSH_P_T  (0x21663LL << 16)	// r1 is SP, r2 is P, trap
#define IMM_PUSH_Q_T  (0x21862LL << 16)	// r1 is SP, r2 is Q, trap
#define IMM_PUSH_Q	  (0x21822LL << 16)	// r1 is SP, r2 is Q
#define IMM_PUSH_W	  (0x21722LL << 16)	// r1 is SP, r2 is W
#define IMM_PUSH_LP_T (0x21362LL << 16)	// r1 is SP, r2 is LP, trap
#define IMM_PUSH_TP_T (0x21263LL << 16)	// r1 is SP, r2 is TP, trap
//#define IMM_Q_TO_P  (0x2182CLL << 16)	// r1 is P, r2 is Q
#define IMM_POP_P_T	  (0x29C06LL << 16)	// r1 is P, r2 (ignored) is NIL, trap
#define IMM_POP_LP_T  (0x29604LL << 16)	// r1 is LP, r2 (ignored) is NIL, trap
//#define IMM_P_TO_Q  (0x21611LL << 16)	// r1 is Q, r2 is P
#define IMM_SP_TO_Q	  (0x21111LL << 16)	// r1 is Q, r2 is SP
//#define IMM_POP_Q	  (0x21010LL << 16)	// r1 is Q, r2 (ignored) is NIL
//#define IMM_Q_TO_XMB (0x2183ALL << 16)	// r1 is XMB, r2 is Q
//#define IMM_Q_TO_YMB (0x21828LL << 16)	// r1 is YMB, r2 is Q
//#define IMM_Q_TO_ZMB (0x21838LL << 16)	// r1 is ZMB, r2 is Q

// other opns that might be needed
//
//#define VM3_SET_Q		0x44 // - PRGL	Q := reg + n (for DEST_REGS)
//#define VM3_SET_W		0x44 // + PRGL	W := reg + n (for DEST_REGS)
//#define VM3_SET_P		0x44 // +- PRGL	P := reg + n
//#define VM3_P_IP		0xBE // +-		P := IP + n
//#define VM3_POP_PTRS	0xB1 // s	delete ptrs component from operand stack
//#define VM3_INC_STK_P	0xC5 // s	inc stk count for ptr in reg P if to heap


// extracode numbers: index into <sys_rt_name> and <sys_rt_number>
// +++ may have to change the way the tables for switch instructions are built
// +++ _BITS values preceded by _PTRS and followed by _BOTH; same is assumed 
//		to be true for VALUE_HAS_
// see declarations of the routines for more detail
#define VM3X_MIN		 0 // assumed in <Makefile::PutFragment>
#define VM3X_MAX		 1 // assumed OP_MAX - OP_MIN in <PutFragment>
#define VM3X_DIV		 2 // assumed OP_DIV - OP_MIN in <PutFragment>
#define VM3X_REM		 3 // assumed OP_REM - OP_MIN in <PutFragment>
#define VM3X_ABS		 4 // assumed OP_ABS - OP_MIN in <PutFragment>
#define VM3X_LOOKUP_SW	 5 // void(Int table) SysExLookupSwitch
#define VM3X_NEW_REC_CLR 6 // ref void(Int ptr_len, Int bit_len) SysExNewRecClear
#define VM3X_FREE_REC	 7 // frees fragment processed by VM3_FREE_RECORD
					//	 8 // ident(-> ptr, -> ss, p_len) SysExGetPutPtrs
#define VM3X_GETPUT_BITS 9 // ident(-> ptr, -> ss, b_len) SysExGetPutBits
					//	10 // ident(-> ptr, -> ss, p_len, b_len) SysExGetPutBoth
#define VM3X_SUB_INDEX	11 // Int(s_spec p,q) SysExSubtractIndexes
					//	12 // index(p_len, index) SysExIndexPlusPtrs
#define VM3X_X_PL_BITS	13 // index(b_len, index) SysExIndexPlusBits
					//	14 // index(p_len, b_len, index) SysExIndexPlusBoth
					//	15 // (-> ptr, -> ss, p_len) SysExMoveIndexPtrs
#define VM3X_MOV_X_BITS	16 // (-> ptr, -> ss, b_len) SysExMoveIndexBits
					//	17 // (-> ptr, -> ss, p_len, b_len) SysExMoveIndexBoth
					//	18 // index(Int ptr_len) SysExNewStringPtrs
#define VM3X_NEW_S_BITS 19 // index(Int bit_len) SysExNewStringBits
					//	20 // index(Int ptr_len, Int bit_len) SysExNewStringBoth
					//	21 // bool(§x, §y) SysExLPtrsEq [x,y ptrs only]
#define VM3X_L_BITS_EQ	22 // bool(§x, §y) SysExLBitsEq [x,y pure bitstrings]
					//	23 // bool(§x, §y) SysExLBothEq [x,y both cpts]
					//	24 // bool(§x) SysExLPtrsNil [x ptrs only]
#define VM3X_L_BITS_NIL	25 // bool(§x) SysExLBitsNil [x pure bitstring]
					//	26 // bool(§x) SysExLBothNil [x both cpts]

#define VM3_EXTRACODES	27 // size of table of extracodes

#define VM3_TRAPS	4 // size of table of "trap" routines
*/

// register numbers as unsigned
#define VM3REG_NIL	 0
#define VM3REG_RP	 1
#define VM3REG_GP	 2
#define VM3REG_LP	 3
#define VM3REG_IP	 4 // pivot of statics (<CodeFragment::Area> assumes +ve)
#define VM3REG_NP	 5
#define VM3REG_HP	 6
#define VM3REG_W	 7 // working-space; -> fieldspec or stringspec
#define VM3REG_Q	 8 // working-space; -> pointer in ident or index

#define VM3REG_YLB	16
#define VM3REG_SP	17 // operand stack pointer
#define VM3REG_TP	18 // head of list heap fragments to be freed
#define VM3REG_QP	19
#define VM3REG_YFB	20

#define VM3REG_P	22 // register P
#define VM3REG_FP	23 // frame stack pointer
#define VM3REG_ZLB	24
#define VM3REG_XLB	25
#define VM3REG_ZLP	26
#define VM3REG_XLP	27
#define VM3REG_ZFB	28
#define VM3REG_XFB	29
#define VM3REG_ZFP	30
#define VM3REG_XFP	31

// register numbers with 1 in ms bit as signed
#define VM3REGS_YLB -16
#define VM3REGS_SP  -15 // operand stack pointer
#define VM3REGS_TP  -14 // head of list heap fragments to be freed
#define VM3REGS_QP	-13
#define VM3REGS_YFB -12

#define VM3REGS_P   -10 // register p
#define VM3REGS_FP	 -9 // frame stack pointer
#define VM3REGS_ZLB	 -8
#define VM3REGS_XLB	 -7
#define VM3REGS_ZLP	 -6
#define VM3REGS_XLP	 -5
#define VM3REGS_ZFB	 -4
#define VM3REGS_XFB	 -3
#define VM3REGS_ZFP	 -2
#define VM3REGS_XFP	 -1

#define REG_NAMES "|NIL|RP|GP|LP|IP|NP|HP|W|Q|r9|r10|r11|r12|r13|r14|r15|\
YLB|SP|TP|QP|YFB|r21|P|FP|ZLB|XLB|ZLP|XLP|ZFB|XFB|ZFP|XFP|"

// NB specification of <CodeFragment::Area> assumes PTR_NIL zero, others +ve
#define PTR_NIL			VM3REG_NIL
#define PTR_RECORD		VM3REG_RP	// pointer to the current record
#define PTR_GLOBAL		VM3REG_GP	// the global stack frame
#define PTR_MAX_GLOBAL	VM3REG_GP	// largest PTR_ value with global scope
#define PTR_LOCAL		VM3REG_LP	// current frame (local variables etc)

#define PTR_FLAG_FRAME	 2	// bit that is set in LP and GP, clear in RP


// I/O addresses which use 32-bit values that are not necessarily short form: in each 
//		case bit n applies to I/O address n
// VHDL code says: -- +++ for VM3.2 we probably don't need that restriction.

#define VM3_OUT_32_MASK_1	0x400  // ) outputs with d10 and one of d9d8 set
#define VM3_OUT_32_MASK_2	0x300  // )
#define VM3_IN_32_MASK		0x0080 // inputs: just 7 (flash data)
