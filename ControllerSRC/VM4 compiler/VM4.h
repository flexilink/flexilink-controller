// VM4.h : header file for VM4
//
// Copyright (c) 2019-2025 Nine Tiles

#pragma once


// definitions for VM4, to be used by both compiler and interpreter; must 
//		be kept compatible with the VHDL code
// +++ currently they are a subset of the VM3.2 op codes so the interpreter 
//		doesn't need to distinguish between the two versions 

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
// +++ in various places we assume POINTER_SIZE = VM_WORD_LENGTH
#define POINTER_SIZE		 32	// bits in a pointer
#define ROUTINE_SIZE		 32	// bits in an entrypoint
#define ROUTINE_SIZE_WORDS	  1	// words in an entrypoint (rounded up)
#define IDENT_SIZE			160	// bits in an ident (5 words)
#define INDEX_SIZE			 64	// bits in an index
// max sizes are such that offsets are always codable as an immediate value
#define MAX_BITS_LENGTH	  (1 << 23)	// max bitstring length in a field = 1MB
#define MAX_BITS_OFFSET	  (1 << 27)	// max bitstring length in a record = 16MB
#define DEFAULT_STRING_LENGTH	120 // elements

// "immediate" codings for some of the above
#define IMM_WORD_LENGTH (0x282LL << 24) // word length (32, 0x20)
#define IMM_M_WORD_LEN  0x38EFFFFFFLL // minus word length (-32, 0xFFFFFFE0)

// addresses of the top bit of the operand stack and the bottom and top bits 
//		of the heap, including the "area" flags
// the dRAM contains, in increasing order of address: code; statics; frame 
//		stack; operand stack; heap; buffers for routing logic
// the areas occupied by the code and statics are sized after the program is 
//		compiled, so that they exactly fit; the code cache can only address 
//		the bottom 4MB, and the loader assumes code + statics fits in 14MB 
//		and 2MB is enough for its own stacks
// setting up of the heap and buffers is done by the VM code; the loader 
//		leaves some version number etc information in the first few words 
//		of the heap area
// +++ currently in writes to SP, LP, GP, and FP, we check the area and that 
//		the value is in the bottom 8KB; we also check that a new value for 
//		FP is between LP and SP, and one for SP is above FP; also should 
//		check that YL etc are within bounds when they point to the operand 
//		stack, and that any new value for LP is between GP and FP
// the frame stack values point to the start of the globbal frame's bit 
//		string, so 2 words below them will be used for its fragment header; 
//		we allow 32 words so that the address will be imm-codable
#define VM4_LOADER_ADDRESS		0x01FFC000	// entrypoint to loader code
#define VM4_FRAME_STACK_AREA	0x40000000	// address 0 in frame stack area
#define VM4_GLOBAL_STACK_AREA	0xC0000000	// address 0 in global stack area
#define VM4_GLOBAL_AREA_SIGNED -0x40000000	// address 0 in global stack area
#define VM4_LOADER_LP_ADDRESS	0x00380020	// allow 2MB for init loader
#define VM4_DUMP_FRAME_STACK	0x403FE020	// allow 32KB for crashdump code
#define VM4_HEAP_AREA			0x80000000	// address 0 in heap area
#define VM4_STATICS_AREA		0x20000000	// address 0 in statics area
#define VM4_STATICS_CACHE_WORDS	  1024	// words in statics area of cache
#define VM4_OPD_STACK_TOP	0x603FFFFF	// word below heap area
#define VM4_HEAP_BASE	0x80400000	// 4M words (128Mb) for stacks, code, etc
#define VM4_AREA_MASK		0xE0000000	// "area" field
#define VM4_ADDRESS_MASK	0x1FFFFFFF	// d28-26 not used in Aubergine
#define VM4_SCP_HEAP_BASE	0x88000000	// VM4_HEAP_BASE as a SCP bit address

// VM4 operations consist of a 8-bit op code and up to 4 further octets 
//		that code an immediate value n which is 32 bits + sign, though not 
//		all such values can be represented. The op code defines from where 
//		in the microcode ROM to read a sequence of instructions to be 
//		executed by the hardware.
// See the VM4 specification document for details of the instruction set.


// VM4 operations: op code values

// The ls 2 bits of the op code show what n (the "immediate" value) is; see 
//		the VM4 specification document for details.
// Here we regard the opcode as being made up of four 2-bit fields, each of 
//		which either can either have a "fixed" value or hold a parameter:
//
//	  d7-6: always fixed
//	  d5-4: base register PTR_ code: PTR_NIL = xP where x is the DEST_
//	  d3-2: destination or condition: DEST_ or COND_ code
//	  d1-0: immediate value: as required by the VM's logic (d1 = 0 if 
//				single-byte op code, 1 if "immediate" value follows)
//
// For VM3.2 d5-4 could also code the components (d5 = bitstring present, 
//		d4 = pointers present); the 00 ("void") code point was never used, 
//		so the only coding used for VM4 is 10 (VALUE_HAS_BITS) which we 
//		code as "fixed" for VM4.
//
// The VM4_ values #defined below are coded as:
//
//	d23-20: permissible values for "base register" parameter
//	d19-16: permissible values for "destination" parameter
//	d15-12: permissible values for "immediate value" parameter
//	   d11: "base register" field may be "fixed" (see below)
//	   d10: "destination" field may be fixed
//		d9: "immediate value" field may be fixed (with d1=0)
//		d8: instruction is a branch
//	  d7-0: opcode: any field containing a parameter is ignored
//
// "Permissible values" settings are 1 in bit n of the field if the 
//		parameter can take the value n, 0 else; parameter values are 0 to 
//		3, or -1 for "fixed" (see above). See <EmitOpn> for more detail.

//#define VM_OP_BITS_FIRST (1<<25) // ignored if d23 or d24 set
//#define VM_OP_PTRS_FIRST (1<<24) // ignored if d23 set
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

// op codes for VM4
// note that the 's' option for VM4_SET_xx (not currently used, I think) 
//		will be needed when <parameter> is implemented
//
// 01+-si are options for d1-0 (1 = -1; s = immediate data not used, 
//			d1 must be 0; i = immediate data required, currently is + 
//			but could redefine as - if required); EmitBranch requires +- 
//			though for _PRE_CALL_ the offset will always be +ve
// vyxz	  are options for d3-2 as dest reg (v = DEST_STACK)
// lnge	  are options for d3-2 as cond codes (respectively LT,NE,GE,EQ)
// PRGL   are options for d5-4 as base reg

// The '*' prefix indicates the contents of the word addressed by a register.
// 'n' is a word popped from the stack (trap if not short form) if 's' 
//		specified, immediate data else
#define VM4_NO_OPN		0x0000E00 // s		no operation
#define VM4_ERROR_TRAP	0x0000E01 // s		causes a VM error trap
#define VM4OP_ERROR			 0x01 //		8-bit op code for VM error trap
#define VM4_INIT_GFRAME	0x0004C02 // +		GP!-2 := n then as VM4_FPUSH_CLR
#define VM4_TRACE_GP	0x0000E04 // s		trace the globals
#define VM4_END_TRAP	0x0000E05 // s		return from "vanilla" extracodes 
#define VM4_FPUSH		0x0004C06 // +		FP +:= n (bits)
#define VM4_SET_X_NIL	0x0000E08 // s		set x to <nil>
#define VM4_SETLEN_T	0x00C4A09 // s+ xz	as _SETLEN with trap if substr
#define VM4_SET_Z_L_NIL 0x0000E0C // s		set z as if <nil> ident loaded
#define VM4_X_FROM_Z	0x0000E10 // s		copy z config to x (does not set XP)
#define VM4_END_TRAP_5	0x0000E11 // s		return from <SysExTrapInitZ>
#define VM4_BR			0x000CD12 // +-		PC +:= n
#define VM4_PRE_PLING	0x00E0A14 // s yxz	xP := *xF
#define VM4_POP_QUICK	0x00A0A15 // s yz	pop stack, set opd as the value popped
#define VM4_MAKE_ENTRY	0x000CD16 // +-		push PC + n (make entrypoint)
#define VM4_END_TRAP_2	0x0000E19 // s		return from <SysExTrapPopX>
#define VM4_SET_SP		0x0004C1A // +		SP := imm and other init'n
#define VM4_INIT_LP_FP	0x0004C1E // +		LP := imm; FP := imm
//#define VM4_CLEAR_MEM	0x0000E20 // s		SysExClearMemory (was <systemroutine 2>)
// code 21 "deref ident (ZP,ZF only)" not currently used
#define VM4_PRE_CALL_1	0x000CD22 // +-		if EQZ push one zero word and branch
#define VM4_SET_L		0x00E5A25 // 0s+ yxz  xL := xF + n
#define VM4_TRACE_VALUE	0x0000E30 // s		SysExTraceValue (<systemroutine 3>)
#define VM4_RESV_FR_STK	0x0000E31 // s		push n bits on frame stack (<systemroutine 11>)
#define VM4_PRE_TRACE_S	0x0004C32 // +		TL := XF map for single ptr (cf E2)
#define VM4_FPUSH_WORD	0x000CC36 // +-		*FP++ := n
#define VM4_SET_L_T		0x00C5A39 // 0s+ xz as _SET_L with trap if substr
#define VM4_SHIFT_X		0x000CE40 // s+-	shift x by n
#define VM4_POP_ID_TO_Z	0x0000E41 // s		set ZF -> ident popped from stack
#define VM4_VOID_INPUT	0x000DC44 // 0+-	+++ could use VM4_INPUT_TO_Z + VM4_RESET_ALU
#define VM4_DEREF_INDEX	0x00C5A49 // 0s+ xz	set P & F, F -> index, offset imm or on stk
#define VM4_PUSH_Z_ADDR	0x0000E60 // s		push ZF on opd stk and reset ALU
#define VM4_STORE_IDENT	0x0000E61 // s		x := &z (XL ignored), reset ALU
#define VM4_PUSH_CP_N	0x000CD62 // +-		push (CP + n) [for VM4X_LOOKUP_SW]
#define VM4_COPY_RVALUE	0x0000E64 // 0		pop opd stk to z & copy, chk x for substr
#define VM4_MARK_RECORD	0x0000E65 // s		set "marked" in (SP++)!-1
#define VM4_CONTROL		0x000CC66 // +-		<vm_control>, "y" (if present) from mem
//#define VM4_COPY_VBLE_U	0x0004C66 // +		x := z (unsigned), chk for substr
//#define VM4_COPY_VBLE	0x0000E68 // 0		x := z (signed), chk for substr;
#define VM4_RESV_ON_STK	0x0004E69 // s		push -n bits on opd stk (for sysrt 7)
#define VM4_NEW_FRAME_2	0x0000E6C // s		new frame with two words zeroed
#define VM4_FPUSH_CLR_1	0x0000E6D // s		*(FP++) := 0
#define VM4_NEW_FRAME_N	0x0004C6E // +		new frame with (n/32 l_lim 3) words zeroed
#define VM4_STATUS_TO_Z	0x000DC70 // 0+-	set z to <vm_status> result
#define VM4_TRACE_FRAG	0x0000E71 // s		SysTraceFragment (<systemroutine 1>)
#define VM4_ALU			0x000DC74 // 0+-	for imm value see ALU_xxx below
#define VM4_OFFSET		0x00C4A78 // s+ xz	xF +:= n incorporating extension
#define VM4_ALIGN_X		0x00C0A79 // s xz	shift x to left-align with z
#define VM4_ERASE		0x0000E80 // s		pop stack, discard value
#define VM4_POP_Z		0x0000E81 // s		as VM4_POP_QUICK but won't omit "set L"
#define VM4_CALL		0x000CD82 // +-		*(FP++) := PC; PC +:= n
#define VM4_ENTER		0x0000E85 // s		xchg *FP with PC
#define VM4_PUSH_REG	0x0004C86 // i		*--r1 := r2
#define VM4_SETLEN		0x00C4A88 // s+ xz	xL := xM + n unless > current xL
#define VM4_DEREF_ID_T	0x00C0A89 // s xz	set opd from ident to which its F points
#define VM4_PUSH_WORD	0x000FC90 // 01+-	*--SP := n
#define VM4_PUSH_I_MAP	0x000CD92 // +-		*--SP := n (as branch, for indirect ptrs map)
#define VM4_NEW_FRAME	0x0000E94 // s		*FP := LP; LP := FP; FP +:= 1
#define VM4_SET_Z		0x000EC95 // 1+-	set z to be n
#define VM4_ALIGN_Z		0x0000E98 // s		set z length equal to x length
#define VM4_POP_REG		0x0004C9A // i		r1 := *(SP++)
#define VM4_COPY_REG	0x0004C9E // i		r1 := r2
#define VM4_SET_J		0x000CEA0 // s+-	set j to be n
#define VM4_SWAP_STK_RP	0x0000EA1 // s		exchange *SP with RP
#define VM4_RETURN		0x0000EA5 // s		return from routine (including pop frame stack)
#define VM4_SET_F_CP	0x00EC9A6 // +- yxz	xF := CP + n
#define VM4_DEREF_IDENT	0x00C0AA8 // s xz	set opd from ident to which its F points
#define VM4_POP_IDENT	0x00C0AA9 // s xz	set opd from ident on stack
#define VM4_PUSH_IDENT	0x0000EB0 // s		push &z as an ident
#define VM4_RESET_ALU	0x0000EB1 // s		reset ALU
#define VM4_BR_COND		0x00FC9B2 // +- lnge	if [condition] then PC +:= n
#define VM4_POP_WORD	0x0000EB5 // s		ls word if long form (for "y" to VM4_CONTROL)
#define VM4_PTR_TO_ID	0x0000EB9 // s		x (ident) := z (pointer), length from fragment
#define VM4_INPUT_TO_Z	0x000DCC0 // 0+-	set z to <vm_in> result
#define VM4_SET_F		0x0FE52C5 // 0s+ yxz PRGL	xF := reg + n
#define VM4_SET_F_N		0x00EDAC5 // s+- yxz		xF := xP + n [1]
#define VM4_SET_ZP		0x0E006D0 // s RGL			ZP := reg
//#define VM4_PUSH_L_WORD 0x000CDD2 // +- 	push imm (1 word) as long form
#define VM4_X_ON_FSTK	0x0000EE1 // s		set x to be !FP
#define VM4_PRE_TRACE_D	0x000CCE2 // +-		TL := XL, set pointers map to imm (direct map)
#define VM4_PRE_TRACE_I	0x000CDE2 // +-		as above, indirect map
#define VM4_INIT_G_CTRL	0x000CCF2 // +-		set GP!-1 to imm
#define VM4_INIT_GCTL_I	0x000CDF2 // +-		set GP!-1 to imm (for indirect map)

// [1] opcodes other than '-' are the same as VM4_SET_F

// components of the n value that accompanies an ALU operation; defaults are 
//		(signed) operand in memory; only include one each of _Y_ and _OP_
// for single-operand cases, we use _OR_NOT for "not", _SUB for "neg", _ADD 
//		for "copy"; y is zero except in the "copy" case where y can be 
//		immediate and z zero
// to copy from z to x we use the all-zero version, in which y is selected 
//		as "signed, as programmed" but defaults to <nil>; _COPY_Z is the 
//		value to pass to EmitOpn() which in this case is zero but if (for 
//		instance) we find we need to set y explictly to zero would need to 
//		be the value returned by ImmediateCoding(ALU_Y_IMMEDIATE)
// +++ note that VM4_COPY_RVALUE assumes zero is the right imm value for 
//		copying from z to x
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
#define ALU_OPI_COPY_Z_U (0x244LL << 24) // as above when z unsigned
#define ALU_OPI_COND (0x282LL << 24) // "immediate" coding for ALU_X_IGNORE
#define ALU_OPI_SUB	  IMM_CODING_ONE // "immediate" coding for (y - z)
#define ALU_OPI_XOR	 (0x202LL << 24) // "immediate" coding for (y xor z)
#define ALU_OPI_CMP	 (0x21102LL << 16) // SUB with ALU_X_IGNORE
#define ALU_OPI_AND_C (0x21602LL << 16) // AND with ALU_X_IGNORE
#define ALU_OPI_CMP_EP (0x316AELL << 16) // AND (~6) with ALU_X_IGNORE

// conditional branches consist of VM4_BR_COND with d3-2 holding one of the 
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
// r1 is reg((n >> VM4_REG_NR_SHIFT) & 31), r2 is reg(n & 31)
// NOTE that, unlike in VM3, d10 does not do anything
#define VM4_REG_SET_D31	0x400
#define VM4_REG_NR_SHIFT  5
#define VM4_REG_NR_MASK  31 // register number is 5 bits
#define IMM_PUSH_W	  (0x21822LL << 16)	// r1 is SP, r2 is W
#define IMM_PUSH_LP	  (0x21322LL << 16)	// r1 is SP, r2 is LP
#define IMM_POP_LP	  (0x28600LL << 16)	// r1 is LP, r2 (ignored) is NIL
#define IMM_SP_TO_W   (0x21111LL << 16)	// r1 is W, r2 is SP
#define IMM_SAVE_SP	  (0x2111BLL << 16)	// r1 is r13, r2 is SP
#define IMM_SAVE_FP	  (0x2171DLL << 16)	// r1 is r14, r2 is FP
#define IMM_SAVE_LP	  (0x2131ELL << 16)	// r1 is r15, r2 is LP
#define IMM_POP_GP	  (0x28400LL << 16)	// r1 is GP, r2 (ignored) is NIL
#define IMM_CP_TO_ZP  (0x21434LL << 16)	// r1 is ZP, r2 is CP
#define IMM_CP_TO_YP  (0x21424LL << 16)	// r1 is YP, r2 is CP
#define IMM_NIL_TO_XF (0x2103ALL << 16)	// r1 is XF, r2 is NIL

//#define IMM_PUSH_P_T  (0x21663LL << 16)	// r1 is SP, r2 is P, trap
//#define IMM_PUSH_Q_T  (0x21862LL << 16)	// r1 is SP, r2 is Q, trap
//#define IMM_PUSH_W	  (0x21722LL << 16)	// r1 is SP, r2 is W
//#define IMM_PUSH_TP_T (0x21263LL << 16)	// r1 is SP, r2 is TP, trap
//#define IMM_Q_TO_P  (0x2182CLL << 16)	// r1 is P, r2 is Q
//#define IMM_POP_P_T	  (0x29C06LL << 16)	// r1 is P, r2 (ignored) is NIL, trap
//#define IMM_P_TO_Q  (0x21611LL << 16)	// r1 is Q, r2 is P
//#define IMM_POP_Q	  (0x21010LL << 16)	// r1 is Q, r2 (ignored) is NIL
//#define IMM_Q_TO_XMB (0x2183ALL << 16)	// r1 is XMB, r2 is Q
//#define IMM_Q_TO_YMB (0x21828LL << 16)	// r1 is YMB, r2 is Q
//#define IMM_Q_TO_ZMB (0x21838LL << 16)	// r1 is ZMB, r2 is Q

// extracode numbers: index into <sys_rt_name> and <sys_rt_number>
// +++ may have to change the way the tables for switch instructions are built
// see declarations of the routines for more detail
#define VM4X_MIN		 0 // assumed in <Makefile::PutFragment>
#define VM4X_MAX		 1 // assumed OP_MAX - OP_MIN in <PutFragment>
#define VM4X_DIV		 2 // assumed OP_DIV - OP_MIN in <PutFragment>
#define VM4X_REM		 3 // assumed OP_REM - OP_MIN in <PutFragment>
#define VM4X_ABS		 4 // assumed OP_ABS - OP_MIN in <PutFragment>
#define VM4X_LOOKUP_SW	 5 // void(Int table) SysExLookupSwitch
#define VM4X_NEW_REC_CLR 6 // ref void(Int bit_len, Int ctl_word) SysExNewRecClear
#define VM4X_GET_PUT	 7 // ident(-> index, b_len) SysExGetPut
#define VM4X_SUB_INDEX	 8 // Int(index p,q) SysExSubtractIndexes [p-q]
#define VM4X_INDEX_PLUS	 9 // index(index, b_len) SysExIndexPlus
#define VM4X_MOV_INDEX	10 // (-> index, b_len) SysExMoveIndex
#define VM4X_NEW_STRING	11 // index(Int bit_len, Int ctl_word) SysExNewString
#define VM4X_ADD_DEC	12 // (int 64, SysTextFormat) SysExAddDecimal
#define VM4X_ADD_HEX	13 // (uint 64, SysTextFormat) SysExAddHex
#define VM4X_ADD_BITS	14 // (§ array of bool, SysTextFormat) SysExAddBits
#define VM4X_ADD_TIME	15 // (int 64, SysTextFormat) SysExAddTime
#define VM4X_ADD_TEXT	16 // (§ bytes) SysExAddText
#define VM4X_OUTPUTLINE	17 // () SysExOutputLine

#define VM4_EXTRACODES	18 // size of table of extracodes

// specification of SysTextFormat as int64_t
// shift between each field and ls end of value
#define VM4TF_L_PAD_ALT		37 // s if hex, z else
#define VM4TF_R_PAD_Z		36
#define VM4TF_R_PAD_SPACES	35
#define VM4TF_PTP			34
#define VM4TF_DAYS			33
#define VM4TF_HOURS			32
#define VM4TF_LEFT_PAD		24
#define VM4TF_FRACTION		16
#define VM4TF_PLACES		 8
#define VM4TF_WORD			 0

// +++ currently loader code doesn't use any extracodes
#define VM4_LOADER_XC	 6 // loader code only uses extracodes 0 to 5

#define VM4_TRAPS	4 // size of table of "trap" routines


// register numbers as unsigned
#define VM4REG_NIL	 0
#define VM4REG_RP	 1
#define VM4REG_GP	 2
#define VM4REG_LP	 3
#define VM4REG_CP	 4 // pivot of constants

#define VM4REG_W	 8
#define VM4REG_TL	 9

#define VM4_SAVE_SP 13
#define VM4_SAVE_FP 14
#define VM4_SAVE_LP 15
#define VM4REG_YL	16
#define VM4REG_SP	17 // operand stack pointer
#define VM4REG_YP	18 // pointer to start of bit string containing y operand

#define VM4REG_YF	20

#define VM4REG_FP	23 // frame stack pointer
#define VM4REG_ZL	24
#define VM4REG_XL	25
#define VM4REG_ZP	26 // pointer to start of bit string containing z operand
#define VM4REG_XP	27 // pointer to start of bit string containing x operand
#define VM4REG_ZF	28
#define VM4REG_XF	29

// register numbers with 1 in ms bit as signed
#define VM4REGS_YL -16
#define VM4REGS_SP -15 // operand stack pointer

#define VM4REGS_YF -12

#define VM4REGS_FP	-9 // frame stack pointer
#define VM4REGS_ZL	-8
#define VM4REGS_XL	-7
#define VM3REGS_ZP	-6
#define VM3REGS_XP	-5
#define VM4REGS_ZF	-4
#define VM4REGS_XF	-3

#define REG_NAMES "|NIL|RP|GP|LP|CP|r5|r6|r7|W|TL|r10|r11|r12|SAVE_SP|\
SAVE_FP|SAVE_LP|YL|SP|YP|r19|YF|r21|r22|FP|ZL|XL|ZP|XP|ZF|XF|r30|r31|"

// NB specification of <CodeFragment::Area> assumes PTR_NIL zero, others +ve
#define PTR_NIL			VM4REG_NIL
#define PTR_RECORD		VM4REG_RP	// pointer to record being created
#define PTR_GLOBAL		VM4REG_GP	// the global stack frame
#define PTR_MAX_GLOBAL	VM4REG_GP	// largest PTR_ value with global scope
#define PTR_LOCAL		VM4REG_LP	// current frame (local variables etc)

#define PTR_FLAG_FRAME	 2	// bit that is set in LP and GP, clear in RP


// I/O addresses which use 32-bit values that are not necessarily short form: 
//		in each case bit n applies to I/O address n
// +++ don't ever seem to be used

//#define VM4_OUT_32_MASK_1	0x400  // ) outputs with d10 and one of d9d8 set
//#define VM4_OUT_32_MASK_2	0x300  // )
//#define VM4_IN_32_MASK		0x0080 // inputs: just 7 (flash data)
