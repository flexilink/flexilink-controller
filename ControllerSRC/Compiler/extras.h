
// extras.h

#pragma once

// convert between UTF8 and Windows "wide character" string
extern CStringW Utf8ToUnicode(std::string s);
extern std::string UnicodeToUtf8(CStringW w);
extern std::string UnicodeToUtf8(uint16_t c);
extern int Utf8ExtraBytes(std::string s);
extern int Utf8ExtraBytes(CStringW w);
