// AnalyserDoc.cpp: management terminal for Simple Control Protocol
//
//  Copyright 2011-2017 Nine Tiles
//
//////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "stdafx.h"
#include "Controller.h"
#include "AnalyserDoc.h"
#include "CrosspointDoc.h"
#include <sys/types.h>
// following two are for the VM3REG_ and VM4REG_ definitions; PTR_ 
//		definitions appear in both, with the same values; REG_NAMES 
//		appears in both with different definitions but is not used in 
//		this program (it's used by the compiler to generate microcode)
#include "VM32.h"
#include "..\VM4 compiler\VM4.h"

/////////////////////////////////////////////////////////////////////////////////////////

// Implementation of class AnalyserDoc

// constructor: <u> must not be NULL
AnalyserDoc::AnalyserDoc(MgtSocket * u, uint8_t p)
{
	partner = u;
	partner_port = p;
	server_state = NULL;
	display_messages = false;
	dbg_st = -1;
	req_issued = false;
	last_r_addr = 0x87FFFF00 - 4096;
	context = NULL;
	lp = NULL;
	frame_addr = NULL;
}

/*AnalyserDoc::AnalyserDoc()
{
	partner = NULL;
	host = NULL;
	server_state = NULL;
	display_messages = false;
	dbg_st = -1;
	req_issued = false;
	last_r_addr = 0x87FFFF00 - 4096;
	context = NULL;
	lp = NULL;
	frame_addr = NULL;
}
*/
// destructor
AnalyserDoc::~AnalyserDoc()
{
	if (call_ref >= 0 && (call_ref & 256)) theApp.scp.at(call_ref & 255) = NULL;
}


// <server_state> as text; empty string if not known
std::string AnalyserDoc::ServerState()
{
	std::string s;
	if (state < MGT_ST_CONN_MADE || server_state == 0) return s;

	s = (server_state & 0x04000000) ? "VM" : "Logic";
	s += " debug controls ";
	s += (server_state & 0x00200000) ? 'S' : 'C';
	s += (server_state & 0x00100000) ? 'S' : 'C';
	s += ToHex(server_state & 0xFFFFF, 5);
	
	switch (server_state & 0x03000000) {
case 0x01000000: s += "; VM waiting"; break;
case 0x02000000: s += "; VM running"; break;
case 0x03000000: s += "; VM stopped";
	}

	if ((server_state & 0xC0800000) == 0x00800000) s += "; capture enabled";
	else if ((server_state & 0xC0000000) == 0xC0000000) s += "; data captured";
	else if ((server_state & 0xC0000000) == 0x80000000) s += "; buffer A data captured";
	else if ((server_state & 0xC0000000) == 0x40000000) s += "; buffer B data captured";

	return s;
}


// Send the FindRoute request message; intended to be called on 
//		initialisation and also whenever the link is reconnected
// Assumes <partner> is non-NULL and <partner_port> is valid, so always uses 
//		data type 1.3.6.1.4.1.9940.2.4.0.144.168 and explicit address
// Also assumes SCP calls can't be requested unless at maintenance level
void AnalyserDoc::SendConnReq()
{
	ASSERT(theApp.privilege == 4); // assuming maintenance level
	server_state = 0;	// in case left over from a previous session
	call_ref += 0x10000;	// new call reference
	call_ref &= 0x7FFFFFFF; // in case has wrapped

	mgt_msg.count = 0;
	mgt_msg.m.resize(65);
	mgt_msg.m[0] = 8;		// FindRoute request
	mgt_msg.m[1] = 13;

	ByteString::size_type i = 0;
	do {
		mgt_msg.m[i+2] = theApp.link_socket->our_ident[i];	// owner
		i += 1;
	} while (i < 8);

	mgt_msg.m[10] = (uint8_t)(call_ref >> 24);
	mgt_msg.m[11] = (uint8_t)(call_ref >> 16);
	mgt_msg.m[12] = (uint8_t)(call_ref >> 8);
	mgt_msg.m[13] = (uint8_t)call_ref;
	mgt_msg.m[14] = 2;		// route ref
		// FlowDescriptor IE
	mgt_msg.m[15] = 4;
	mgt_msg.m[16] = 0;
	mgt_msg.m[17] = 4;
	mgt_msg.m[18] = 2;
	mgt_msg.m[19] = 0;
	mgt_msg.m[20] = 0;
	mgt_msg.m[21] = 1;
		// AsyncAlloc IE
	int rcv_label = AddHec(call_ref & 0x1FFF);
	mgt_msg.m[22] = 20;
	mgt_msg.m[23] = 0;
	mgt_msg.m[24] = 2;
	mgt_msg.m[25] = (uint8_t)(rcv_label >> 8);
	mgt_msg.m[26] = (uint8_t)rcv_label;
		// DataType IE: 1.3.6.1.4.1.9940.2.4.0.144.168.0
	mgt_msg.m[27] = 5;
	mgt_msg.m[28] = 0;
	mgt_msg.m[29] = 15;
	mgt_msg.m[30] = 43;
	mgt_msg.m[31] = 6;
	mgt_msg.m[32] = 1;
	mgt_msg.m[33] = 4;
	mgt_msg.m[34] = 1;
	mgt_msg.m[35] = 0xCD;
	mgt_msg.m[36] = 0x54;
	mgt_msg.m[37] = 2;
	mgt_msg.m[38] = 4;
	mgt_msg.m[39] = 0;
	mgt_msg.m[40] = 0x81;
	mgt_msg.m[41] = 0x10;
	mgt_msg.m[42] = 0x81;
	mgt_msg.m[43] = 0x28;
	mgt_msg.m[44] = 0;
		// CalledAddress IE
	mgt_msg.m[45] = 3;
	mgt_msg.m[46] = 0;
	mgt_msg.m[47] = 13;
	mgt_msg.m[48] = 0;		// "locator" prefix
	mgt_msg.m[49] = 9;
	mgt_msg.m[50] = 5;
	mgt_msg.m[51] = (uint8_t)(partner->unit_id >> 56);
	mgt_msg.m[52] = (uint8_t)(partner->unit_id >> 48);
	mgt_msg.m[53] = (uint8_t)(partner->unit_id >> 40);
	mgt_msg.m[54] = (uint8_t)(partner->unit_id >> 32);
	mgt_msg.m[55] = (uint8_t)(partner->unit_id >> 24);
	mgt_msg.m[56] = (uint8_t)(partner->unit_id >> 16);
	mgt_msg.m[57] = (uint8_t)(partner->unit_id >> 8);
	mgt_msg.m[58] = (uint8_t)partner->unit_id;
	mgt_msg.m[59] = 9;		// " block id" prefix
	mgt_msg.m[60] = partner_port;
		// PrivilegeLevel IE
	mgt_msg.m[61] = 12;
	mgt_msg.m[62] = 0;
	mgt_msg.m[63] = 1;
	mgt_msg.m[64] = 4;

	if (theApp.link_socket->TxMessage(mgt_msg.m, 0)) {
		SaveMessage(mgt_msg.m.data(), mgt_msg.m.size(), 'T');
		state = MGT_ST_CONN_REQ;
		return;
	}

	SaveMessage(mgt_msg.m.data(), mgt_msg.m.size(),'E');
	state = MGT_ST_FAILED;
}


void AnalyserDoc::ConnectionMade(uint8_t * b)
{
		// we already know it's a SCP debug server
		// request the debug buffer etc status
	SendReadRequest(4, 0x08000000);	// read 1 word from area 00001
}


// send a SCP request to read <len> bytes from address <addr>
void AnalyserDoc::SendReadRequest(int len, uint32_t addr)
{
		// request the debug buffer etc status
	uint8_t b[6];	// reading, so no data
	b[0] = (uint8_t)(len >> 8);
	b[1] = (uint8_t)len;
	b[2] = (uint8_t)(addr >> 24);
	b[3] = (uint8_t)(addr >> 16);
	b[4] = (uint8_t)(addr >> 8);
	b[5] = (uint8_t)addr;
	TxMessage(b, 6);
}


// process an incoming data message
void AnalyserDoc::ReceiveData(uint8_t * b, int len)
{
	SaveMessage(b, len, 'r');
	len -= 4;				// length of data
	if (len < 0) return;	// no data

	uint32_t addr = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
	b += 4;

		// now <addr> is the address and b[0] to b[len-1] the data
	int i, j, k, m, n;
	uint32_t p, q;
	std::string s;
	if (addr < 0x08000000) goto default_action;	// area 00000

	if (addr < 0x10000000) {
			// area 00001: debug capture etc status
		if (req_issued) switch (pending_console_input[0]) {
case 's':
case 'c':
case 'e':
case 'f':	pending_console_input.clear();
		}

		p = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
		if (p == server_state) return;	// no change
		server_state = p;
		ConsoleLine(ServerState());	// assumed nonempty
		unless (state == MGT_ST_CONN_MADE) return;
		state = MGT_ST_ACTIVE;
		if (server_state & 0x00400000) return;
		j = (server_state & 0x00FFFFFF) | 0x00400000;
		b[0] = 0;
		b[1] = 4;
		b[2] = 8;
		b[3] = 0;
		b[4] = 0;
		b[5] = 0;
		b[6] = (uint8_t)(j >> 24);
		b[7] = (uint8_t)(j >> 16);
		b[8] = (uint8_t)(j >> 8);
		b[9] = (uint8_t)j;
		TxMessage(b, 10);
		return;
	}

	if (addr < 0x18000000) {
			// area 00010: block RAMs
		if (addr >= 0x17000000) {
				// console data (unsolicited)
			s = ">>> ";
			i = 0;
			while (i < len) s.push_back(b[i++]);
			ConsoleLine(s);
			return;
		}

			// here if not console data: assume it's from the debug buffer; 
			//		expect one of 32 tranches, each 1KB (= 8Kb)
		unless (req_issued && pending_console_input[0] == 'd' && 
				len == 1024 && (addr & 0x07FC1FFF) == 0) goto default_action;

			// ask for the next tranche
		if (addr < 0x1003E000) SendReadRequest(1024, addr + 0x2000);
		else AckConsole();

			// dump the debug data, ignoring lines (other than the first in 
			//		the tranche) that are the same as the previous one
			// unlike the <scp_eth> logic, the data isn't shuffled
		if ((addr & 0x6000) == 0) {
				// write the captions: we repeat them every 64 lines
			if ((addr & 0x38000) == 0) {
					// first tranche: leave a blank line
				ConsoleLine("");

				if (dbg_st != (server_state & (1 << 26))) {
						// first read the DBF file
					dbg_st = server_state & (1 << 26);
					s = (dbg_st == 0) ? "logic" : "vm";
					s += theApp.vm4scp ? ".9t4dbf" : ".9t3dbf";
					dbg_caption = ReadFile(s);
					until (dbg_caption.empty() || !dbg_caption.back().empty()) 
														dbg_caption.pop_back();
					until (dbg_caption.empty() || !dbg_caption.front().empty()) 
										dbg_caption.erase(dbg_caption.begin());
					if (dbg_caption.empty()) {
						ConsoleLine("Cannot read " + s + " (or is empty)");
						s = "44444444 44444444 44444444 44444444";	// 128 bits
						dbg_format = s + "  " + s + "   " + s + "  " + s;
					}
					else {
						dbg_format = dbg_caption.back();
						do dbg_caption.pop_back(); 
									until (dbg_caption.empty() || 
												!dbg_caption.back().empty());
					}
				}
			}

			j = dbg_caption.size();
			i = 0;
			while (i < j) {
				ConsoleLine("     " + dbg_caption[i]);
				i += 1;
			}
		}

		k = 0;	// line number within tranche
		do {
			if (k > 0 && memcmp(b + (k * 64), b + ((k-1) * 64), 64) == 0) continue;

				// template is:
				//	- 1 to 4 = next 1 to 4 bits as a hex digit; 
				//	- b = 1 bit ignored, output as a single space; 
				//	- 8,c,e = 1,2,3 bits as ms bits of a hex digit; 
				//	- m = 1 bit, 'M' for 1, 'R' for 0 (provided for VM3.1 
				//		microcode addresses, now deprecated); 
				//	- upper case letters = 1 bit, letter for 1, space for 0; 
				//	- ? = 1 bit, '?' for 1, space for 0; 
				//	- $$$ = 5 bits as 3-character register name; 
				//	- h = 4 bits ignored, output as a single space; 
				//	- w = 16 bits ignored, output as a single space; 
				//	- other characters verbatim without consuming any data
			p = 0;	// offset into template
			j = k * 512;	// bit offset from start of data
			s = ToHex(((addr >> 9) & 0x3FF) + k, 3) + ": ";
			while (p < dbg_format.length()) {
				char c = dbg_format[p];
				p += 1;
				if (c == 'b') { j += 1; s += ' '; continue; }
				if (c == 'h') { j += 4; s += ' '; continue; }
				if (c == 'w') { j += 16; s += ' '; continue; }
				i = (b[j/8] << (j & 7)) & 255;	// bit addressed by <j> in d7
				if ((j & 7) >= 4) i |= b[(j/8)+1] >> (8 - (j & 7));
				if ((c >= 'A' && c <= 'Z') || c == '?') {
					s += (i & 128) ? c : ' ';
					j += 1;
				}
				else switch (c) {
		case '4':
		case '3':
		case '2':
		case '1':
					s += ToHex(i >> ('8' - c));
					j += c - '0';
					break;
		case '8':
					s += (i & 128) ? '8' : '0';
					j += 1;
					break;
		case 'c':
					s += ToHex((i >> 4) & 12);
					j += 2;
					break;
		case 'e':
					s += ToHex((i >> 4) & 14);
					j += 3;
					break;
		case 'm':
					s += (i & 128) ? 'M' : 'R';
					j += 1;
					break;
		case 'b':
					s += (i & 128) ? '?' : ' ';
					j += 1;
					break;
		case 'h':
					s += (i & 240) ? '?' : ' ';
					j += 4;
					break;
		case '$':
					p += 2;	// consume 2 more characters (assumed $$)
					s += RegName(i >> 3);
					j += 5;
					break;
		default:
					s += c;
				}
			}

			ConsoleLine(s);
		} while (++k < 16);

		return;
	}

	if (addr < 0x20000000) {
			// area 00011 (VM's pointer registers): we always read them all
		unless (len == (33 * 4) && addr == 0x18000000) goto default_action;

		GetBigendianWords(vm_reg, b, 33);
		if (context & SCP_CONTEXT_PRE_M) {
				// fetching the registers preparatory to listing stack frames
			context &= ~ SCP_CONTEXT_PRE_M;

			unless (theApp.vm4scp ? vm4frame.empty() : frame.empty()) {
				if (context & SCP_CONTEXT_L) {
					if ((vm_reg[VM3REG_LP] & 0xE0000000) == 0x40000000) {
						if (theApp.vm4scp) {
							if (Vm4SetOwner(vm_reg[32])) {
								SendReadRequest(1280, 
												vm_reg[VM4REG_FP] - 10208);
								lp = vm_reg[VM4REG_LP];
								gp = vm_reg[VM4REG_GP];
								return;
							}
						}
						else {
							if (SetOwner(vm_reg[32])) {
								SendReadRequest(1280, 
												vm_reg[VM3REG_FP] - 10208);
								lp = vm_reg[VM3REG_LP];
								gp = vm_reg[VM3REG_GP];
								return;
							}
						}
					}
				}
				else if (theApp.vm4scp) { // just listing the globals (VM4)
					if ((vm_reg[VM4REG_GP] & VM4_AREA_MASK) == 
													VM4_FRAME_STACK_AREA) {
						owner = 0;	// <pc_offset> not required for globals
						SendReadRequest(1280, vm_reg[VM4REG_GP] + 
							(vm4frame.at(0).length & 0xFFFFFFE0) - 10144);
						lp = vm_reg[VM4REG_GP];
						return;
					}
				}
				else { // just listing the globals (VM3 version)
					if ((vm_reg[VM3REG_GP] & 0xE0000000) == 0x40000000) {
						owner = 0;	// <pc_offset> not required for globals
						SendReadRequest(1280, vm_reg[VM3REG_GP] + 
							(frame.at(0).bits_length & 0xFFFFFFE0) - 10144);
						lp = vm_reg[VM3REG_GP];
						return;
					}
				}
			}
		}

			// here to dump the registers in hex
		ConsoleLine("");
		s = "VM registers";
		j = 0;
		do {
			if ((j & 7) == 0) {
				ConsoleLine(s);
				s = "   ";
			}
			else s += ", ";
			s += RegName(j) + " = ";
			if (theApp.vm4scp) {
					// translate back to VM4 pointer format; note that when 
					//		SCP translates to VM3 format it converts global 
					//		addresses to frame stack addresses
				i = vm_reg[j]; // NB <i> is signed, vm_reg[j] isn't
				if (i < 0) s += '8'; // heap address
				else {
					s += ToHex((i >> 28) & 0xE, 1);
					i &= 0x1FFFFFFF;
				}
				s += ' ' + ToHex((i >> 5) & 0x3FFFFFF, 7) + ' ' + 
														ToHex(i & 31, 2);
			}
			else s += ToHex(vm_reg[j], 8);
		} while (++j < 32);
		ConsoleLine(s);
		ConsoleLine("    PC = " + ToHex(vm_reg[32], 8));
		AckConsole();
		return;
	}

		// here if reading from the VM's cache (incl reading back after write)
	if ((context & SCP_CONTEXT_PRE_M) && 
									addr == 0x88000000 && len == (37 * 4)) {
			// reading the heap area base, the first stage for l, g, and m
			// next stage is to read the VM's registers; if this stage fails 
			//		(e.g. can't find symbols file) they'll be listed in hex
		SendReadRequest(33 * 4, 0x18000000);

		unless ((context & SCP_CONTEXT_L) == 0 || b[32] == 0) {
				// output the error message
			b[131] = 0;	// ensure it's NUL-terminated (truncates to 99 chars)
			s = (char *)b + 32;
			ConsoleLine(s);
			ConsoleLine("");
		}

			// read and decode the symbols file; use the back end of <b> as a 
			//		buffer
		snprintf((char *)b + 512, 512, "VM-%d-%d-%d-%d~%d-%d-%d", b[24], 
									b[25], b[26], b[27], b[5], b[6], b[7]);
		s = (char *)b + 512;
		unless (b[4] == 0) s += "-BETA-" + ToDecimal(b[4]);
		s += (theApp.vm4scp ? ".9t4sym" : ".9t3sym");
		sym = ReadFile(s);
		if (sym.empty()) {
			ConsoleLine("Symbols file " + s + " not found (or is empty)");
			context &= ~ SCP_CONTEXT_ALL_M;
			return;
		}
		j = sym.size() - 1;	// line number of last line
		if (j < 3) {
			ConsoleLine("Symbols file " + s + " is too short to be valid");
			context &= ~ SCP_CONTEXT_ALL_M;
			return;
		}
		unless (sym.at(0) == (theApp.vm4scp ? 
							  ":#ninetiles.com::9tos3:symbols:VM4:0" : 
							  ":#ninetiles.com::9tos3:symbols:VM3:0")) {
			ConsoleLine("Wrong header on symbols file " + s);
			context &= ~ SCP_CONTEXT_ALL_M;
			return;
		}
		until (sym.at(j) == ":end") {
			if (j < 4) {
				ConsoleLine("No ':end' line found in symbols file " + s);
				context &= ~ SCP_CONTEXT_ALL_M;
				return;
			}
			j -= 1;
		}

			// now read the symbol definitions for the globals and the stack 
			//		frames; we expect the globals to come before the stack 
			//		frames, and the stack frames to be in increasing order 
			//		of entrypoint address
			// note that we don't need to read the labels because it's only 
			//		the entrypoints we are interested in, and they also 
			//		appear in the stack frames table
			// <j> is the line number of the ":end" line (at least 3)
		i = 1;
		until (sym.at(i++) == ":globals") if (i >= j) {
			ConsoleLine("Global variables not found in " + s);
			context &= ~ SCP_CONTEXT_ALL_M;
			return;
		}

		if (theApp.vm4scp) {
			vm4frame.clear();
			unless (DecodeVm4Frame(i)) {
				ConsoleLine("Global variables invalid format in " + s);
				context &= ~ SCP_CONTEXT_ALL_M;
				return;
			}
			until (sym[i++] == ":frames") if (i >= j) {
				ConsoleLine("Local variables not found in " + s);
				context &= ~ SCP_CONTEXT_ALL_M;
				return;
			}
			while (DecodeVm4Frame(i)) { }
		}
		else {
			FrameInfo fi;
			fi.addr = 0;
			fi.name = "[globals]";
			fi.bits_length = 0;
			fi.ptrs_length = 0;
			i = DecodeFrame(fi, i);

			until (sym[i++] == ":frames") if (i >= j) {
				ConsoleLine("Local variables not found in " + s);
				context &= ~ SCP_CONTEXT_ALL_M;
				return;
			}

				// <fi> is the globals and <i> points to the first of the 
				//		entrypoints
			frame.clear();
			frame.push_back(fi);	// globals are element 0

			while (true) {
				s = sym[i++];
				unless (sscanf_s(s.c_str(), "%x %x %n", 
								&fi.addr, &fi.ptrs_length, &j) > 0) break;
				fi.name = s.substr(j);
				fi.bits_length = 0;
				fi.v.clear();
				i = DecodeFrame(fi, i);
				frame.push_back(fi);
			}

			fi.addr = INT_MAX;
			fi.name = "[end]";
			fi.v.clear();
			frame.push_back(fi);

			unless (s == ":end") ConsoleLine('"' + s + 
											"\" where \":end\" expected");
		}

			// now <frame> holds all the information about the variables
			// collect the start of the heap from <buf> so it can be re-used
			// +++ not needed if looking at the heap is moved to 'h' command
//		uint32_t heap_base[4]; // segment header, free record header, free list
//		GetBigendianWords(heap_base, buf + 138, 4);

			// +++ there is some other information in the heap area base that 
			//		we don't currently use, e.g. the flash area header, the 
			//		IP/CP value (which we maybe should check against the IP/CP 
			//		register), and the word that is written with CCCC by 
			//		<SysCrashInConsole> and DDDD by <CrashIfDebug>
		return;
	}

	if (lp && len == 1280 && (context & SCP_CONTEXT_M) && 
										(addr & 0xE0000000) == 0x40000000) {
			// reading frame stack to list it
			// note that we always read 320 words (1280 bytes), even if that 
			//		goes beyond the bottom of the stack
			// we put the data in the low half of <frame_data>, first moving 
			//		the existing data up if it's adjacent
		if (frame_addr == addr + (320 * 32)) {
				// have the adjacent tranche
			memcpy(frame_data + 320, frame_data, 1280);
		}
		GetBigendianWords(frame_data, b, 320);
		frame_addr = addr;

		if (theApp.vm4scp) while (true) {
				// here for each stack frame if VM4
				// <lp> (which we've checked is nonzero) defines where the 
				//		frame is and <owner> defines its format
				// <frame_data> contains data from <frame_addr> to at least 
				//		the top of the frame
				// check the whole frame is available
				// +++ currently the global stack frame (always likely to be 
				//		the biggest) is under 400 words; if it gets to be 
				//		more than 640 we'll need to enlarge the <vm_data> 
				//		array; beware that this code uses literal numbers for 
				//		the size
				// note that C++ is picky about how you initialise "by 
				//		reference" variables, so we have to declare <f> here
			Vm4FrameInfo& f = vm4frame.at(owner);	// the frame to be listed
			if (lp < frame_addr + 64) {
					// don't have the whole frame so must read another tranche
					// set <i> to the number of bits of the frame in the top 
					//		half of the buffer
					// +++ beware that some of the terms on the rhs are 
					//		unsigned, so if it was calculated as a number of 
					//		bytes or words by including a division or shift 
					//		the result wouold always be positive
				i = (lp + f.length - frame_addr) - (318 * 32);
				if (i > 0) {
						// we can't simply read the next 320 words, because 
						//		that would kick out some data we still need
					if (f.length > (638 * 32)) {
							// frame is bigger than <frame_data>
						ConsoleLine("*** Frame for " + f.name + 
													" is too big to list");
						return;
					}

						// else realign the data so this frame goes to the top
					memmove(frame_data, frame_data + (i / 32), 320 * 4);
					frame_addr += i;
				}

					// now we can send the request to read another tranche
				SendReadRequest(1280, frame_addr - (320 * 32));
				return;
			}

				// now the whole frame is in the buffer
			if (owner == 0) s = "[globals] at ";
			else s = f.name + " + " + ToDecimal(pc_offset) + 
							" (" + ToHex(f.addr + pc_offset) + ") at ";
			ConsoleLine(s + ScpPointerToHex(lp));

				// list the variables
			std::vector<Vm4VariableInfo>::iterator vp = f.v.begin();
			std::vector<Vm4VariableInfo>::iterator v_end = f.v.end();
			until (vp == v_end) {
				s = "    " + vp->name;
					// ident for the variable, but with P in the VM3 format 
					//		used by SCP
					// +++ we only store the first three words; for local 
					//		variables s, x, and e are always zero and we 
					//		don't list the value of "by reference" parameters 
					//		unless they're on the frame stack; in the case of 
					//		a subarray that extends outside the array bounds 
					//		we will list x and e but not include the 
					//		extension bits in the value as listed
				int32_t v_id[3]; // first 3 words of ident
				if (vp->ref) {
						// it's a parameter called by reference
					s += " (";
					i = (lp + vp->offset - frame_addr) >> 5;
					v_id[0] = frame_data[i++];	// P
/* convert to SCP format
					if ((v_id[0] & VM4_AREA_MASK) == VM4_HEAP_AREA) 
						v_id[0] = 0x80000000 | ((v_id[0] << 5) & 0x7FFFFFFF);
					else v_id[0] = (v_id[0] & 0x60000000) | 
											((v_id[0] << 5) & 0x1FFFFFFF);*/
					v_id[1] = frame_data[i++];	// f
					v_id[2] = frame_data[i++];	// m

					if ((v_id[0] & VM4_AREA_MASK) == 0) {
						v_id[0] = 0; // set to <nil>
						s += "nil";
					}
					else { // list the ident
						s += ToHex(v_id[0], 8) + ' ' + 
									ToHex(v_id[1]) + ' ' + ToHex(v_id[2]);
						j = frame_data[i++];	// s,x
						k = frame_data[i];		// e
						unless (j == 0 && k == 0) { // include ext bits etc
							if (j < 0) s += " s";
							s += ' ' + ToHex(j & 0x7FFFFFFF) + ' ' + ToHex(k);
						}
					}
					s += ')';
				}
				else {
						// it's a normal variable; create the ident
					v_id[0] = lp; // already in SCP format
					v_id[1] = vp->offset;
					v_id[2] = vp->length;
				}

					// now output the value in hex showing the alignment
				i = v_id[0] + v_id[1] - frame_addr;
				if (i >= 0 && (i + v_id[2]) <= 640*32) {
						// value is within the window
					s += " =";
					k = 0; // set nonzero if value is truncated
					if (v_id[2] > 23*32) {
						k = 1;
						v_id[2] = 22*32 - (v_id[1] & 31);
					}
					uint32_t * p = frame_data + (i >> 5); // word cont'g 1st bit
					j = i & 31; // bits to skip in first word
					n = j + v_id[2] - 32; // bits beyond the first word
					q = *p++;
					if (j == 0) j = 8; // digits to list
					else {
						q &= (UINT32T_MAX >> j);
						j = 8 - (j >> 2);
					}
					while (n >= 0) {
						s += ' ' + ToHex(q, j);
						j = 8;
						n -= 32;
						q = *p++;
					}
					if (n > -32) { // <n + 32> bits at ms end of <q>
						q &= (UINT32T_MAX << -n);
						s += ' ' + ToHex(q, (3 - n) >> 2);
					}

					if (k) s += " ... (" + ToDecimal(v_id[2]) + ')';

						// +++ to do: check whether to list according to the 
						//		type
				}

				ConsoleLine(s);
				vp++;
			}

			if (owner == 0) {
					// we've just listed the globals so it's all done
				ConsoleLine("");
				context &= ~ SCP_CONTEXT_G;
				return;
			}
				// now move on to the next frame
				// set <p> to the return address and <lp> to the frame below
				// for the last frame, we assume at least one of them will 
				//		be <nil> +++ spec for VM4 is tbc
			p = frame_data[((lp - 64) - frame_addr) >> 5];
			if (p == 0) {
					// end of local frames
				context &= ~ SCP_CONTEXT_L;
				unless (context & SCP_CONTEXT_G) {
						// all done
					AckConsole();
					return;
				}
					// else move on to the globals
				lp = gp;
				owner = 0;	// <pc_offset> not required for globals
				continue;
			}
			else {
				lp = frame_data[((lp - 32) - frame_addr) >> 5];
					// convert the address (which we assume isn't in the heap) 
					//		to the format SCP uses
				lp = ((lp & VM4_AREA_MASK) | ((lp & 0x00FFFFFF) << 5));
			}

			unless ((lp & VM4_AREA_MASK) == VM4_FRAME_STACK_AREA) {
					// not a frame stack address
				ConsoleLine("*** Bad 'next frame' pointer " + ToHex(lp, 8));
				AckConsole();
				return;
			}

			unless (Vm4SetOwner(p)) {
				ConsoleLine("*** Bad return address " + ToHex(p, 8));
				AckConsole();
				break;
			}
		}
		else while (true) {
				// here for each stack frame if VM3
				// <lp> (which we've checked is nonzero) defines where the 
				//		frame is and <owner> defines its format
				// <frame_data> contains data from <frame_addr> to at least 
				//		the top of the frame
				// check the whole frame is available
				// +++ currently the global stack frame (always likely to be 
				//		the biggest) is about 360 words; if it gets to be 
				//		more than 640 we'll need to enlarge the <vm_data> 
				//		array; beware that this code uses literal numbers for 
				//		the size
				// note that C is picky about how you initialise "by 
				//		reference" variables, so we have to declare <f> here
			FrameInfo& f = frame.at(owner);	// the frame to be listed
			if (lp < frame_addr + f.ptrs_length + 32) {
					// don't have the whole frame so must read another tranche
					// set <i> to the number of bits of the frame in the top 
					//		half of the buffer
					// +++ beware that some of the terms on the rhs are 
					//		unsigned, so if it was calculated as a number of 
					//		bytes or words by including a division or shift 
					//		the result wouold always be positive
				i = (lp + f.bits_length - frame_addr) - (319 * 32);
				if (i > 0) {
						// we can't simply read the next 320 words, because 
						//		that would kick out some data we still need
					if (f.ptrs_length + f.bits_length > (638 * 32)) {
							// frame is bigger than <frame_data>
						ConsoleLine("*** Frame for " + f.name + 
													" is too big to list");
						return;
					}

						// else realign the data so this frame goes to the top
					memmove(frame_data, frame_data + (i / 32), 320 * 4);
					frame_addr += i;
				}

					// now we can send the request to read another tranche
				SendReadRequest(1280, frame_addr - (320 * 32));
				return;
			}

				// now the whole frame is in the buffer
			if (owner == 0) ConsoleLine("[globals], GP = " + ToHex(lp));
			else ConsoleLine(f.name + " + " + ToDecimal(pc_offset) + 
					" (" + ToHex(f.addr + pc_offset) + "), LP = " + ToHex(lp));

				// list the variables
			std::vector<VariableInfo>::iterator vp = f.v.begin();
			std::vector<VariableInfo>::iterator v_end = f.v.end();
			until (vp == v_end) {
				s = "    " + vp->name;
					// lvalue of the variable: [0] = VM address of first 
					//		(lowest-addressed) pointer, [1] = ((number of 
					//		pointers) - 1) * 32, [2] = VM address of first 
					//		bit, [3] = (number of bits) - 1
					// +++ NOTE: this ignores any extension bits; ought at 
					//		least to flag their existence
				uint32_t lv[4];
				if (vp->llfmt) {
						// it's a parameter called by reference
						// we assume the lvalue is a fieldspec
					s += " (";
					i = (lp + vp->bits_offset - frame_addr) >> 5;
					if (vp->cpts == 'P' || vp->cpts == '2') {
							// pointers component
						lv[0] = frame_data[i++];	// FP
						lv[1] = frame_data[i];		// LP
						i += 2; // +++ skip extensions, see above
						if (lv[1] > lv[0] || (lv[0] & 0xE0000000) == 0) {
							lv[0] = 0; // set to <nil>
							s += "nil";
						}
						else {
							lv[1] = lv[0] - lv[1];
							s += ToHex(lv[0]) + ' ' + ToDecimal(lv[1]);
						}
					}
					if (vp->cpts == '2') s += ' ';
					unless (vp->cpts == 'P') {
							// bitstring component
						lv[2] = frame_data[i++];	// FB
						lv[3] = frame_data[i];		// LB
						if (lv[3] < lv[2] || (lv[2] & 0xE0000000) == 0) {
							lv[2] = 0; // set to <nil>
							s += "nil";
						}
						else {
							lv[3] -= lv[2];
							s += ToHex(lv[2]) + ' ' + ToDecimal(lv[3]);
						}
					}
					s += ')';
				}
				else {
						// it's a normal variable; collect the lvalue into 
						//		<lv>
						// we collect both components; if one is unused it 
						//		will have negative length
					lv[0] = lp - vp->ptrs_offset;
					lv[1] = vp->ptrs_length;
					lv[2] = lp + vp->bits_offset;
					lv[3] = vp->bits_length;
				}

					// now output the value
				s += " =";
				if (vp->cpts == 'P' || vp->cpts == '2') {
						// have a pointers component
					i = lv[0] - frame_addr;
					if ((lv[0] & 0xE0000000) == 0) s += " nil";
					else if (i < (int)lv[1] || i >= 640*32) {
							// it isn't within the window; this should only 
							//		happen with "by reference" values; the 
							//		test for being above the top of the 
							//		window could be more sophisticated (e.g. 
							//		we'll only list the first 6 or 12) but in 
							//		practice it should never be above this 
							//		frame (though it might be on the heap, or 
							//		in the statics)
							// +++ it also seems to happen with global 
							//		pointers, so maybe we need a bigger 
							//		window
							// +++ we ought to make more of an effort to get 
							//		as much of the stack as possible into the 
							//		window, maybe also to fetch values from 
							//		other areas; eventually need a UI that 
							//		will allow the user to retrieve any 
							//		values including dunping selected heap 
							//		records
						s += " ???";
					}
					else {
						k = 0;
						j = lv[1] >> 5;
						if (j > 11) {
							if (vp->cpts == '2') { k = 1; j = 9; }
							else if (j > 23) { k = 1; j = 21; }
						}

						i = i >> 5;
						do s += ' ' + ToHex(frame_data[i--], 8); 
															while (--j >= 0);
						if (k) s += " ... (" + ToHex(lv[0],8) + ')';
					}
				}

				if (vp->cpts == '2') s += " /";

				unless (vp->cpts == 'P') {
						// have a bitstring component
					i = lv[2] - frame_addr;
					if ((lv[2] & 0xE0000000) == 0) s += " nil";
					else if (i < 0 || i + lv[3] >= 640*32) {
							// it isn't within the window; this should only 
							//		happen with "by reference" values; see 
							//		note on pointers component above
						s += " ???";
					}
					else {
						uint32_t * p = frame_data + (i >> 5); // word cont'g 1st bit
						k = 0; // set nonzero if value is truncated
						if (lv[3] > 11*32) {
								// truncate to 12 or 24 words, keeping the alignment
							if (vp->cpts == '2') { k = 1; lv[3] = 9*32 | (lv[3] & 31); }
							else if (lv[3] > 23*32) { k = 1; lv[3] = 21*32 | (lv[3] & 31); }
						}
						i += lv[3]; // index to last bit
						j = (i + 1) & 31; // 0 if aligned, else bits in last word
						uint32_t v_buf[24]; // buffer to hold the value if shifting
						uint64_t acc;
						i = i >> 5;
						unless (j == 0) {
							acc = ((uint64_t)frame_data[i]) << j;
							m = lv[3];
							p = v_buf + 24;
							do {
								acc = (acc >> 32) | (((uint64_t)frame_data[--i]) << j);
								*--p = (uint32_t)acc;
								m -= 32;
							} until (m < 0);
						}
							// now <p> points to the first word, <lv[3] + 1> is the number 
							//		of bits, right aligned
						j = (~lv[3]) & 31; // number of unused bits at the top
						n = *p++;
						unless (j == 0) {
								// sign extend or zero extend into unused bits
							i = (j == 31) ? 1 : (1 << (31 - j)); // top bit
							if (vp->cpts == 'S' && (n & i) != 0) n |= -i;
							else n &= (i << 1) - 1;
						}

							// now <n> is the first word and <p> points to the next
						if (lv[3] < 64) {
								// small enough to be listed in decimal
							if (vp->cpts == 'S') {
								int64_t acc_s = n;
								if (lv[3] > 31) acc_s = (acc_s << 32) | *p;
								s += ' ' + ToDecimal(acc_s);
							}
							else if (vp->cpts == 'U') {
								acc = n & 0xFFFFFFFF;
								if (lv[3] > 31) acc = (acc << 32) | *p;
								s += ' ' + ToUnsigned(acc);
							}
							s += " =";
						}	

							// here to list in hex; <k> shows whether it's been 
							//		trunctated
						i = 0;
						j = lv[3] >> 5; // number of words after the first
						s += ' ' + ToHex(n & 0xFFFFFFFF, ((lv[3] & 31) >> 2) + 1);
						while (i < j) s += ' ' + ToHex(p[i++], 8);
						if (k) s += " ... (" + ToHex(lv[2],8) + ')';

							// +++ to do: check whether to list as characters
					}
				}

				ConsoleLine(s);
				vp++;
			}

			if (owner == 0) {
					// we've just listed the globals so it's all done
				ConsoleLine("");
				context &= ~ SCP_CONTEXT_G;
				return;
			}
				// now move on to the next frame
				// set <p> to the return address and <lp> to the frame below
				// for the last frame, we assume at least one of them will 
				//		be <nil>; ought to tighten up the spec so the return 
				//		address from the outer level is specified to be <nil> 
				//		and its chain point to the globals
				// +++ currently (8 Aug 2020) the Compiler sets the return 
				//		address for SysEntryInit to <nil> which means if 
				//		control does return from it deletion of pointers 
				//		will carry on past it into the global bitstring; 
				//		need to specify it as an entryploint for address 
				//		zero; the code below accepts either
			p = frame_data[((lp - (f.ptrs_length + 32)) - frame_addr) >> 5];
			lp = frame_data[(lp - frame_addr) >> 5];
			if ((p == 0 || p == 7) && lp == gp) {
					// end of local frames
				context &= ~ SCP_CONTEXT_L;
				unless (context & SCP_CONTEXT_G) {
						// all done
					AckConsole();
					return;
				}
					// else move on to the globals
				owner = 0;	// <pc_offset> not required for globals
				continue;
			}

			unless ((lp & 0xE0000000) == 0x40000000) {
					// not a frame stack address
				ConsoleLine("*** Bad 'next frame' pointer " + ToHex(lp, 8));
				AckConsole();
				return;
			}

			unless (SetOwner(p)) {
				ConsoleLine("*** Bad return address " + ToHex(p, 8));
				AckConsole();
				break;
			}
		}
	}

default_action:
		// here to list the incoming data in hex
	p = addr & 0xFFFFFF00;	// first address on first line
	q = addr + (len * 8);	// address after last to be listed
	std::string s_a;
	do {
		s = theApp.vm4scp ? ScpPointerToHex(p) : ToHex(p, 8);
		s += ": ";
		s_a.clear();
		do {
			unless (p & 0x78) s += ' ';
			unless (p & 0x18) s += ' ';
			if (p < addr || p >= q) { s += "  "; s_a += ' '; }
			else {
				uint8_t ch = b[(p - addr) / 8];
				s += ToHex(ch, 2);
				if (ch < 33 || ch > 126) s_a += (ch == 32) ? '_' : '.';
				else s_a += ch;
			}
			p += 8;
		} while (p & 0xF8);

		ConsoleLine(s + "  " + s_a);
	} while (p < q);
	AckConsole();
}


// Commands that can be typed into the console window
// Whereas the Mac version waited for a reply, in this version displaying of incoming 
//		data is decoupled from the commands, which simply send the request that kicks 
//		off the process. If the request is ignored (e.g. asks to access a VM resource 
//		while the VM is running), nothing further will happen.
// In the Mac version, state was preserved in the local variables and return address 
//		while data was being fetched; here we need to preserve it in the <AnalyserDoc> 
//		object. This isn't a problem for most commands (though it requires the code 
//		to be structured rather differently from the Mac version) but significantly 
//		affects 'h', the heap check. Currently it isn't supported (and shouldn't be 
//		needed as long as the VM's heap management is unchanged); it probably needs 
//		to be a separate thread (see the Windows <CreateThread> routine) with 
//		<DownloadBuffer> getting this thread to send the request and then hanging 
//		until this thread forwards the reply, but might be implemnted using further 
//		SCP_CONTEXT_ bits and probably some additional state variables.

// process input from the "console" window which talks to SCP
// <ch> is the character that was typed
// might be better to use dialogue boxes for R and W, as in 9tXP
void AnalyserDoc::InputChar(char ch)
{
	if (ch == '\b') {
			// delete a character from the command line
		AckConsole();
		if (pending_console_input.empty()) return;
		pending_console_input.pop_back();
		UpdateDisplay();
		return;
	}
	unless (ch == '\r') {
			// add a character to the command line, ignoring leading white space
		AckConsole();
		unless (isspace(ch) && pending_console_input.empty()) 
													pending_console_input += ch;
		UpdateDisplay();
		return;
	}

		// here with <pending_console_input> holding the command when "enter" typed
	int i,j,k;
	uint8_t b[38];
	uint32_t w[9];
	uint32_t v;
	uint8_t * q;
	std::string s;

	switch (pending_console_input[0]) {
case 'r':
			// read 128 words from address following 'r', or next block of 
			//		512 bytes if no address specified
			// for VM4 we translate to the VM3 format the SCP engine uses
		if (sscanf_s(pending_console_input.c_str() + 1, 
						 " %x", &last_r_addr) == 1) { // have an address
			if (theApp.vm4scp && (last_r_addr & VM4_AREA_MASK)) { // convert
				if ((last_r_addr & VM4_AREA_MASK) == VM4_HEAP_AREA) 
							last_r_addr = 0x80000000 | (last_r_addr << 5);
				else last_r_addr = (last_r_addr & 0x60000000) | 
									((last_r_addr << 5) & VM4_ADDRESS_MASK);
			}
		}
		else last_r_addr +=  4096;
		SendReadRequest(512, last_r_addr);
		req_issued = true;
		return;

case 'w':
		unless ((k = sscanf_s(pending_console_input.c_str() + 1, 
					" %x %x %x %x %x %x %x %x %x", &w[0], &w[1], &w[2], 
					&w[3], &w[4], &w[5], &w[6], &w[7], &w[8]), k > 1)) break;
		if (theApp.vm4scp) w[0] = (w[0] & 0xE0000000) | 
												((w[0] & 0x03FFFFE0) << 5);
			// write up to 8 words starting at <w[0]>; separated by white 
			//		space
			// we ask for them to be read back, though <ReceiveData> doesn't 
			//		check the values
		b[0] = 0;
		b[1] = (uint8_t)((k - 1) * 4);	// up to 32 bytes
		q = b + 2;
		i = 0;
		do {
			j = 3;
			v = w[i];
			do {
				q[j] = (uint8_t)v;
				v >>= 8;
				j -= 1;
			} while (j >= 0);
			i += 1;
			q += 4;
		} while (i < k);
		TxMessage(b, q - b);
		req_issued = true;
		return;

case 'd':
			// dump the debug data
			// ask for the first tranche
		SendReadRequest(1024, 0x10000000);
		req_issued = true;
		return;

case 'p':
			// dump the VM's pointer registers (NB 'r' is used for "read")
			// read 33 words from address zero in area 00011
		SendReadRequest(33 * 4, 0x18000000);
		req_issued = true;
		return;

case 'q':
			// read 1 word from address zero in area 00001
		SendReadRequest(4, 0x08000000);
		req_issued = true;
		return;

case 'm':
			// memory dump (frame stack): locals and globals
		context = SCP_CONTEXT_ALL_M;
		SendReadRequest(37 * 4, 0x88000000);
		req_issued = true;
		return;

case 'l':
			// locals only in frame stack dump
		context = SCP_CONTEXT_PRE_M | SCP_CONTEXT_L;
		SendReadRequest(37 * 4, 0x88000000);
		req_issued = true;
		return;

case 'g':
			// globals only in frame stack dump
		context = SCP_CONTEXT_PRE_M | SCP_CONTEXT_G;
		SendReadRequest(37 * 4, 0x88000000);
		req_issued = true;
		return;

default:
		pending_console_input.clear();
		ConsoleLine("Commands are as follows; single-key commands:");
		ConsoleLine("    ESC = switch to console display");
		ConsoleLine("    @ = cycle round all the SCP sessions");
		ConsoleLine("    ~ = toggle inclusion of protocol messages in the display");
		ConsoleLine("  Commands terminated by ENTER; <h> is a hex number:");
		ConsoleLine("    d = debug dump: list the contents of the debug buffer");
		ConsoleLine("    p = pointers: list the pointer registers (rubbish if VM running)");
		ConsoleLine("    q = query status: re-read the status word");
		ConsoleLine("  For the next four, <h> is optional; if present it is a new value for ");
		ConsoleLine("             DBG_TRIGGER_MASK in the logic");
		ConsoleLine("    c <h> = continuous: set controls to capture until triggered");
		ConsoleLine("    s <h> = single: set controls to capture one bufferful after trigger");
		ConsoleLine("    e <h> = enable: reset the trigger");
		ConsoleLine("    f <h> = force trigger: cause a trigger event");
		ConsoleLine("  The other six are ignored by the SCP server if the VM is running");
		ConsoleLine("    l = locals: list crash message and frame stack except the globals");
		ConsoleLine("    g = globals: list crash message and global stack frame");
		ConsoleLine("    m = memory: list crash message and frame stack including the globals");
		ConsoleLine("    w <h0> <h1> <h2> ... = write: <h0> is a non-nil VM address,");
		ConsoleLine("             1 to 8 further values are 32-bit words to be written");
		ConsoleLine("    r <h> = read 128 words from <h> (8 digits; VM address unless ms digit 0 or 1)");
		return;

case 'e':
			// reset trigger for debug
		k = 0;
		break;

case 'f':
			// force trigger for debug
		k = 0x01800000;
		j = -1;
		break;

case 'c':
			// set debug controls for continuous capture (until trigger)
		k = 0x00B00000;
		j = -1;
		break;

case 's':
			// set debug controls for single capture (starting on trigger)
		k = 0x00B00000;
		j = 0x00800000;
	}

		// here for commands that request writing area 00001: the format is:

/*			 d31-26: reserved; ignored but should be coded as 0
--				d25: 1 = start VM (ignored if already running)
--				d24: 1 = force trigger event
--				d23: 0 = hold capture logic in reset, 1 = enable
--				d22: 1 = send unsolicited message when either "captured" bit changes
--				d21: 1 = collect single bufferful to buffers A, starting on trigger; 
--						0 = collect continuously to buffers A, stopping on trigger
--				d20: as d21, for buffers B
--			  d19-0: controls used for logic tracing "trigger" and "include" signals
*/
		// <k> has a 1 in each bit of the value to be written that is to be taken 
		//		from <j>, 0 for those that are to be taken from a standard value 
		//		with d31-23 zero, d22 = 1, d21-0 from <server_state>
		// if the command line includes a hex number, it replaces d19-0 and any 
		//		higher bits are XOR'd into the word that would otherwise be written
		// for the 'e' command, a second word is written with d23 set
	req_issued = true;
	j = (j & k) | ((0x00400000 | (server_state & 0x003FFFFF)) & ~ k);
	if (sscanf_s(pending_console_input.c_str() + 1, " %x", &i) == 1) 
														j = (j & 0xFFF00000) ^ i;
	b[0] = 0;
	b[1] = 4;
	b[2] = 8;
	b[3] = 0;
	b[4] = 0;
	b[5] = 0;
	b[6] = (uint8_t)(j >> 24);
	b[7] = (uint8_t)(j >> 16);
	b[8] = (uint8_t)(j >> 8);
	b[9] = (uint8_t)j;
	if (k) {
		TxMessage(b, 10);
		return;
	}
		// here for 'e'
	b[10] = b[6];
	b[11] = b[7] | 0x80;
	b[12] = b[8];
	b[13] = b[9];
	TxMessage(b, 14);
}


// set <owner> and <pc_offset> for VM3 format
// <pc> is a bit address pointing somewhere in the routine; if it's equal to 
//		an entrypoint we assume it's on the last operation of the preceding 
//		routine
// returns whether successful; false if there are no routines in <frame> or 
//		<pc> comes before the first or is not a valid address; we assume the 
//		last routine extends to the top of memory
bool AnalyserDoc::SetOwner(uint32_t pc)
{
	int j = frame.size();
	if (j < 2) return false;	// no routines (may or may not be globals)
	unless (pc & 1) return false;	// not a valid address
	pc = pc >> 3;	// convert to byte address
	if (pc < frame[1].addr) return false;

		// use binary chop to find the routine
	int i = 1;
		// <pc> is in or after <i> but before <j>
	while ((j - i) > 1) {
		int k = (i + j) / 2;
		if (pc > frame.at(k).addr) i = k; else j = k;
	}
		// now <i> and <j> are adjacent so we want entry <i>
	owner = i;
	pc_offset = pc - frame.at(i).addr;
	return true;
}


// set <owner> and <pc_offset> for VM4 format
// <pc> is a bit address pointing somewhere in the routine; if it's equal to 
//		an entrypoint we assume it's on the last operation of the preceding 
//		routine
// returns whether successful; false if there are no routines in <frame> or 
//		<pc> comes before the first or is not a valid address; we assume the 
//		last routine extends to the top of memory
bool AnalyserDoc::Vm4SetOwner(uint32_t pc)
{
	int j = vm4frame.size();
	if (j < 2) return false;	// no routines (may or may not be globals)
	unless (pc & 1) return false;	// not a valid address
	pc = pc >> 3;	// convert to byte address
	if (pc < vm4frame[1].addr) return false;

		// use binary chop to find the routine
	int i = 1;
		// <pc> is in or after <i> but before <j>
	while ((j - i) > 1) {
		int k = (i + j) / 2;
		if (pc > vm4frame.at(k).addr) i = k; else j = k;
	}
		// now <i> and <j> are adjacent so we want entry <i>
	owner = i;
	pc_offset = pc - vm4frame.at(i).addr;
	return true;
}


// collect and return information on the variables in a stack frame
// <i> is the index into <sym> for the frame heading (entrypoint, bit string 
//		length, and name)
// sets <i> to the index into <sym> for the first line that doesn't fit the 
//		format
// returns whether the heading was valid; if true a <Vm4FrameInfo> object 
//		has been added to <vm4frame> and <i> has been advanced; if false, an 
//		[end] line has been added and <i> is unchanged
bool AnalyserDoc::DecodeVm4Frame(int& i)
{
	int j;
	Vm4VariableInfo vi;
	Vm4FrameInfo fi;
	std::string s = sym[i];
	unless (sscanf_s(s.c_str(), "%x %x %n", &fi.addr, &fi.length, &j) > 1) {
		fi.addr = INT_MAX;
		fi.name = "[end]";
		vm4frame.push_back(fi);
		return false;
	}
	fi.name = s.substr(j);
	while (true) {
		i += 1;
			// read variable; we expect tab, 3 characters (of which the first 
			//		can be a space but the other two won't), two hex numbers, 
			//		and the name
		s = sym[i];
		if (s.length() < 15) break;
		unless (s[0] == '\t') break;
		if (s[1] == 'R') vi.ref = true;
		else if (s[1] == ' ') vi.ref = false;
		else break;
		vi.cpts = s[2];
		vi.fmt = s[3];

		unless (sscanf_s(s.c_str(), " %*s %x %x %n", 
									&vi.offset, &vi.length, &j) > 1) break;
		vi.name = s.substr(j);
		fi.v.push_back(vi);
	}
	vm4frame.push_back(fi);
	return true;
}


// collect and return information on the variables in a stack frame
// <i> is an index into <sym>
// updates <v> and <bits_length> in <fi>, adding the variables to the end 
//		of <v>
// returned value is index into <sym> for the first line that doesn't fit 
//		the format for a variable
int AnalyserDoc::DecodeFrame(FrameInfo& fi, int i)
{
	int j, n;
	VariableInfo vi;

	while (true) {
			// read variable
		std::string s = sym[i];
		char cp[4];
		unless (s[0] == '\t') return i;	// variables all begin with tab
		unless (sscanf_s(s.c_str(), " %s", &cp, 3) == 1) return i;
		j = (cp[0] == 'F') ? 1 : 0;
		vi.llfmt = (int8_t)j;
		vi.cpts = cp[j];

		if (vi.cpts == '2' && j == 0) {
				// two components
			unless (sscanf_s(s.c_str(), " %*s %x %x %x %x %n", 
							&vi.ptrs_offset, &vi.ptrs_length, 
						&vi.bits_offset, &vi.bits_length, &n) > 3) return i;
			j = vi.bits_offset + vi.bits_length -31;
		}
		else if (vi.cpts == 'P' && j == 0) {
				// pointers component only
			vi.bits_offset = 0;
			vi.bits_length = -1;
			unless (sscanf_s(s.c_str(), " %*s %x %x %n", 
						   &vi.ptrs_offset, &vi.ptrs_length, &n) > 1) return i;
				// <j> is already zero
		}
		else {
				// pure bit string, including "by reference" case
			vi.ptrs_offset = 0;
			vi.ptrs_length = -32;
			unless (sscanf_s(s.c_str(), " %*s %x %x %n", 
						   &vi.bits_offset, &vi.bits_length, &n) > 1) return i;
			j = vi.bits_offset + vi.bits_length -31;
		}

		vi.name = s.substr(n);
		fi.v.push_back(vi);
		if (j > fi.bits_length) fi.bits_length = j;
		i += 1;
	}
}


// return name of register <n> as 3 characters (padded on the left with spaces)
// high bits of <n> are ignored, so the top half of the register file can be 
//		addressed as either -16 to -1 or 16 to 31
std::string AnalyserDoc::RegName(int n)
{
	n &= 31;

	if (theApp.vm4scp) switch (n) {
case VM4REG_NIL:
		return ("NIL");
case VM4REG_RP:
		return (" RP");
case VM4REG_GP:
		return (" GP");
case VM4REG_LP:
		return (" LP");
case VM4REG_CP:
		return (" CP");
case VM4REG_W:
		return ("  W");
case VM4REG_TL:
		return (" TL");
case VM4_SAVE_SP:
		return ("SSP");
case VM4_SAVE_FP:
		return ("SFP");
case VM4_SAVE_LP:
		return ("SLP");
case VM4REG_YL:
		return (" YL");
case VM4REG_SP:
		return (" SP");
case VM4REG_YP:
		return (" YP");
case VM4REG_YF:
		return (" YF");
case VM4REG_FP:
		return (" FP");
case VM4REG_ZL:
		return (" ZL");
case VM4REG_XL:
		return (" XL");
case VM4REG_ZP:
		return (" ZP");
case VM4REG_XP:
		return (" XP");
case VM4REG_ZF:
		return (" ZF");
case VM4REG_XF:
		return (" XF");
	}
	else switch (n) {
case VM3REG_NIL:
		return ("NIL");
case VM3REG_RP:
		return (" RP");
case VM3REG_GP:
		return (" GP");
case VM3REG_LP:
		return (" LP");
case VM3REG_IP:
		return (" IP");
case VM3REG_NP:
		return (" NP");
case VM3REG_HP:
		return (" HP");
case VM3REG_W:
		return ("  W");
case VM3REG_Q:
		return ("  Q");
case VM3REG_YLB:
		return ("YLB");
case VM3REG_SP:
		return (" SP");
case VM3REG_TP:
		return (" TP");
case VM3REG_QP:
		return (" QP");
case VM3REG_YFB:
		return ("YFB");
case VM3REG_P:
		return ("  P");
case VM3REG_FP:
		return (" FP");
case VM3REG_ZLB:
		return ("ZLB");
case VM3REG_XLB:
		return ("XLB");
case VM3REG_ZLP:
		return ("ZLP");
case VM3REG_XLP:
		return ("XLP");
case VM3REG_ZFB:
		return ("ZFB");
case VM3REG_XFB:
		return ("XFB");
case VM3REG_ZFP:
		return ("ZFP");
case VM3REG_XFP:
		return ("XFP");
	}

	if (n < 10) return " r" + ToDecimal(n);
	return 'r' + ToDecimal(n);
}

