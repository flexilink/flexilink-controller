// AnalyserDoc.h
// Copyright 2011-2017 Nine Tiles. All rights reserved.

// This is the "analyser" program which collects information from a target 
//		board via SCP. It was developed from the Configure program, which 
//		was in turn developed from the VM3.0 interpreter. This version is 
//		used via the Controller's management window.
// Upoading of code is out of scope; during development it is done by the 
//		Compiler, and field upgrades are done via the MIB.

// Supported currently (all via SCP):
//
//		- downloading and displaying debug dumps
//		- reading VM registers and memory
//		- dumping the state when the VM has crashed

// Intended for future versions
//
//		- interpreting VM crashdumps (unless that's done via the MIB)

/////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <stdint.h>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <deque>
#include "../common/string_extras.h"
#include "ControllerDoc.h"

#ifndef ASSERT_VALID
#ifdef _DEBUG
#define ASSERT_VALID(p) p->AssertValid()
#else
#define ASSERT_VALID(p) {}
#endif
#endif


// entry in <AnalyserDoc::frame> (for VM3)
struct VariableInfo {
	int ptrs_offset;	// bit offset backwards from pivot
	int ptrs_length;	// (number of bits) - 32
	int bits_offset;	// bit offset from pivot
	int bits_length;	// (number of bits) - 1
	std::string name;
	char cpts;
	int8_t llfmt;	// now 0 for normal vbles, 1 if by reference
};

// <ptrs_length>, which defines where the return address should be, is 
//		included explicitly in the file, whereas <bits_length> is inferred 
//		from the variables
struct FrameInfo {
	uint32_t addr;		// byte address of entrypoint to routine that owns it
	std::string name;	// of routine that owns it
	int ptrs_length;	// number of bits (always a multiple of 32)
	int bits_length;	// number of bits
	std::vector<VariableInfo> v;	// entry for each variable
};

// entry in <AnalyserDoc::vm4frame> (for VM4)
// <fmt> indicates the type as: N = int, U = uint, E = entrypoint, I = ident, 
//			S = struct, P = pointer, A = array, X = index
struct Vm4VariableInfo {
	int offset;	// bit offset from start of bit string
	int length;	// number of bits (excluding padding at the end)
	std::string name;
	bool ref;	// whether by reference
	char cpts;	// P = incl Pointer, E = incl Entrypoint, B = pure Bitstring
	char fmt;	// type (see above)
};

struct Vm4FrameInfo {
	uint32_t addr;		// byte address of entrypoint to routine that owns it
	std::string name;	// of routine that owns it
	int length;			// number of bits in the bit string
	std::vector<Vm4VariableInfo> v;	// entry for each variable
};

// see where used in <InputChar>; it complains if you try to use a locally-
//		declared type in a template
struct UseCounts {
	int stack;
	int heap;
};
struct FreeListPointers {
	uint32_t next;
	uint32_t previous;
};

// document class for the Analyser

class AnalyserDoc : public FlexilinkSocket
{
public:
//	AnalyserDoc();
	AnalyserDoc(MgtSocket * u, uint8_t p);
	virtual ~AnalyserDoc();
#ifdef _DEBUG
//	virtual void AssertValid() const;
//	virtual void Dump(CDumpContext& dc) const;
#endif
		// identification of the SCP server
		// <partner> is the unit to which the SCP engine is connected
		// <partner_port> is the block id for the port on the partner unit
	class MgtSocket * partner;	// assumed not to be NULL
	uint8_t partner_port;		// block id (rubbish if <partner> is NULL)
		// the unit containing the SCP server (NULL if unknown)
	MgtSocket * Host() { return partner->LinkPartner(partner_port); }

		// current debug state (latest value from area 00001, zero if none 
		//		received; NB 00 is not a likely value for d25-24) coded as 
		//		follows, not valid if state is less than _CONN_MADE
		//
		//			d31: 1 = data captured in buffer 0
		//			d30: 1 = data captured in buffer 1
		//		 d29-28: reserved for "captured" flag for two more buffers
		//			d27: reserved
		//			d26: 0 = ad hoc debug, 1 = debug buffers monitoring VM
		//			d25: 0 = waiting for VM code to be loaded, 1 = VM code 
		//						has started
		//			d24: 0 = VM running, 1 = VM stopped
		//		  d23-0: current setting of controls, coded as when writing
		//					(set d22 to get message when d31-28 change)
	uint32_t server_state;
	std::string ServerState();	// <server_state> as text; empty string if not known

		// over-rides etc; NB for <PollAwaitingAck> we only need the base class version
		// send FindRoute request
	void SendConnReq();
		// called when the connection has been made
	void ConnectionMade(uint8_t * b);
		// send a SCP request to read <len> bytes from address <addr>
	void SendReadRequest(int len, uint32_t addr);

		// process an incoming message
	void ReceiveData(uint8_t * b, int len);

		// whether to display the messages
	bool display_messages;

		// strings for debug dump, loaded from a DBF file (DeBugFormat, .9t3dbf 
		//		or .9t4dbf depending on <theApp.vm4scp>) with name vm if d26=1, 
		//		logic else
		// currently we assume there are two capture buffers which between them 
		//		hold 512 lines x 512 bits, and that there will only be one of 
		//		each kind on the network (being the one that is under develop-
		//		ment); future versions might make more information available 
		//		through the SCP interface
	int dbg_st;	// -ve if nothing attempted, 0 if logic, 1<<26 if vm
	std::vector<std::string> dbg_caption;
	std::string dbg_format;	// set to dunmp in hex if couldn't read file

//	const char * UploadWords(uint32_t addr, int len, uint32_t * p, uint8_t * buf);
//	const char * UploadBuffer(uint32_t addr, int len, uint8_t * buf);
//	std::string DownloadBuffer(uint8_t area, uint32_t addr, int len, uint8_t * buf);

		// input from the user is collected in <pending_console_input>; when "enter" 
		//		is typed the command is actioned and <req_issued> is set; hitting 
		//		"enter" again actions it again; both are cleared out when the reply 
		//		is completed and when anything other than "enter" is typed
	std::string pending_console_input;
	bool req_issued; // must be false if <pending_console_input> is empty
	void AckConsole() { if (req_issued) { pending_console_input.clear(); 
														req_issued = false; } }
	void InputChar(char ch);
	uint32_t vm_reg[33];	// VM registers as most recently reported
	uint32_t last_r_addr;	// address last listed for an 'r' command

		// indication of the context when data from the cache or VM registers 
		//		is received; bitwise encoded, default is to list the data in hex
		// +++ apart from _PRE_M, this might have been done better using the 
		//		command letter, which should be in <pending_console_input[0]>
	uint32_t context;
#define SCP_CONTEXT_PRE_M	1 // fetching heap area base or registers for l, g, or m 
#define SCP_CONTEXT_L		2 // l or m command
#define SCP_CONTEXT_G		4 // g or m command
#define SCP_CONTEXT_M		6 // m command; mask for l, g, and m
#define SCP_CONTEXT_ALL_M	7 // mask for l, g, m, and _PRE_M
	uint32_t lp; // address of stack frame to be listed next (<nil> if none)
	uint32_t gp; // address of global stack frame
		// +++ ought to get the Compiler to set LP = GP before entering 
		//		<SysEntryInit>, then <gp> wouldn't be needed because the globals 
		//		would be chained onto the end of the locals
	int owner;		// index into <frame> (rubbish if <lp> is <nil>)
	int pc_offset;	// byte offset to return address from base of <frame[owner]>
	bool Vm4SetOwner(uint32_t pc);	// set <owner>; returns whether successful
	bool SetOwner(uint32_t pc);	// set <owner>; returns whether successful

	std::vector<std::string> sym;	// contents of symbols file
	int DecodeFrame(FrameInfo& fi, int i);	// read VM3 frame data from file
	bool DecodeVm4Frame(int& i); // read VM4 frame data from file
		// cache for data from the SCP server; see code for 'm' etc commands
		// <frame_addr> nonzero only guarantees that the lower half of 
		//		<frame_data> is valid; the high half is effectively local 
		//		storage for <ReceiveData> but is here so it's contiguous 
		//		with the low half
	uint32_t frame_data[640];	// copy of the target's memory
	uint32_t frame_addr;		// address for <frame_data[0]>; 0 if none
//	void GetFrame(uint32_t lp);	// request the data necessary to list frame, lp -> pivot
		// table that holds the globals followed by the locals, in the order 
		//		in which they appear in <sym>, followed by a dummy entry with 
		//		entrypoint INT_MAX; empty if symbols could not be read
	std::vector<FrameInfo> frame; // used if VM3
	std::vector<Vm4FrameInfo> vm4frame; // used if VM4

		// return the name of register <n>
	std::string RegName(int n);

		// cache of contents of the target's memory, for the "check heap" command
		// this is intended for use by code which scans the memory from the top down; 
		//		therefore, in the case of a miss it reads a block of memory in which 
		//		the required word is at the top
		// <heap_cache_base> can be in any area (not necessarily in the heap); if 
		//		there is nothing in the cache, it is zero (i.e. <nil>) and the cache 
		//		contains rubbish (NB not all-zero)
//	uint32_t heap_cache[320];	// copy of the target's memory
//	uint32_t heap_cache_base;	// address for first word in cache
//	std::string GetWord(uint32_t a, uint32_t& d);
};



