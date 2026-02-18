/*
 *  extras.cpp
 *  Editor
 *
 *  Import the utility routines
 *
 *	I couldn't find a way of getting it to compile when I added 
 *		string_extras.cpp to the project, because it couldn't find 
 *		stdafx.h unless I added "../Compiler" to the path, and when I did 
 *		that it didn't recognise it as the pre-compiled header.
 *
 *	Having created this dummy source file to import it, I've also put the 
 *		Microsoft-specific utilities here.
 *
 */
#include "stdafx.h"
#include "../Common/string_extras.cpp"

#include "extras.h"

// return a Windows "wide character" string corresponding to UTF8 string <s>
// assumes all characters are in the Basic Multilingual Plane (BMP), i.e. 
//		fit in a single "wide character"
// if any illegal UTF8 sequences are found, we assume <s> is Windows-1252
extern CStringW Utf8ToUnicode(std::string s)
{
	int len = s.size();
	int i = 0;	// index into <s>
	int c, k;
	CStringW r;	// for the result
	while (i < len) {
		c = s[i++];		// NB <char> is signed so d7 is the sign
		if (c >= 0) {
				// ASCII code point
			r.AppendChar((wchar_t)c);
			continue;
		}
			// here if first byte of a multibyte character
		if ((c & 0x40) == 0) goto not_utf8;
		if ((c & 0x20) != 0) {
				// 3- or 4-byte character
			unless ((c & 0x10) == 0 && i < len) goto not_utf8;
			k = s[i++];
			unless (k < -64) goto not_utf8;	// code points 80 to BF
			c = ((c & 0x0F) << 6) | (k & 0x3F);
		}
		else c &= 0x1F;	// 2-byte character

			// here to collect the last byte of a multibyte character
			// <c> holds the bits from the previous byte(s), aligned at d0
		unless (i < len) goto not_utf8;
		k = s[i++];
		unless (k < -64) goto not_utf8;	// code points 80 to BF
		r.AppendChar((wchar_t)((c << 6) | (k & 0x3F)));
	}
	return r;

not_utf8:
		// here if illegal UTF8: start again assuming Windows-1252
	r.Empty();
	i = 0;
	while (i < len) r.AppendChar(W1252ToUnicode(s[i++]));
	return r;
}


// return a UTF8 string corresponding to Windows "wide character" string <w>
// assumes all characters are in the Basic Multilingual Plane (BMP), i.e. 
//		fit in a single "wide character"
// +++ debug version ASSERTs if <w> includes a surrogate code; ought to 
//		translate valid surrogate pairs into a 4-byte UTF8 character
extern std::string UnicodeToUtf8(CStringW w)
{
	int len = w.GetLength();
	int i = 0;	// index into <w>
	uint16_t c;
	std::string r;	// for the result
	while (i < len) {
		c = w[i++];
		if (c < 0x80) {
				// ASCII code point
			r.push_back((char)c);
			continue;
		}
			// here if needs more than one byte
		if (c < 0x800) r.push_back((char)(0xC0 | (c >> 6))); // fits in two
		else {
				// needs more than two
			ASSERT((c & 0xFC00) != 0xD800); // doesn't need four
			r.push_back((char)(0xE0 | (c >> 12)));
			r.push_back((char)(0x80 | ((c >> 6) & 0x3F)));

		}
		r.push_back((char)(0x80 | (c & 0x3F))); // ls 6 bits in either case
	}
	return r;
}


// return a UTF8 string corresponding to Unicode Basic Multilingual Plane 
//		(BMP) code point <c>
// +++ debug version ASSERTs if <c> is a surrogate code
extern std::string UnicodeToUtf8(uint16_t c)
{
	if (c < 0x80) return std::string(1, (char)c);	// ASCII code point

			// here if needs more than one byte
	std::string r;	// for the result
	if (c < 0x800) r.push_back((char)(0xC0 | (c >> 6))); // fits in two
	else {
			// needs more than two
		ASSERT((c & 0xFC00) != 0xD800); // doesn't need four
		r.push_back((char)(0xE0 | (c >> 12)));
		r.push_back((char)(0x80 | ((c >> 6) & 0x3F)));

	}
	r.push_back((char)(0x80 | (c & 0x3F))); // ls 6 bits in either case
	return r;
}


// return the number of 10xxxxxx bytes in <s>
extern int Utf8ExtraBytes(std::string s)
{
	int i = s.size();
	int r = 0;	// for the result
	while (--i >= 0) if ((s[i] & 0xC0) == 0x80) r += 1;
	return r;
}

// return the amount by which the length of the UTF8 form of <w> is more 
//		than the length of <w>
// counts one for a surrogate code point, on the assumption that they will 
//		occur in pairs and a pair maps onto four bytes in the UTF8
extern int Utf8ExtraBytes(CStringW w)
{
	int i = w.GetLength();
	int r = 0;	// for the result
	uint16_t c;
	while (--i >= 0) {
		c = w[i];
		if (c < 0x80) continue;
			// here if needs more than one byte
		r += (c < 0x800 || (c & 0xFC00) == 0xD800) ? 1 : 2;
	}
	return r;
}
