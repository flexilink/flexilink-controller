/*
 *  string_extras.h
 *  routines to fill in some of the gaps in std::string
 *	also a few other useful things
 *
 *  Created by John Grant on 14-12-2010.
 *  Copyright 2010-2024 Nine Tiles. All rights reserved.
 *
 */
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <map>

// vector types
typedef std::vector<uint8_t> ByteString;
typedef std::vector<std::string> StringArray;
// type used for <CodeFragment::param>
// +++ was previously unsigned but with the new definition of extension 
//		bits is better signed; should consider converting <ByteString> too 
//		though note some code (e.g. <Makefile::PutControlWord>) expects it 
//		to be unsigned
typedef std::vector<int32_t> ParamString;

// compact VM4 pointers map <p>
extern ByteString TidyPointersMap(ByteString p, int len = -1);

// append <b> to the end of <a>: <a> and <b> must be vectors of the same type
#define APPEND(a,b) a.insert(a.end(), b.begin(), b.end())

// character representations of values
extern std::string ToDecimal(int64_t n);
extern std::string ToUnsigned(uint64_t n);
extern std::string ToHex(uint64_t n, int min_len = 0);
extern std::string ToBinary(int64_t n, int len);
extern std::string ToText(uint32_t n);
extern std::string Ip4AddrString(uint32_t n);
extern std::string OidToText(ByteString o);
extern std::string ScpPointerToHex(uint32_t n);

extern uint64_t FromHexEtc(std::string n, int d_size);
extern uint64_t FromDecimal(std::string n);

extern CString MgtReqCode(int c); // as in Tealeaves <MgtMsgHdr.msg_type>

// routines similar to the C library functions but avoiding the problems 
//		with the Windows implementation
extern "C" bool IsSpace(char c);
extern "C" bool IsSpaceW(wchar_t c);
extern "C" bool IsDigit(char c);
extern "C" bool IsAlnumU(char c);

// number of characters in a UTF8 string
extern int Utf8Length(std::string s);
// convert Windows 1252 encoding to UTF-16
extern uint16_t W1252ToUnicode(uint8_t c);

// helper routine for CCompilerDoc::OnActionsGeneratemicrocode
extern int FindName(std::string n, std::string d);

// CRC calculation for Flexilink IT packet headers
extern int AddHec(int n);
// label on rcv packets for the signalling flow, including the CRC 
//		(assumed to be flow 0 for <LinkSocket::flow>)
#define RCV_SIG_FLOW	7

// VM code "immediate data" values, see ImmediateCoding for more details
// IMM_CODING_4_WORDS is ImmediateCodingMaxLength(0), used as a placeholder 
//		in the initialisation code which is replaced by the true value when 
//		the code has been assembled
#define IMM_CODING_ZERO (1LL << 33)		// 0 coded with an extra byte
#define IMM_CODING_M_1 (0x30FLL << 24)	// -1 coded with an extra byte
#define IMM_CODING_ONE (0x201LL << 24)	// ImmediateCoding(1)
#define IMM_CODING_31 (0x281LL << 24)	// ImmediateCoding(word length - 1)
#define IMM_CODING_M_31 (0x311FELL << 16)// ImmediateCoding(1 - word length)
#define IMM_CODING_4_WORDS (0x2FLL << 28)// placeholder (see above)
#define IMM_CODING_SIGN (1LL << 32)		// sign bit, also coding for -1
#define IMM_CODING_HAVE_DATA (1LL << 33)// bit that shows imm data present
#define IMM_CODING_PMAP_SINGLE (0x2D403LL<<16)// ptrs map for single pointer
extern int64_t ImmediateCodingVm3(int64_t v);
extern int64_t ImmediateCodingVm4(int64_t v);
extern int64_t ImmediateCodingMaxLength(int64_t v); // always 4 octets

// whether <v> is within the range for a "short form" operand stack value
extern bool ShortForm(int64_t v);

#ifndef WINVER
#ifdef DEBUG
#define ASSERT(p) assert(p)
#else
#define ASSERT(p) {}
#endif
#endif

#define unless(a) if (!(a))
#define until(a) while (!(a))

// copy between byte array in network byte order and word array
extern void GetBigendianWords(uint32_t * w, uint8_t * b, int n);

// limit values
// this horrible kludge is needed for Windows, which has #defined <min> 
//		and <max> as macros; the "::" to their left doesn't stop the 
//		preprocessor making the substitution, and if you kill off the 
//		macros by defining NOMINMAX some vital part of the Windows support 
//		code fails to compile; I've made it conditional on WINVER being 
//		defined, which ought to mean it will only apply to Windows

#ifdef WINVER
#define INT64T_MIN	INT64_MIN
#define INT64T_MAX	INT64_MAX
#define UINT64T_MAX UINT64_MAX
#define INT32T_MIN	INT32_MIN
#define INT32T_MAX	INT32_MAX
#define UINT32T_MAX UINT32_MAX
#else
#include <limits>
#define INT64T_MIN	(std::numeric_limits<int64_t>::min())
#define INT64T_MAX	(std::numeric_limits<int64_t>::max())
#define UINT64T_MAX (std::numeric_limits<uint64_t>::max())
#define INT32T_MIN	(std::numeric_limits<int32_t>::min())
#define INT32T_MAX	(std::numeric_limits<int32_t>::max())
#define UINT32T_MAX (std::numeric_limits<uint32_t>::max())
#endif  /* WINVER */
