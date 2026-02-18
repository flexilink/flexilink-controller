/*
 *  Common\string_extras.cpp
 *  Editor
 *
 *  Created by John Grant on 14-12-2010.
 *  Copyright 2010-2019 Nine Tiles. All rights reserved.
 *
 */

// For Windows this file needs to be #included in a .cpp file in 
//		the project directory; otherwise the compiler complains 
//		it can't find stdafx.h
 
#include "string_extras.h"

// return VM4 pointers map <p> compacted by replacing adjacent entries with 
//		the same f value by a single entry
// used where <p> has been built by concatenating maps for components of a 
//		complex value, e.g. a pointer followed by an ident will have two 
//		bytes with f=1, n=1; this replaces them by one byte with f=1, n=2
// if <len> is more than the length (i.e. sum of all n) excluding the non-
//		pointer words at the end, adds or adjusts f=0 entry at the end so 
//		the total number of words is <len>
extern ByteString TidyPointersMap(ByteString p, int len)
{
	p.push_back(0); // add terminator
	int k = 0; // accumulates size (in words)
	int i = 1; // read pointer
	int j = 0; // write pointer
	uint8_t s = p.at(0);
	uint32_t b = s; // current byte (but with n more than just 7 bits)
	until (s == 0) {
		s = p.at(i++); // following byte
		if ((b ^ s) & 1) { // different f values
			k += b >> 1;
			while (b > 0xFF) {
					// needs more than one byte; NB can't need more than in 
					//		the original so <j> won't overtake <i>
				p.at(j++) = b | 0xFE;
				b -= 0xFE; }
			p.at(j++) = b;
			b = s;
		}
		else b += s & 0xFE;
	}

		// now the map has been written except for the non-pointer area at 
		//		the end
		// <k> = number of words in map so far
		// <b> = twice the number of non-pointer words at the end
		// <j> = current length of map
	if (len > k) b = (len - k) << 1;
	p.resize(j);
	if (b > 0) {
		i = (b - 1) / 254; // number of bytes needed with n=127
		p.resize(j + i, 254);
		p.push_back(b - (i * 254));
	}
	return p;
}


// convert signed integer (up to 64 bits) to decimal
std::string ToDecimal(int64_t n)
{
	if (n >= 0) return ToUnsigned((uint64_t)n);

		// NB if n is INT64_MIN there is some overflow but it 
		//		still gives the right answer
	return '-' + ToUnsigned((uint64_t)(-n));
}

// convert unsigned integer (up to 64 bits) to decimal
std::string ToUnsigned(uint64_t n)
{
//	if (n == 0) return std::string("0");

	std::string s;
	do {
		uint64_t m = n / 10;
		s = (char)((n - (m * 10)) + '0') + s;
		n = m;
	} while (n != 0);
	return s;
}

// convert unsigned integer (up to 64 bits) to hex
// adds leading zeroes to make result at least <min_len> digits
std::string ToHex(uint64_t n, int min_len)
{
	std::string s;
	do {
		uint8_t ch = (n & 15) + '0';
		if (ch > '9') ch += 'A' - ('9' + 1);
		s = (char)ch + s;
		n = n >> 4;
	} while (n != 0);
	min_len -= s.size();
	if (min_len <= 0) return s;
	return std::string(min_len, '0') + s;
}

// return the ls <len> bits of <n> in binary
// sign extends <n> if <len> is more than 64
std::string ToBinary(int64_t n, int len)
{
	std::string s;
	if (len >= 64) {
		s = std::string(len - 63, (n < 0) ? '1' : '0');
		len = 63;
	}
	n = n << (64 - len);
	while (len > 0) {
		s += (n < 0) ? '1' : '0';
		len -= 1;
		n = n << 1;
	};
	return s;
}


// return a 4-character string with non-ASCII characters replaced by '.'
std::string ToText(uint32_t n)
{
	std::string s;
	int i = 24;
	do {
		int c = (n >> i) & 255;
		i -= 8;
		s.push_back((c < 0x20 || c >= 0x7F) ? '.' : (char)c);
	} while (i >= 0);
	return s;
}


// return IPv4 address <n> in dotted decimal form
std::string Ip4AddrString(uint32_t n)
{
	std::string s = ToDecimal(n >> 24);
	int i = 16;
	do {
		s += '.';
		s += ToDecimal((n >> i) & 255);
		i -= 8;
	} while (i >= 0);
	return s;
}


// return text representing OID o (empty if bad format)
// the first octet is unscrambled, e.g. 0x2B is rendered as "1.3", not "43"
// any arc values that don't fit in 64 bits are rendered as the residue 
//		modulo 2**64
std::string OidToText(ByteString o) {
	std::string s;
	size_t len = o.size();
	if (len == 0) return s;	// NB <size_t> is unsigned
	size_t j = o.at(0);
	unless (j < 120 && (o.at(len-1) & 0x80) == 0) return s;

		// here if OK
	size_t i = j / 40;	// <i> in range 0 to 2
	s = (char)(i + '0');
	s += '.';
	s += ToUnsigned(j - (i * 40));
	i = 1;
	while (i < len) {
			// add next arc to <s>
			// note that we know the last byte has d7=0 so will be the end 
			//		of an arc
		uint64_t n = 0;
		do {
			j = o.at(i);
			i += 1;
			n = (n << 7) | (j & 0x7F); } while (j & 0x80);
		s += '.';
		s += ToUnsigned(n);
	}
	return s;
}


// return 8-digit hex number representing the VM4 address equivalent to VM3 
//		address <n> (on the assumption that it's a VM4 address that the SCP 
//		engine has converted to VM3 format)
std::string ScpPointerToHex(uint32_t n) {
	if (n & 0x80000000) n = 0x80000000 | ((n & 0x7FFFFFFF) >> 5);
	else n = (n & 0xE0000000) | ((n & 0x1FFFFFFF) >> 5);
	return ToHex(n, 8);
}


// return the value of <n>; <d_size> is the number of bits in a digit (i.e. 
//		the log to base 2 of the radix)
// assumes <d_size> is in the range 1 to 4, all characters are digits 
//		appropriate to the radix, and the result fits in 64 bits, i.e. that 
//		caller has checked the string is valid
uint64_t FromHexEtc(std::string n, int d_size)
{
	uint64_t r = 0;		// for the result
	size_t i = 0;
	while (i < n.size()) {
		uint8_t c = n[i++];
		if (c > '9') c += 9;	// convert A-F to hex 4A-4F and a-f to 6A-6F
		r = (r << d_size) | (c & 15);
	}
	return r;
}


// return the value of decimal integer <n>
// assumes all bytes are decimal digits whose value is the ls 4 bits of the 
//		code (as is true for ASCII digits); returns UINT64T_MAX if the result 
//		is more than 18,446,744,073,709,551,614
extern uint64_t FromDecimal(std::string n)
{
	uint64_t r = 0;		// for the result
	size_t i = 0;
	while (i < n.size()) {
		if (r >= 1844674407370955161ull) {
			if (r > 1844674407370955161ull) return UINT64T_MAX;
				// else r * 10 = 18,446,744,073,709,551,610
			if ((n[i] & 15) > 4) return UINT64T_MAX;
		}
		r = (r * 10) + (n[i++] & 15);
	}
	return r;
}


// return text form of 3-bit request code <c> in management message header
CString MgtReqCode(int c)
{
	switch (c) {
#ifdef _UNICODE
case 0: return L"Get";
case 1: return L"GetNext";
case 2: return L"Status";
case 3: return L"Set";
case 4: return L"NvSet";
case 7: return L"ConsoleData";
#else
case 0: return "Get";
case 1: return "GetNext";
case 2: return "Status";
case 3: return "Set";
case 4: return "NvSet";
case 7: return "ConsoleData";
#endif
	}
	CString s;
#ifdef _UNICODE
	s.Format(L"code %d", c);
#else
	s.Format("code %d", c);
#endif
	return s;
}


// versions of <isspace> etc that avoid the problems with Microsoft's library
// Microsoft's version of <isspace> (for instance) takes an <int> parameter 
//		which it requires to be in the range -1 to 255, and <char> is 
//		signed, so character values 0x80 to 0xFE (i.e. anything non-ASCII, 
//		whether or not using UTF8) get faulted
// Also, we want to restrict ourselves to ASCII and mathematical symbols, so 
//		don't need the complications of checking for accented characters etc

// white space: space or any control character except DEL
extern "C" __inline bool IsSpace(char c) { return (uint8_t)c < 33; }
extern "C" __inline bool IsSpaceW(wchar_t c) { return (uint16_t)c < 33; }
// digit
extern "C" __inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }
// alphabetic, numeric, and underline
extern "C" __inline bool IsAlnumU(char c) { 
	if (c <= 'Z') {
		if (c <= '9') return c >= '0';
		return c >= 'A';
	}
	if (c >= 'a') return c <= 'z';
	return c == '_';
}


// number of characters in a UTF8 string
// returns the number of bytes that don't have value 10xxxxxx; doesn't 
//		check they are correctly preceded by a 11xxxxxx byte
int Utf8Length(std::string s)
{
	int len = s.size();
	int i = 0;	// index into string
	int n = 0;	// number of 10xxxxxx bytes
	while (i < len) if ((int8_t)(s[i++]) < -64) n += 1;
	return len - n;
}

/*
// convert Windows 1252 encoding to UTF-8
// returned value is 1 to 3 bytes
extern std::string W1252ToUtf8(char c)
{
	std::string s;
	switch (c & 0xE0) {
			// ASCII characters (check for NUL)
case 0:
case 0x20:
case 0x40:
case 0x60:
		s = c;
		return;

			// codes with 10 in the top 2 bits
case 0xA0:
		s = (char)0xC2;
		s += c;
		return;

			// codes with 11 in the top 2 bits
case 0xC0:
case 0xE0:
		s = (char)0xC3;
		s += c;
		return;
	}

		// here for code points 0x80-0x9F, which don't map directly onto 
		//		Unicode
	switch (c) {
case 0x80:	return "\xE2\x82\xAC";
case 0x82:	return "\xE2\x80\x9A";
case 0x83:	return "\xC6\x92";
case 0x84:	return "\xE2\x80\x9E";
case 0x85:	return "\xE2\x80\xA6";
case 0x86:	return "\xE2\x80\xA0";
case 0x87:	return "\xE2\x80\xA1";
case 0x88:	return "\xCB\xC6";
case 0x89:	return "\xE2\x80\xB0";
case 0x8A:	return "\xC5\xA0";
case 0x8B:	return "\xE2\x80\xB9";
case 0x8C:	return "\xC5\x92";
case 0x8E:	return "\xC5\xBD";
case 0x91:	return "\xE2\x80\x98";
case 0x92:	return "\xE2\x80\x99";
case 0x93:	return "\xE2\x80\x9C";
case 0x94:	return "\xE2\x80\x9D";
case 0x95:	return "\xE2\x80\xA2";
case 0x96:	return "\xE2\x80\x93";
case 0x97:	return "\xE2\x80\x94";
case 0x98:	return "\xCB\x9C";
case 0x99:	return "\xE2\x84\xA2";
case 0x9A:	return "\xC5\xA1";
case 0x9B:	return "\xE2\x80\xBA";
case 0x9C:	return "\xC5\x93";
case 0x9E:	return "\xC5\xBE";
case 0x9F:	return "\xC5\xB8";
	}

		// here for the code points that aren't defined in W-1252: return 
		//		non-breaking space
	return "\xC2\xA0";
}
*/

// convert Windows 1252 encoding to 16-bit Unicode
// returns non-breaking space for the unassigned codes
extern uint16_t W1252ToUnicode(uint8_t c)
{
	unless ((c & 0xE0) == 0x80) return c;

		// here for code points 0x80-0x9F, which don't map directly onto 
		//		Unicode
	switch (c) {
case 0x80:	return 0x20AC;
case 0x82:	return 0x201A;
case 0x83:	return 0x0192;
case 0x84:	return 0x201E;
case 0x85:	return 0x2026;
case 0x86:	return 0x2020;
case 0x87:	return 0x2021;
case 0x88:	return 0x02C6;
case 0x89:	return 0x2030;
case 0x8A:	return 0x0160;
case 0x8B:	return 0x2039;
case 0x8C:	return 0x0152;
case 0x8E:	return 0x017D;
case 0x91:	return 0x2018;
case 0x92:	return 0x2019;
case 0x93:	return 0x201C;
case 0x94:	return 0x201D;
case 0x95:	return 0x2022;
case 0x96:	return 0x2013;
case 0x97:	return 0x2014;
case 0x98:	return 0x02DC;
case 0x99:	return 0x2122;
case 0x9A:	return 0x0161;
case 0x9B:	return 0x203A;
case 0x9C:	return 0x0153;
case 0x9E:	return 0x017E;
case 0x9F:	return 0x0178;
	}

		// here for the code points that aren't defined in W-1252: return 
		//		non-breaking space
	return 0x00A0;
}


// <n> is a "name" (in practice it can be any text) and <d> is a "dictionary" 
//		consisting of names separated by '|' and with '|' at the beginning 
//		and end
// returns -1 if <n> not in the list, else the number of names preceding it
extern int FindName(std::string n, std::string d)
{
	n = '|' + n + '|';
	size_t p = d.find(n);
	if (p == std::string::npos) return -1;
	size_t i = 0;
	int j = 0;
	ASSERT(d[0] == '|');
	while (i < p) { // count names before the one we've found
		i = d.find('|', i + 1);
		j += 1;
	}
	return j;
}


// return <n> as the top 13 bits of one half of an IT packet header, 
//		with the CRC in the ls 3 bits
// this is not the fastest way to calculate the CRC; the way <AddHec> in the VM 
//		code does it would be faster, and quickest would be to have a table lookup 
//		(32KB, or 3002 bytes if we reckon lengths and flow labels will never be 
//		more than 1500 and the packet type will always be zero)
int AddHec(int n)
{
	int x = n & 0x1FFF;	// in case <n> wasn't 13 bits
	int i = 13;
	do {
		x = x << 1;
		if (x & 0x2000) x ^= 0x2C00; // if shifted a 1 out
		i -= 1;
	} while (i > 0);
	
	return (uint16_t)((n << 3) | ((x >> 10) ^ 7));
}


// return the coding of v as an immediate value if possible, else -1
// uses the codings defined for VM3
// note that -1 will be returned if v is INT64T_MIN
// coding has d63-34 zero, d33-32 ls 2 bits of op code byte
// when d33 = 0 (no immediate bytes), d31-0 are zero so coding of 0 is 0
// else immediate bytes are left aligned in d31-0, with unused low bytes 
//		holding sign extension, so the value will still be correct if the 
//		value in the "number of further octets" field (d29-28) is increased
// uses the shortest possible coding
// note that the codings for 0 and -1 have zero in d29-28, the same as 
//		IMM_CODING_ZERO and IMM_CODING_M_1
int64_t ImmediateCodingVm3(int64_t v)
{
	if (v == 0) return 0;					// value is 0: d33-32 = 00
	if (v == -1) return IMM_CODING_SIGN;	// value is -1: d33-32 = 01
	if (v < - (1LL << 32) || v >= (1LL << 32)) return -1; // > 33 bits
	
		// here if needs bytes beyond the op code, having checked it fits 
		//		in 32 bits + sign
	int64_t n;	// coding excl "number of further octets" field
	if ((v & 0xFFFF) == 0 || (v & 0xFFFF) == 0xFFFF) {
		n = 11LL << 30;	// set d33, align to d15 (d31-30 = 3)
		v = v >> 15;
	}
	else {
		n = 8LL << 30;	// align to d0
		while ((v & 7) == 0 || (v & 7) == 7) {
				// align 2 places further left
			v = v >> 2;
			n += 1 << 30;
			if (n & (1LL << 31)) break;
		}
	}
	n |= (v & 15) << 24;
	v = v >> 4;		// shift out the 4 bits that are in the first byte
	n |= (v & 0xFF) << 16;
	n |= v & 0xFF00;
	n |= (v >> 16) & 0xFF;
	uint64_t n_len = 0;	// "further" octets; 4 if can't represent at all
	if (v < 0) {
		n |= IMM_CODING_SIGN;	// sign bit in op code
		while (v < -1) { v = v >> 8; n_len += 1; }
	}
	else while (v > 0) { v = v >> 8; n_len += 1; }
		// now <n_len> is in the range 0 to 4; we can only use codings 
		//		that have <n_len> in the range 0 to 3
	if (n_len < 4) return n | (n_len << 28);
	return -1;
}


// return the coding of v as an immediate value if possible, else -1
// uses the codings defined for VM4; we only use the "pointers map" code 
//		points for positive values where it gives a shorter encoding, 
//		regarding the others as reserved
// note that -1 will be returned if v is INT64T_MIN
// coding has d63-34 zero, d33-32 ls 2 bits of op code byte
// when d33 = 0 (no immediate bytes), d31-0 are zero so coding of 0 is 0
// else immediate bytes are left aligned in d31-0, with unused low bytes 
//		holding sign extension, so the value will still be correct if the 
//		value in the "number of further octets" field (d29-28) is increased
// uses the shortest possible coding
// note that the codings for 0 and -1 have zero in d29-28, the same as 
//		IMM_CODING_ZERO and IMM_CODING_M_1
int64_t ImmediateCodingVm4(int64_t v)
{
	if (v == 0) return 0;					// value is 0: d33-32 = 00
	if (v == -1) return IMM_CODING_SIGN;	// value is -1: d33-32 = 01
	if (v < - (1LL << 32) || v >= (1LL << 32)) return -1; // > 33 bits
	
		// here if needs bytes beyond the op code, having checked it fits 
		//		in 32 bits + sign
	int64_t n;	// coding excl "number of further octets" field
	n = 8LL << 30;	// align to d0
	while ((v & 7) == 0 || (v & 7) == 7) {
			// align 2 places further left
		v = v >> 2;
		n += 1 << 30;
		if (n & (1LL << 31)) break;
	}
	n |= (v & 15) << 24;
	v = v >> 4;		// shift out the 4 bits that are in the first byte
	n |= (v & 0xFF) << 16;
	n |= v & 0xFF00;
	n |= (v >> 16) & 0xFF;
	uint64_t n_len = 0;	// "further" octets; 4 if can't represent at all
	if (v < 0) {
		n |= IMM_CODING_SIGN;	// sign bit in op code
		while (v < -1) { v = v >> 8; n_len += 1; }
	}
	else while (v > 0) { v = v >> 8; n_len += 1; }
		// now <n_len> is in the range 0 to 4; we can only use codings 
		//		that have <n_len> in the range 0 to 3
	if (n_len >= 4) return -1;
	if (n_len == 3 && (n & (3LL << 31)) == (1LL << 31) && 
													(n & 0xFF0000) == 0) {
			// positive value aligned to d4 (code 10) which needs 3 extra 
			//		bytes but can be coded with fewer using the "pointers 
			//		map" alignment
		n |= (n & 0xFF) << 16; // move ms byte to new alignment
		n &= -256;
		n_len = (n & 0xFF00) ? 6 : 5; // new length, & change alignment
	}
	return n | (n_len << 28);
}


// as <ImmediateCoding> (see above) but always uses the maximum length
// intended for cases where the value needs to be changed at a late stage 
//		of code generation, see also IMM_CODING_4_WORDS
int64_t ImmediateCodingMaxLength(int64_t v)
{
	if (v < - (1LL << 32) || v >= (1LL << 32)) return -1; // > 33 bits
	int64_t n;	// coding with "number of further octets" = 3
	if ((v & 0xFFFF) == 0 || (v & 0xFFFF) == 0xFFFF) {
		n = 0x2FLL << 28;
		v = v >> 15;
	}
	else {
		n = 0x23LL << 28;
		while ((v & 7) == 0 || (v & 7) == 7) {
				// align 2 places further left
			v = v >> 2;
			n += 1 << 30;
			if (n & (1LL << 31)) break;
		}
	}
	if (v < - (1LL << 28) || v >= (1LL << 28)) return -1; // doesn't fit
	n |= (v & 15) << 24;
	v = v >> 4;		// shift out the 4 bits that are in the first byte
	n |= (v & 0xFF) << 16;
	n |= v & 0xFF00;
	n |= (v >> 16) & 0xFF;
	return (v >= 0) ? n : n | IMM_CODING_SIGN;
}


// return whether <v> is within the range for a "short form" value on the 
//		operand stack
inline bool ShortForm(int64_t v)
{
	return v >= - 0x80000000LL && v < 0x60000000LL;
}


// copy <n> words from <b> (which is in network byte order) to <w>
void GetBigendianWords(uint32_t * w, uint8_t * b, int n)
{
	int i = 0;
	n *= 4;
	while (i < n) {
		w[i >> 2] = (b[i] << 24) | (b[i+1] << 16) | (b[i+2] << 8) | b[i+3];
		i += 4;
	}
}

