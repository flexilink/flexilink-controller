// Implementation file for the MgtSocket class
// Copyright (c) 2014-2015 Nine Tiles

#include "stdafx.h"
#include "Controller.h"
#include "AnalyserDoc.h"
#include "ControllerDoc.h"
#include "CrosspointDoc.h"
#include "PcodeChange.h"
#include "SHA3.h"
#include <sys/timeb.h>
#include "../Compiler/extras.h"



// return the length of the tag + length fields and of the value, given <b> 
//		pointing to the tag; there are <len> bytes available in the message
ValueSizes ParseLength(uint8_t * b, int len)
{
	ValueSizes v;
	if (len < 2) goto error;
	v.len = b[1];
	if (v.len < 0x80) {
			// "length" field is the 7-bit form
		if (len < v.len + 2) goto error;
		v.hd_len = 2;	// length of tag + length
		return v;
	}

	switch (v.len) {
			// the only others we support are the 8-bit and 16-bit forms
case 0x81:
		if (len < 3) break;
		v.len = b[2];
		if (len < v.len + 3) goto error;
		v.hd_len = 3;
		return v;

case 0x82:
		if (len < 4) break;
		v.len = (b[2] << 8) | b[3];
		if (len < v.len + 4) goto error;
		v.hd_len = 4;
		return v;
	}

error:	// here if can't parse
	v.hd_len = -1;
	v.len = -1;
	return v;
}


// ------------------------ class MibObject

MibObject::MibObject() {
	tag = TAG_INVALID;
		// added so the compiler (which doesn't understand "valid only if 
		//		<tag> is ASN1_TAG_INTEGER") won't whinge
	value = 0;
	recd = 0;
	msg_type = 0;
}


// initialise by reading a VarBind from the buffer described by <p> and <len>, 
//		which are skipped over it
// sets <tag = TAG_INVALID> if any error: if failed to read the OID all arrays 
//		are empty and <b> and <len> unchanged; else they are skipped over the 
//		OID, which is in <oid_ber>, and <oid> is empty if the OID is invalid, 
//		valid if the error was in the value.
MibObject::MibObject(uint8_t * &p, int &len) {
		// get OID as if it was the value, transfer it to the OID, then 
		//		get the value
	if (!GetAsn1Value(p, len)) {
		tag = TAG_INVALID;
		return;
	}
	oid_ber = *this;
	oid = ConvertOid();
	clear();
	if (oid.IsEmpty() || !GetAsn1Value(p, len)) tag = TAG_INVALID;
}


MibObject::~MibObject() {
}


// return value of octet-string object as a character string
// result is a null string if <this> is NULL or tag not OCTET STRING
CString MibObject::StringValue() //const
{
	CString s;	// initially empty
	if (this == NULL || tag != ASN1_TAG_OCTET_STRING) return s;

	int n = GetCount();
	int i = 0;
	while (i < n) { s += (char)at(i); i += 1; }
	return s;
}


// return value of octet-string object in hex
// result is a null string if <this> is NULL or tag not OCTET STRING
// if <group_size > 0>, a space is inserted after every <group_size> bytes 
//		except at the end of the string; else no spaces inserted
CString MibObject::StringHex(int group_size)// const
{
	CString s;	// initially empty
	if (this == NULL || tag != ASN1_TAG_OCTET_STRING) return s;

	int i = 0;	// counts bytes
	int j = 0;	// counts bytes modulo <group_size>
	INT_PTR n = GetCount();
	if (n <= 0) return s;
	while (true) {
		unsigned int b = GetAt(i);
		s.AppendFormat("%02X",b);
		i += 1;
		if (i >= n) return s;
		j += 1;
		if (j != group_size) continue;
		s += ' ';
		j = 0;
	}
}


// copy the value of octet-string object to a byte array
// result is a null string if <this> is NULL or tag not OCTET STRING
// note that whereas you can copy and dereference CString values you can't 
//		do that with CByteArray, and the compiler errors if you try to are 
//		really unhelpful
void MibObject::CopyOctetStringValue(CByteArray& b)
{
	if (this == NULL || tag != ASN1_TAG_OCTET_STRING) {
		b.RemoveAll();
		return;
	}

	int n = GetCount();
	b.SetSize(n);
	int i = 0;
	while (i < n) { b[i] = at(i); i += 1; }
}


// return value of octet-string object as a character string
// result is a null string if <this> is NULL or tag not OCTET STRING
std::string MibObject::StdStringValue()// const
{
	std::string s;	// initially empty
	if (this == NULL || tag != ASN1_TAG_OCTET_STRING) return s;

	int n = GetCount();
	int i = 0;
	while (i < n) { s += (char)at(i); i += 1; }
	return s;
}


// return value of octet-string object in hex
// result is a null string if <this> is NULL or tag not OCTET STRING
// if <group_size > 0>, a space is inserted after every <group_size> bytes 
//		except at the end of the string; else no spaces inserted
std::string MibObject::StdStringHex(int group_size)// const
{
	std::string s;	// initially empty
	if (this == NULL || tag != ASN1_TAG_OCTET_STRING) return s;

	int i = 0;	// counts bytes
	int j = 0;	// counts bytes modulo <group_size>
	int n = (int)GetCount();
	if (n <= 0) return s;
	while (true) {
		unsigned int b = at(i);
		s += ToHex(b, 2);
		i += 1;
		if (i >= n) return s;
		j += 1;
		if (j != group_size) continue;
		s += ' ';
		j = 0;
	}
}


// return value of object as text
std::string MibObject::TextValue()// const
{
	switch (tag) {
case ASN1_TAG_INTEGER:
		return "INT: " + ToDecimal(value);

case ASN1_TAG_OCTET_STRING:
		return "STR: " + StdStringHex(1);

case ASN1_TAG_OID:
		return "OID: " + OidToText(*this);
	}

		// here if tag not recognised
	return "TAG " + ToHex(tag, 2) + ": " +  StdStringHex(1);
}



// set <tag> and the byte array from an ASN.1 value in a message
// <p> points to the tag, <len> is the number of bytes left in the message
// updates <p> and <len> (to skip over the object) and returns <true> if OK, 
//		else returns <false> without changing anything
// if the tag is ASN1_TAG_INTEGER, and the length is in the range 1 to 4 
//		inclusive, we set <value> accordingly; if the length is out of range 
//		we set <tag> to TAG_INTEGER_OUT_OF_RANGE
// does not read beyond <p[len-1]> in case that would cause a protection fault
bool MibObject::GetAsn1Value(uint8_t * &p, int &len)
{
	ValueSizes sz = ParseLength(p, len);
	if (sz.hd_len < 0) return false;

	uint8_t * b = p;
	tag = b[0];
	b += sz.hd_len;
	resize(sz.len);
	int i = 0;
	while (i < sz.len) { at(i) = b[i]; i += 1; }

	if (tag == ASN1_TAG_INTEGER) switch (sz.len) {
case 1:
		value = (char)(b[0]); // cast to signed so will sign extend
		break;

case 2:
		value = ((char)(b[0]) << 8) | b[1];
		break;

case 3:
		value = ((char)(b[0]) << 16) | (b[1] << 8) | b[2];
		break;

case 4:
		value = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
		break;

default:
		tag = TAG_INTEGER_OUT_OF_RANGE;
	}

	p = b + sz.len;
	len -= (sz.hd_len + sz.len);
	return true;
}


// return the value in dotted-decimal form if <tag> is ASN1_TAG_OID, the 
//		byte array is nonempty, and the first and last bytes have their top 
//		bits clear; else return an empty string
// also returns an empty string if <this> is NULL
// the first octet is unscrambled, e.g. 0x2B is rendered as "1.3", not "43"
// any arc values that don't fit in 64 bits are rendered as the residue 
//		modulo 2**64
CString MibObject::ConvertOid()
{
	CString s;
	if (this == NULL || tag != ASN1_TAG_OID) return s;
	int top = GetUpperBound();
	if (top < 0) return s;
//	char * b = (char *)GetData(); // signed so d7 is sign bit
	int j = at(0);
	if ((j & 0x80) || (at(top) & 0x80)) return s;

		// here if OK
	int i = j / 40;
	s.Format("%d.%d", i, j - (i * 40));
	i = 1;
	while (i <= top) {
			// add next arc to <s>
			// note that we know the last byte has d7=0 so will be the end 
			//		of an arc
		__int64 n = 0;
		do {
			j = at(i);
			i += 1;
			n = (n << 7) | (j & 0x7F); } while (j & 0x80);
		s.AppendFormat(".%I64d", n);
	}
	return s;
}

/*
// append the value to <b> in the format for an index in an OID; returns 
//		<true> if OK, <false> with <b> unchanged else
// currently only copes with octet strings and positive 32-bit integers
bool  MibObject::AppendAsIndexTo(CByteArray & b) {
	int i, n;
	uint8_t v;
	switch (tag) {
case ASN1_TAG_INTEGER:
		if (value < 0) return false;
			// set <n> to 7 * ((bytes to add) - 1)
		n = 0;
		while (value > (128 << n) && n < 28) n += 7;
		while (n > 0) { b.Add((value >> n) | 0x80); n -= 7; }
		b.Add(value & 0x7F);
		return true;

case ASN1_TAG_OCTET_STRING:
		n = (int)GetCount();
		if (n > 0x3FF) return false; // length is silly
		if (n > 0x7F) b.Add((n >> 7) | 0x80);
		b.Add(n & 0x7F);
		i = 0;
		while (i < n) {
			v = GetAt(i);
			if (v & 0x80) b.Add(0x81);
			b.Add(v & 0x7F);
			i += 1;
		}
		return true;
	}

	return false;
}
*/

// append the value to <b> in the format for an index in an OID; returns 
//		<true> if OK, <false> with <b> unchanged else
// currently only copes with octet strings and positive 32-bit integers
bool MibObject::AppendAsIndexTo(ByteString & b) {
	int i, n;
	uint8_t v;
	switch (tag) {
case ASN1_TAG_INTEGER:
		i = IntegerValue();
		if (i < 0) return false;
			// set <n> to 7 * ((bytes to add) - 1)
		n = 0;
		while (i >= (128 << n) && n < 28) n += 7;
		while (n > 0) { b.push_back((i >> n) | 0x80); n -= 7; }
		b.push_back(i & 0x7F);
		return true;

case ASN1_TAG_OCTET_STRING:
		n = (int)size();
		if (n > 0x3FF) return false; // length is silly
		if (n > 0x7F) b.push_back((n >> 7) | 0x80);
		b.push_back(n & 0x7F);
		i = 0;
		while (i < n) {
			v = at(i);
			if (v & 0x80) b.push_back(0x81);
			b.push_back(v & 0x7F);
			i += 1;
		}
		return true;
	}

	return false;
}


// copy the OID into a message (including tag and length)
// length uses the shortest form it can
// assumes there is enough room (and that the length is less than 64KB)
// returns the number of bytes written
int MibObject::CopyOid(uint8_t * p)
{
	*p++ = ASN1_TAG_OID;
	int n = (int)oid_ber.size();
	int len = n + 2;
	if (n < 128) *p++ = (uint8_t)n;
	else if (n < 256) { *p++ = 0x81; *p++ = (uint8_t)n; len += 1; }
	else { *p++ = 0x82; *p++ = (uint8_t)(n >> 8); *p++ = (uint8_t)n; len += 2; }

	int i = 0;
	while (i < n) *p++ = oid_ber.at(i++);
	return len;
}


// return whether <m> and <this> have different values
// if <this->oid> indicates unitUpTime, we compare the implied reset times, 
//		not the values
bool MibObject::ChangedFrom(MibObject * m) {
	if (m->tag != tag) return true;

	if (tag == ASN1_TAG_INTEGER) {
		if (oid != "1.0.62379.1.1.1.9") return m->value != value;

			// else is unitUpTime; compare the implied reset times
			// we allow up to 2 secs for rounding errors etc; this means if a 
			//		unit is reset twice within about 2 secs we only see one 
			//		of the resets, but it's unlikely we'll have got far with 
			//		collecting MIB etc information between the two anyway
		return _abs64(recd + value - (m->recd + m->value)) > 2;
	}

	int n = m->GetCount();
	if (n != GetCount()) return true;
	int i = 0;
	while (i < n) { if (m->at(i) != at(i)) return true; i += 1; }
	return false;
}



// --------------------------- class VersionNumber


VersionNumber::VersionNumber()
{
	valid = false;
		// added so the compiler (which doesn't understand what <valid> 
		//		means) won't whinge
	vn[0] = 0;
	vn[1] = 0;
	vn[2] = 0;
	vn[3] = 0;
}


// fill in from ASCII characters in an octet string formatted as "n.n.n[ BETA n]"
// sets invalid if string not in right format; returns whether valid
bool VersionNumber::FromOctetString(ByteString& b)
{
	std::string s;
	size_t n = b.size();
	if (n < 5) { valid = false; return false; }	// shortest valid is "0.0.0"
	s.resize(n);
	while (n > 0) {
		n -= 1;
		s.at(n) = b.at(n);
	}
	int v[4];
	n = (sscanf_s(s.c_str(), "%d.%d.%d BETA %d", &v[0], &v[1], &v[2], &v[3]));
	if (n == 3) v[3] = 0;
	else if (n != 4) { valid = false; return false; }

	n = 0;
	while (true) {
		if (v[n] < 0 || v[n] > 255) { valid = false; return false; }
		vn[n] = (uint8_t)(v[n]);
		if (n == 3) { valid = true; return true; }
		n += 1;
	}
}


// fill in from ASCII characters in a ByteString formatted as "n-n-n[-BETA-n]"
// sets invalid if string not in right format; returns whether valid
bool VersionNumber::FromFilename(ByteString& b)
{
	std::string s;
	size_t n = b.size();
	if (n < 5) { valid = false; return false; }	// shortest valid is "0.0.0"
	s.resize(n);
	while (n > 0) {
		n -= 1;
		s.at(n) = b.at(n);
	}
	int v[4];
	n = sscanf_s(s.c_str(), "%d-%d-%d-BETA-%d", &v[0], &v[1], &v[2], &v[3]);
	if (n == 3) v[3] = 0;
	else if (n != 4) { valid = false; return false; }

	n = 0;
	while (true) {
		if (v[n] < 0 || v[n] > 255) { valid = false; return false; }
		vn[n] = (uint8_t)(v[n]);
		if (n == 3) { valid = true; return true; }
		n += 1;
	}
}


// fill in from ASCII characters in a CString formatted as "n-n-n[-BETA-n]"
// sets invalid if string not in right format; returns whether valid
bool VersionNumber::FromFilename(CString s)
{
	int v[4];
	int n = sscanf_s(s, "%d-%d-%d-BETA-%d", &v[0], &v[1], &v[2], &v[3]);
	if (n == 3) v[3] = 0;
	else if (n != 4) { valid = false; return false; }

	n = 0;
	while (true) {
		if (v[n] < 0 || v[n] > 255) { valid = false; return false; }
		vn[n] = (uint8_t)(v[n]);
		if (n == 3) { valid = true; return true; }
		n += 1;
	}
}


// return the version number formatted as "n.n.n[ BETA n]", or an empty 
//		string if not valid
std::string VersionNumber::ToText()
{
	std::string s;
	unless (valid) return s;
	char buf[12];	// long enough for "255.255.255" incl NUL at end
	sprintf_s(buf, 12, "%d.%d.%d", (int)(vn[0]), (int)(vn[1]), (int)(vn[2]));
	s = buf;
	if (vn[3] == 0) return s;

	sprintf_s(buf, 12, " BETA %d", (int)(vn[3]));
	s += buf;
	return s;
}


// return the version number as ASCII characters in an octet string 
//		formatted as "n.n.n[ BETA n]", or an empty string if not valid
ByteString VersionNumber::ToOctetString()
{
	ByteString b;
	unless (valid) return b;
	char buf[12];	// long enough for "255.255.255" incl NUL at end
	sprintf_s(buf, 12, "%d.%d.%d", (int)(vn[0]), (int)(vn[1]), (int)(vn[2]));
	int n = 0;
	until (buf[n] == 0) { b.push_back(buf[n]); n += 1; }
	if (vn[3] == 0) return b;

	sprintf_s(buf, 12, " BETA %d", (int)(vn[3]));
	until (buf[n] == 0) { b.push_back(buf[n]); n += 1; }
	return b;
}


// return the version number formatted as "n-n-n[-BETA-n]", or an empty 
//		string if not valid
CString VersionNumber::ToFilename()
{
	CString s;
	unless (valid) return s;
	char buf[12];	// long enough for "255.255.255" incl NUL at end
	sprintf_s(buf, 12, "%d-%d-%d", (int)(vn[0]), (int)(vn[1]), (int)(vn[2]));
	s = buf;
	if (vn[3] == 0) return s;

	sprintf_s(buf, 12, "-BETA-%d", (int)(vn[3]));
	s += buf;
	return s;
}


bool VersionNumber::operator== (VersionNumber v)
{
	if (!valid) return !v.valid;

	int i = 3;
	do if (vn[i] != v.vn[i]) return false; while (--i >= 0);
	return true;
}


bool VersionNumber::operator!= (VersionNumber v)
{
	if (!valid) return v.valid;

	int i = 3;
	do if (vn[i] != v.vn[i]) return true; while (--i >= 0);
	return false;
}


// --------------------------- class LinkSocket

LinkSocket::LinkSocket()
{
	standard_format = false;
	memset(our_ident, 0, 8);
	state = LINK_ST_BEGIN;
	rcv_timer_count = LINK_RCV_TIMEOUT;
	tx_timer_count = 1;
}


LinkSocket::~LinkSocket()
{
	if (!link_ip_addr.IsEmpty()) {
			// send a Link Reject message
		ByteString m(aes51_data_hdr);
		m[1] = 0x82;
		Send(m.data(), 6);
	}
}


// Send the Link Request message: must be called exactly once, as the 
//		first call after creation of the socket.
// returns whether successful
BOOL LinkSocket::Init()
{
		// Note that if <sv> is NULL the datagram will be broadcast
		// apparently <server.IsEmpty() ? NULL : server> is an empty string, 
		//		not NULL, when <server> is empty; maybe the NULL gets coerced 
		//		to a CString?
		// we don't set <link_ip_addr> at this stage even if we have a server 
		//		address; we can't do <Connect> until we have the port number, 
		//		so use the same code in <OnReceive> as in the broadcast case
	int err; // to hold result of GetLastError() for debug
	LPCTSTR sv;
	if (theApp.server_addr.IsEmpty()) {
			// ask for the socket to be allowed to send broadcasts
		sv = NULL;
		BOOL opt_buf = TRUE;
		if (!SetSockOpt(SO_BROADCAST, & opt_buf, sizeof(BOOL))) {
			err = GetLastError(); // for debug
			theApp.controller_doc->failure_notice = "Failed to set broadcast, code ";
			theApp.controller_doc->failure_notice += ToDecimal(err).c_str();
			theApp.controller_doc->failure_notice += ' ';
			theApp.controller_doc->failure_notice += strerror(err);
			state = LINK_ST_FAILED;
			return FALSE;
		}
	}
	else sv = theApp.server_addr;

	ByteString m(aes51_data_hdr);	// accumulates the Link Request message
	m[1]  = 0x80;	// Link Request
	m[6]  = 0x85;	// IE selecting LinkTypeExternal
	m[7]  = 4;
	m[8]  = 0x11;	// standard value; legacy units ignore d7-4
	m[9]  = 0;
	m.resize(12);
	m[10] = 0;
	m[11] = 0;
	if (SendTo(m.data(), 12, AES51_PORT, sv) != 12) {
		err = GetLastError(); // for debug
// 10051 = 0x2743 = socket operation was attempted to an unreachable network.
		theApp.controller_doc->failure_notice = "Failed to send link request, code ";
		theApp.controller_doc->failure_notice += ToDecimal(err).c_str();
		theApp.controller_doc->failure_notice += ' ';
		theApp.controller_doc->failure_notice += strerror(err);
		state = LINK_ST_FAILED;
		return FALSE;
	}

	state = LINK_ST_REQ;
	return TRUE;
}


// send message from <b>, total size <len>, with flow label (including 
//		CRC) <flow>
// if <flow> is all-zero or omitted, <tx_sig_flow> is used
// adds the AES51 header and IT header into the front of a copy
// returns whether the message was sucessfully sent
bool LinkSocket::TxMessage(uint8_t * b, int len, int flow) {
	if (state != LINK_ST_ACTIVE || flow < 0) {
		return false;
	}
	ByteString msg(aes51_data_hdr);
	if (flow) {
		msg[8] = (uint8_t)(flow >> 8);
		msg[9] = (uint8_t)flow;
	}
	flow = AddHec(len + (standard_format ? -1 : 3));	// packet length
	msg[6] = (uint8_t)(flow >> 8);
	msg[7] = (uint8_t)flow;
	msg.resize(len + 10);
	uint8_t * b2 = msg.data(); // NB after resizing in case it moves
	memcpy(b2 + 10, b, len);
	len += 10;
	if (Send(b2, len) == len) return true;
	return false;
}


// process an incoming message
// for type 0x26, the label values are: 0 signalling 1-255 management socket 
//		256- analyser
void LinkSocket::OnReceive(int nErrorCode)
{
// NOTE: we're told to call the base class function, but currently it's null 
//		and it's difficult to imagine what it might do except maybe consume 
//		the message so I think it's safer not to.
//	CAsyncSocket::OnReceive(nErrorCode);

		// read the message
		// I've added a few more bytes to the length of the buffer because the 
		//		compiler has noticed that if the message ends in the middle of 
		//		an IE the code that collects info from a ClearDown request 
		//		could be reading beyond the end of the buffer; now it'll be 
		//		reading uninitialised bytes within the buffer, which is maybe 
		//		no better
	uint8_t b[MAX_REPLY_LENGTH + 16];
	CString remote_address;
	UINT remote_port;
	int len = ReceiveFrom(b, MAX_REPLY_LENGTH, remote_address, remote_port);
	FlexilinkSocket * f_skt;
	int err;
/*	if (theApp.controller_doc->failure_notice.IsEmpty()) {
		err = GetLastError();
		theApp.controller_doc->failure_notice = "Packet received, last error code ";
		theApp.controller_doc->failure_notice += ToDecimal(err).c_str();
		theApp.controller_doc->failure_notice += ' ';
		theApp.controller_doc->failure_notice += strerror(err);
	}
*/
	switch (len) {
case 0:	return;		// length zero, or socket has been closed

case SOCKET_ERROR:	// error
		err = GetLastError();
		if (err != WSAEWOULDBLOCK && state != LINK_ST_FAILED) {
			state = LINK_ST_FAILED;
			theApp.controller_doc->failure_notice = "Socket error, code ";
			theApp.controller_doc->failure_notice += ToDecimal(err).c_str();
			theApp.controller_doc->failure_notice += ' ';
			theApp.controller_doc->failure_notice += strerror(err);
			theApp.controller_doc->UpdateDisplay();
		}
		return;
	}

		// here if OK
	if (remote_port != AES51_PORT) return;
	if (len >= MAX_REPLY_LENGTH) return; // overlength UDP datagram
	if (len < 6 || b[0] != 2) return;	// not a valid message

	if (state == LINK_ST_REQ) {
			// collect the address for use with subsequent packets
		if (!Connect(remote_address, AES51_PORT)) {
			state = MGT_ST_FAILED;
			err = GetLastError();
			theApp.controller_doc->failure_notice = 
								"Failed to connect socket, error code ";
			theApp.controller_doc->failure_notice += ToDecimal(err).c_str();
			theApp.controller_doc->failure_notice += ' ';
			theApp.controller_doc->failure_notice += strerror(err);
			theApp.controller_doc->UpdateDisplay();
			return;
		}
		link_ip_addr = remote_address;
	}

		// local variables used inside the switch (NB C doesn't let 
		//		you declare them there)
		// I've declared <label> as signed because else the compiler 
		//		doesn't know which instruction to use when comparing 
		//		it with signed variables; <b> is unsigned so the value 
		//		in <label> (which is 32 bits) will always be in the 
		//		range 0 to 65535 and it's safe to cast it to unsigned 
		//		if required
	int label, cause;
	int i, j;
	ByteString m;	// for reply messages
	ByteString id;	// for link partner's id in Link Accept
	MgtSocket * m_skt;
	switch (b[1]) {
case 0x26:		// IT packet
		if (b[6] & 0xC0) return;	// ignore if not normal data
			// +++ maybe should allow a preamble? spec doesn't say
		label = (b[6] << 8) | b[7];	// "length" field
			// set <i> to length including AES51 header & IT header
		i = (label >> 3) + (standard_format ? 11 : 7);
		if (len != i) {
				// length in IT header is different from UDP length
			if (len < i) return;	// IT packet truncated
			if (label != AddHec(label >> 3)) return;	// CRC error
			len = i;	// remove padding
		}
		label = (b[8] << 8) | b[9];

		if (label == RCV_SIG_FLOW) switch (b[10]) {	// signalling message
	case 9:			// ClearDown request: collect info from IEs
			i = 12 + b[11];	// first byte of variable part
			label = -1; // call ref; initialise to "not found"
			cause = -1; // initialise to "not found"
			while (i < len) {
					// <i> points to the next IE
				switch (b[i] & 0x7F) {
		case 23:		// Cause (assume IEC 62379-5-2 format)
					j = i + ((b[i] & 0x80) ? 4 : 3);
					cause = (b[j] << 8) | b[j+1]; // 1st 2 bytes of cause
					break;
		case 24:		// Route
					j = i + ((b[i] & 0x80) ? 4 : 3);
					if (memcmp(b+j, our_ident, 8) != 0) break;	// wrong owner
					label = (b[j+10] << 8) | b[j+11];	// ls 2 bytes of call ref
				}
				i += (b[i+1] << 8) + b[i+2] + 3;
			}

			if (b[14] || b[13] || b[12]) {
					// send ack
				b[10] = 0x89;
				b[11] = 3;
				Send(b, 15);
			}

					// mark the socket as closed
					// <theApp.FindSocket> returns NULL if not found, 
					//		including whenever <label> is negative
					// +++ a previous version deleted it; now that 
					//		we don't do that it would be better to 
					//		process it in the <f_skt> object but we 
					//		still wouldn't be able to share code 
					//		with the FindRoute case because we want 
					//		different action when the label's not 
					//		recognised
			f_skt = theApp.FindSocket(label);
			if (f_skt == NULL) return;
			f_skt->SaveMessage(b + 10, len - 10, 'R');
			f_skt->state = (f_skt->state < MGT_ST_CONN_MADE) ?
									MGT_ST_NOT_CONN : MGT_ST_CLOSED;
			if (cause != 0x0218) return;
				// here if cause is Q.850 code for "call rejected due 
				//		to a feature at the destination" which we will 
				//		use for "wrong password" though the current 
				//		Aubergine software never includes a Cause IE
			m_skt = dynamic_cast<MgtSocket *>(f_skt);
			if (m_skt == NULL) return;
			m_skt->password_state = PW_NOT_SET; // delete the password
	default:		// anything unrecognised: silently ignore
			return;

	case 0x88:		// ack FindRoute request
	case 0x28:		// FindRoute response
			if (b[11] != 13 || memcmp(b+12, our_ident, 8) != 0) return;
			f_skt = theApp.FindSocket((b[22] << 8) | b[23]);
			if (f_skt) {
				f_skt->ReceiveSignalling(b + 10, len - 10);
				return;
			}
		} // end of code for signalling messages
		else {
				// <label> is the flow label, and is not the signalling flow
			i = label >> 3;
			if (label != AddHec(i)) return;	// ignore (bad CRC)
			f_skt = theApp.FindSocket(i);
			if (f_skt) {
				f_skt->ReceiveData(b + 10, len - 10);
				return;
			}
		}

			// here if the flow label isn't recognised for data or FindRoute
			// +++ ought to send a ClearDown but don't have a call id; need 
			//		to add option to GS NIN 005 for Terminate request to 
			//		include a BasicAlloc IE
		return;


case 0x81:	// Link Accept
		unless (state == LINK_ST_REQ) return;
			// we only offered one protocol, so don't need to check 
			//		which one the link partner is proposing
		state = LINK_ST_ACTIVE;
			// log that we can connect (stays true if link lost)
		theApp.pre_connection = false;
			// we've already collected the link partner's IP address
			// collect the signalling flow label and our ident
		label = 0;	// default if no type 83 IE
		i = 6;
		while (i < len) {
			if (b[i] == 0x83) {
					// have the signalling flow
				if (b[i+1] < 2) {
					if (b[i+1] == 1) label = b[i+2];
				}
				else label = (b[i+2] << 8) | b[i+3];
			}
			else if (b[i] == 0x84 && b[i+1] >= 8) {
					// have our 64-bit ident
				memcpy(our_ident, b+i+2, 8);
			}
			else if (b[i] == 0x82 && b[i+1] >= 8) {
					// have the link partner's 64-bit ident
				id.clear();
				id.push_back(5);	// <FlAddrTypeUnitId>
				int n = 0;
				do {
					id.push_back(b[i+2+n]);
					n += 1;
				} while (n < 8);
			}
			else if (b[i] == 0x85 && b[i+1] > 0 && b[i+2] == 0x11) {
					// have a LinkType IE specifying a virtual link 
					//		with 1 ms allocation period as specified in 
					//		GS NIN 005; assume it's using the standard 
					//		format for packet headers
				standard_format = true;
			}
			i += (b[i+1] + 2);
		}

		label = AddHec(label);
		aes51_data_hdr[8] = (uint8_t)(label >> 8);
		aes51_data_hdr[9] = (uint8_t)label;

		theApp.NewLinkPartner(id);
		return;


case 0x82:	// Link Reject
		state = LINK_ST_CLOSED;
			// nothing else to do; we leave it up to the 
			//		<CControllerDoc> object to notice the change
			// we don't close it so that the user can see the state
		return;


case 0x84:	// Link Keepalive
		rcv_timer_count = LINK_RCV_TIMEOUT;
			// KLUDGE ALERT: we seem to stop sending keepalives when uploading 
			//		software; I don't understand why that should be, but sending 
			//		one back each time we receive one should be OK provided the 
			//		other end doesn't do the same; receiving clearly doesn't 
			//		stop because the acks to the writes are being seen
		m = aes51_data_hdr;
		m[1] = 0x84;
		Send(m.data(), 6);
		tx_timer_count = LINK_KEEPALIVE_PERIOD;
		return;
	}

		// here if packet type not recognised
		// arguably we should just ignore the packet, though it must be 
		//		implementing something we haven't said we support
	state = LINK_ST_ERROR;
		// send a Link Reject message
	m = aes51_data_hdr;
	m[1] = 0x82;
	Send(m.data(), 6);
}


// routine called every 0.5 sec; returns whether the link has timed out
bool LinkSocket::PollKeepalives()
{
	rcv_timer_count -= 1;
	if (rcv_timer_count <= 0) {
			// treat as Link Reject (see above)
		state = LINK_ST_CLOSED;
		return true;
	}

	tx_timer_count -= 1;
	if (tx_timer_count > 0) return false;
		// else send a Link Keepalive message
	ByteString m(aes51_data_hdr);
	m[1] = 0x84;
	Send(m.data(), 6);
	tx_timer_count = LINK_KEEPALIVE_PERIOD;
	return false;
}


// ----------------- MgtSocket and FlexilinkSocket classes ----------

// NOTE: the constructor and various routines that were originally part of 
//		the ControllerDoc class are in ControllerDoc.cpp

// Initialise: must be called exactly once, immediately after creating the 
//		object
// Note that, unlike in the version that used AES51 type 0x24, <call_addr> 
//		must not be empty
/*void MgtSocket::Init(ByteString call_addr)
{
	unit_TAddress = call_addr;
	unit_address = ByteArrayToHex(unit_TAddress);
	theApp.unit_addrs.SetAt(unit_address, (void *)this);

	SendConnReq();
}*/



// Send the FindRoute request message; intended to be called on 
//		initialisation and also whenever the link is reconnected
// +++ NOTE: we assume any existing flow has been cleared down; 
//		if not, the <call_ref> and <tx_flow> for the previous flow 
//		will be forgotten but the managed unit will continue to send 
//		status broadcasts etc on it
void MgtSocket::SendConnReq()
{
	password_state = PW_NOT_USED; // in case reconnecting
	call_ref += 0x10000;	// new call reference
	call_ref &= 0x7FFFFFFF; // in case has wrapped

	ByteString::size_type n = unit_TAddress.size();
	mgt_msg.count = 0;
	mgt_msg.m.resize(62+n);
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
		// DataType IE: 1.3.6.1.4.1.9940.2.2.4
	mgt_msg.m[15] = 5;
	mgt_msg.m[16] = 0;
	mgt_msg.m[17] = 10;
	mgt_msg.m[18] = 43;
	mgt_msg.m[19] = 6;
	mgt_msg.m[20] = 1;
	mgt_msg.m[21] = 4;
	mgt_msg.m[22] = 1;
	mgt_msg.m[23] = 0xCD;
	mgt_msg.m[24] = 0x54;
	mgt_msg.m[25] = 2;
	mgt_msg.m[26] = 2;
	mgt_msg.m[27] = 4;
		// PrivilegeLevel IE
	mgt_msg.m[28] = 12;
	mgt_msg.m[29] = 0;
	mgt_msg.m[30] = 1;
	mgt_msg.m[31] = theApp.privilege;
		// AsyncAlloc IE
	int rcv_label = AddHec(call_ref & 0x1FFF);
	mgt_msg.m[32] = 20;
	mgt_msg.m[33] = 0;
	mgt_msg.m[34] = 2;
	mgt_msg.m[35] = (uint8_t)(rcv_label >> 8);
	mgt_msg.m[36] = (uint8_t)rcv_label;
		// FlowDescriptor IE
	mgt_msg.m[37] = 4;
	mgt_msg.m[38] = 0;
	mgt_msg.m[39] = 4;
	mgt_msg.m[40] = 2;
	mgt_msg.m[41] = 0;
	mgt_msg.m[42] = 0;
	mgt_msg.m[43] = 1;

	unsigned int j;
	if (ExpectPassword()) {
			// Password IE
		mgt_msg.m[44] = 13;
		mgt_msg.m[45] = 0;
		mgt_msg.m[46] = 12;

			// create the second half of the "random string" with count = 0
			// don't check for success in collecting time or random number; 
			//		whatever is left in <tb> ot <j> will be "random" enough
		struct __timeb64 tb;
		_ftime64_s(&tb);
		rand_s(&j);
		password_string[6] = tb.time;
		password_string[7] = (((uint64_t) tb.millitm) << 54) | 
										(((uint64_t)(j & 0x3FFFFF)) << 32);
		uint8_t b[16];
		CopyLongwordsToNetwork(b, password_string + 6, 2);
		j = 47;
		i = 0;
		do mgt_msg.m[j++] = b[i++]; while (i < 12);
	}
	else {
			// don't include Password IE
		mgt_msg.m.resize(47+n);
		j = 44;
	}

		// CalledAddress IE; <j> is the index to the tag byte
	mgt_msg.m[j++] = 3;
	mgt_msg.m[j++] = 0;
	mgt_msg.m[j++] = (uint8_t)n;	// assumed < 256

	i = 0;
	while (i < n) mgt_msg.m[j++] = unit_TAddress[i++];

	if (theApp.link_socket->TxMessage(mgt_msg.m, 0)) {
		SaveMessage(mgt_msg.m.data(), mgt_msg.m.size(), 'T');
		state = MGT_ST_CONN_REQ;
		return;
	}

	SaveMessage(mgt_msg.m.data(), mgt_msg.m.size(),'E');
	state = MGT_ST_FAILED;
}


// send message from <b>, total size (including header) <len>, on flow 
//		<tx_flow> (default zero, which is the signalling flow)
// flag in the saved message is upper case for signalling, lower case else
// saves a copy in hex if maintenance level; note that at other levels there 
//		is no point in saving a copy because there is nowhere to display it
// if transmission is unsuccessful, enters "failed" state
void FlexilinkSocket::TxMessage(uint8_t * b, int len) {
	bool ok = theApp.link_socket->TxMessage(b, len, tx_flow);
	if (theApp.privilege == PRIV_MAINTENANCE) 
		SaveMessage(b, len, tx_flow ? (ok ? 't' : 'e') : (ok ? 'T' : 'E'));
	unless (ok) {
		SetStateFailed();
	}
}


// send a management message from <b>, total size (including header) <len>
// fills in the serial number in <b[1]>; all else except the password hash (if 
//		required) must be already there; doesn't make any other changes to the 
//		buffer to which <b> points
// <pw> defaults to true, in which case the message must be a management request 
//		(without the hash); the user will be asked for the password if not 
//		already set and if there is no random string a type 5 request will be 
//		sent instead; otherwise the hash is calculated and inserted after the 
//		second byte and the random string is invalidated
// <flash_request> defaults to false; if it is true and transmission is 
//		successful, fills in <upd_reply> and <upd_msg>
// if transmission is unsuccessful, enters "failed" state
void MgtSocket::TxNewMessage(uint8_t * b, int len, bool pw, bool flash_request) {
	b[1] = next_serial;
	if (next_serial >= 255) next_serial = 1;
	else next_serial += 1;
	TxMessage(b, len, pw);

	if (flash_request) {
		upd_reply = (b[0] & 0x70) | 0x80;
		upd_msg_ser = b[1];
		upd_msg.m.resize(len);
		memcpy(upd_msg.m.data(), b, len);
		upd_msg.count = 0;
	}
}

// send a message from <b>, total size (including header) <len>
// <pw> defaults to true, in which case the message must be a management request 
//		(without the hash); the user will be asked for the password if not 
//		already set and if there is no random string (and none on the way) a 
//		type 5 request will be sent instead; otherwise the hash is calculated 
//		and inserted after the second byte and the random string is invalidated
// on return the message might have been transmitted or it might only have been 
//		queued awaiting a new random string, but in the latter case it will be 
//		transmitted without any further action needed on the part of the caller; 
//		in normal circumstances the transmission will happen within a few 
//		milliseconds, but if a message carrying a string is lost there will be 
//		a delay similar to the delay before an unacknowledged message is repeated
// if transmission is unsuccessful, enters "failed" state
void MgtSocket::TxMessage(uint8_t * b, int len, bool pw) {
	if (pw && ExpectPassword()) {
		if (password_state == PW_NOT_SET) GetPassword();
		if (password_state == PW_VALID) {
				// we have the information needed to calculate the hash
			TxWithHash(b, len);
			return;
		}
	}
		// else send without password hash
	FlexilinkSocket::TxMessage(b, len);
}


// add password hash to message and then send it
// caller must check there is a random string available, also that <len> 
//		is at least 2
void MgtSocket::TxWithHash(uint8_t * b, int len) {
		// increment the count; note that if it wraps the unit will see the 
		//		value as being less than the previous one and clear the call 
		//		down; also, there will be carry into the "random" part so the 
		//		hash will be wrong, which would also clear the call down
	password_string[7] += 1;

		// calculate the hash
	sha3_sponge s;
		// initialise to <in> + 01 for SHA-3 hash + 1 0* 1 for padding + 
		//		16 words zero
	ASSERT(SHA3_KECCAK_SPONGE_WORDS > 9);
	memcpy(s, password_string, 64);
		// merge the message in in the same way as in the Tealeaves code
	int i = 0;			// counts bytes in the message
	uint8_t * b3 = b;	// next byte to read
	uint64_t w;			// accumulates words
	w = 0;
	do {
		if ((i & 7) == 7) {
			w |= *b3++;
			s[(i >> 3) & 7] ^= w;
			w = 0;
		}
		else w |= ((uint64_t)(*b3++)) << (56 - (8 * (i & 7)));
	} while (++i < len);
	s[(i >> 3) & 7] ^= w;
		//   1234567890123456 (16 digits, little-endian)
	s[8] = 0x8000000000000006UL;
	memset(&s[9], 0, (SHA3_KECCAK_SPONGE_WORDS - 9) * 8);
	keccakf(s);

		// add it to the message
	ByteString m;
	m.resize(len + 2 + SHA3_HASH_STRING_BYTES);
	uint8_t * b2 = m.data(); // NB after resizing in case it moves
	memcpy(b2, b, 2);
	b2[2] = ASN1_TAG_OCTET_STRING;
	b2[3] = SHA3_HASH_STRING_BYTES;
	b2[4] = (uint8_t)(password_string[7] >> 24);
	b2[5] = (uint8_t)(password_string[7] >> 16);
	b2[6] = (uint8_t)(password_string[7] >> 8);
	b2[7] = (uint8_t)(password_string[7]);
	CopyLongwordsToNetwork(b2 + 8, s, 8);
	if (len > 2) memcpy(b2 + 4 + SHA3_HASH_STRING_BYTES, b + 2, len - 2);

	FlexilinkSocket::TxMessage(b2, len + 2 + SHA3_HASH_STRING_BYTES);
}


// send Status Request message; fill in <mgt_msg> if <req_ack> is true
inline void MgtSocket::RequestStatus(bool req_ack) {
	unsigned char b2[2];
	b2[0]  = 0x20;	// "Status" request
	TxNewMessage(b2, 2, false);
	if (req_ack) {
		mgt_msg.m.resize(2);
		memcpy(mgt_msg.m.data(), b2, 2);
		mgt_msg.count = 0;
	}
}


// read text file with filename <fn>
std::vector<std::string> FlexilinkSocket::ReadFile(std::string fn)
{
	CStdioFile f;
	CString cs;
	std::string s;
	std::vector<std::string> v;
	if (f.Open(fn.c_str(), CFile::modeRead)) {
		while (f.ReadString(cs)) {
			s = cs;
			v.push_back(s);
		}
		f.Close();
	}
	return v;
}


// process an incoming signalling message (always a FindRoute; 
//		ClearDown messages are processed by the LinkSocket object)
// b[0:14] is the header and fixed part, which caller has checked is 
//		this object's management or SCP call; b[15] is the tag of 
//		the first IE if len > 15
// assumes the buffer to which <b> points is MAX_REPLY_LENGTH bytes 
//		and won't be needed again by the caller so can be used to 
//		send an acknowledgement
void FlexilinkSocket::ReceiveSignalling(uint8_t * b, int len)
{
	SaveMessage(b, len, 'R');

	switch (b[0]) {
	case 0x88:	// ack FindRoute request
		if (state == MGT_ST_CONN_REQ) {
			mgt_msg.m.clear();
			state = MGT_ST_CONN_ACK;
		}
		return;

	case 0x28:	// FindRoute response: send ack
		b[0]  = 0xA8;
		SaveMessage(b, 15, (theApp.link_socket->TxMessage(b, 15, 0) ? 
														'T' : 'E'));

		if (state < MGT_ST_CONN_MADE) {
				// collect the tx flow label and the password
			int pw = 0;	// offset to Password IE
			int i = 15;
			while (i < len) {
				uint16_t ie_len = (b[i+1] << 8) | b[i+2];
				if (b[i] == 20 && ie_len >= 2) {
						// AsyncAlloc IE
					tx_flow = (b[i+3] << 8) | b[i+4];
				}
				else if (b[i] == 13) {
						// Password IE (any length but no nested IEs)
					pw = i;
				}
				i += ie_len + 3;
			}

			if (tx_flow < 0) {
				state = MGT_ST_NOT_CONN;
				return;
			}
			state = MGT_ST_CONN_MADE;
				// NB if response is repeated we won't come here again
			ConnectionMade(pw == 0 ? NULL : b + pw);
		}
	}
}


void MgtSocket::ConnectionMade(uint8_t * b)
{
	ASSERT(password_state == PW_NOT_USED);
	if (b && ExpectPassword()) {
			// collect words 4 and 5 of the password string
		uint16_t ie_len = (b[1] << 8) | b[2];
		if (ie_len == 16) {
			BytesTo64Bit(b + 3, &password_string[4], 2);
			password_state = PW_NOT_SET;
		}
	}
		// send a "GetNext" request for up to 10 objects
		// +++ in the Aubergine, the next object after unitUpTime(9) is 
		//		the new "unit id" at (16); other units might implement 
		//		more of 4.2.4 (possibly including the "power source" 
		//		table) in which case we'd need to send out a separate 
		//		Get request for the unit id; maybe it'd be better to ask 
		//		for that first, anyway
	mgt_msg.m.resize(11);
	mgt_msg.m[0]  = 0x19;
	mgt_msg.m[2]  = 6;		// OID tag
	mgt_msg.m[3]  = 7;		// length of OID
	mgt_msg.m[4]  = 0x28;	// OID = 1.0.62379.1.1.1
	mgt_msg.m[5]  = 0x83;
	mgt_msg.m[6]  = 0xE7;
	mgt_msg.m[7]  = 0x2B;
	mgt_msg.m[8]  = 1;
	mgt_msg.m[9]  = 1;
	mgt_msg.m[10] = 1;
	TxMessage(mgt_msg.m, false);
	mgt_msg.count = 0;
	upd_state = UPD_ST_NO_INFO; // in case reconnecting
}


// process an incoming data message
// b[0] is the command byte
// assumes the buffer to which <b> points is MAX_REPLY_LENGTH bytes 
//		and can be used to send a reply
void MgtSocket::ReceiveData(uint8_t * b, int len)
{
	SaveMessage(b, len, 'r');

	MibObject * m = NULL;
	uint8_t * p;
	CString rel_oid;
	CString s;
	int i,j,k;
	bool call_id_error = false; // KLUDGE

	len -= 2;				// length of VarBinds
	if (len < 0) return;	// if message is too short
	p = b + 2;				// start of first VarBind
	keep_alive_count = 0;	// note that we've seen a message

		// throughout this code, <m> points to a <MibObject> created 
		//		from the VarBind being processed, <p> points to the 
		//		[OID tag of the] VarBind after it, and <len> shows 
		//		the number of bytes remaining in the message (i.e. 
		//		starting from <p>); if we haven't started processing 
		//		the first VarBind, <m> is NULL and <p> points to the 
		//		first VarBind
	if (b[0] & 0x80) {
			// reply
		if ((b[0] & 0x0F) && b[0] != 0xAF) {
				// response reports failure
			s.Format(": error code %d from ", b[0] & 15);
			s += MgtReqCode((b[0] >> 4) & 7);
			m = new MibObject(p, len);
			unless (m->oid.IsEmpty()) {
				if (b[0] == 0x82 && 
							m->oid == "1.0.62379.5.1.1.3.2.0") {
						// NoSuchName response to Get unitNextCallId
					s = " refused request for call id";
					call_id_error = true;
				}
				else {
					s += " for ";
					s += m->oid;
					unless (m->empty()) {
						s += " = ";
						s += Utf8ToUnicode(m->TextValue());
					}
				}
			}
			delete m;
			m = NULL;
			theApp.err_msgs.Add(DisplayName() + s);
			theApp.output_list->UpdateAllViews(NULL);
		}

			// see if it's one we're waiting for
		if (state > MGT_ST_CONN_REQ && 
					mgt_msg.m.size() > 1 && b[1] == mgt_msg.m[1] && 
								((mgt_msg.m[0] ^ b[0]) & 0x70) == 0) {
			mgt_msg.m.clear();
			if (state == MGT_ST_CONN_MADE) {
					// have the response to the initial GetNext; send 
					//		the status broadcast request
				RequestStatus(true);
				state = MGT_ST_HAVE_INFO;
				UpdateDisplay();
			}
		}

			// check for replies to messages involved in setting a 
			//		flow up; the Get for the call id is processed  
			//		further down this code, so here we just look for 
			//		replies to the Set requests in the dests table; 
			//		we assume all entries in a <ConnReqInfo> are for 
			//		the same message type, and there's only one for 
			//		each serial number, also that the order in which 
			//		the messages are sent means it's more likely the 
			//		last in the list will be acknowledged first 
			//		(which also makes <erase> more efficient)
			// NOTE: we don't check for error codes at this point; we 
			//		assume that if we were able to create a call id 
			//		any problems with individual parameters (which 
			//		will be reported, see above) won't stop the 
			//		connection being requested
		if ((b[0] & 0x70) == 0x30) {
			i = (int)conn_pend.size();
			while (i > 0) {
				i -= 1;
				ConnReqInfo& ci = conn_pend[i];
				if (ci.m[0][0] != 0x30) continue;	// not for a Set
				j = (int)ci.m.size();
				if (j > 1) do {
					j -= 1;
					if (ci.m[j][1] == b[1]) {
						ci.m.erase(ci.m.begin() + j);
						break;
					}
				} while (j > 0);
				else if (ci.m[0][1] == b[1]) {
						// acknowledging the last one
						// deselect the output to show we've reached 
						//		the stage of requesting the connection
					if (theApp.output_list->sel_port.unit == this &&
							theApp.output_list->sel_port.port == 
														ci.dest_port) {
						theApp.output_list->sel_port.unit = NULL;
						theApp.mib_changed = true;
					}
						// remove the entry (invalidates <ci>)
					conn_pend.erase(conn_pend.begin() + i);
				}
			}
		}
	}

	if ((b[0] & 0xF0) == 0xA0) {
			// Status Response message
		if (b[0] != 0xA0 && b[0] != 0xAF) return; // if error signalled
		if (len == 2 && b[0] == 0xA0) return;	// OK reply to Status Request

		if (b[1] == 1) {
				// first in a cycle
			if (state == MGT_ST_HAVE_INFO) state = MGT_ST_ACTIVE;
			start_cycle_time = _time64(& prev_report_time) - 2;
			next_seq = 2;
		}
		else if (b[1] == next_seq && next_seq > 1) {
				// in-cycle Status report with the expected sequence number 
				//		during an OK cycle
				// check it's not so long after the previous packet that 
				//		it might be part of a later cycle
			__time64_t time_received = _time64(NULL);
			if (prev_report_time - time_received > 10) next_seq = 0;
			else {
				prev_report_time = time_received;
				if (next_seq == 0xFF) next_seq = 2;
				else next_seq += 1;
			}
		}
		else next_seq = 0;
	}
	else if ((b[0] & 0x70) == 0x70) {
			// console data
		if (b[0] & 0x80) return; // ack to user command: ignore for now

			// here if it's data to be displayed in the controller 
			//		window
			/* +++ remove the following when we know the VM is OK 
			//		crossing page boundaries
		if (theApp.controller_doc->UnitIs(this)) {
			s.Format(" - %d", b[1]);
			theApp.controller_doc->SetTitle(unit_name + s);
		}*/
			// send ack
		b[0] |= 0x80;
		SaveMessage(b, 2, (theApp.link_socket->TxMessage(b, 2, 
											tx_flow) ? 't' : 'e'));
		if (b[1] == last_console_serial) return;	// repetition
		last_console_serial = b[1];
		SaveConsole(b + 2, len);
		return;
	}
	else if ((b[0] & 0xF0) == upd_reply && b[1] == upd_msg_ser) {
			// it's the reply to a message sent as part of the process 
			//		of collecting the flash map or updating the flash
			// won't come here if <upd_reply> is coded as "status 
			//		reponse"
			// we don't include anything from the flash map in <mib>
			// if it's a "busy" reply to a write, just wait for the 
			//		repeat
		if (b[0] == 0xBE && upd_state >= 0) return;
		upd_reply = 0xA0;
		upd_msg.m.clear();
		if ((b[0] & 0x0F) != 0) {
				// error signalled in reply
			SetStateError();
			return;
		}
		switch (upd_state) {
default: 
/*#ifdef _DEBUG
			ASSERT(FALSE);
#endif*/
//			SetStateError();	// shouldn't be expecting a message
case UPD_ST_NO_ACTION:	// +++ need to add diagnostics that will 
						//		explain why we can get a GetNext 
						//		response in this state
			return;

case UPD_ST_COLLECT_MAP: // filling in the map; message will have been 
						 //		GetNext
			while (true) {
				m = new MibObject(p, len);
				if (m->tag == TAG_INVALID) {
					goto update_failed;
				}
				if (m->oid.Left(20) != "1.0.62379.1.1.5.1.1.") {
						// not in the table, so must have collected 
						//		everything
					if (unit_id == 0x0090A8990000000DLL) {
							// +++ KLUDGE: in unit 13 block 145 fails 
							//		to program; ideally the VM code 
							//		should handle bad blocks, maybe 
							//		via "sticky" MIB objects
						FlashMap::iterator p;
						p = flash.begin();
						unless (p == flash.end()) do {
							if (p->second.status != AREA_STATUS_EMPTY) continue;
							if (p->first > 145) continue; // area id is block number
							if (p->first + (p->second.length >> 16) <= 145) continue;
								// here if we've found an empty area that includes 
								//		block 145; we can use the part below block 
								//		145, but using the part above would be more 
								//		difficult as it would require inventing a 
								//		new area
							i = 145 - p->first;
							if (i > 0) p->second.length = i << 16;
							else p->second.status = AREA_STATUS_INVALID;
						} until (++p == flash.end());
					}
					upd_state = UPD_ST_HAVE_MAP;	// trigger for OnIdle()
					delete m;
					return;
				}

					// here if it's in the table
					// set <j> to the column number and <i> to the area id
				if (sscanf_s(m->oid.Mid(20), "%d.%d", &j, &i) != 2) {
					delete m;
					continue;
				}

				switch (j) {
						// +++ ought to read the class (case 2) as well
		case 3:		flash[i].access = m->IntegerValue(); break;

		case 4:		k = m->IntegerValue();
					if (k == AREA_STATUS_ERASING) {
							// don't want to go on to upload etc until 
							//		it's done
						upd_state = UPD_ST_WAITING;
						delete m;
						return;
					}

					flash[i].status = k;
					break;

		case 5:		flash[i].length = m->IntegerValue(); break;
		case 6:		flash[i].data_type = m->IntegerValue(); break;
		case 7:		flash[i].serial = m->IntegerValue(); break;

		case 8:		if (m->tag == ASN1_TAG_OCTET_STRING) {
						flash[i].vn.FromFilename(*m);
						break;
					}
					flash[i].vn.Invalidate(); // if not an octet string
				}

				if (len <= 0) break;
				delete m;
			}

				// here to ask for more
				// <m> is the last object in the message
			b[0] = 0x1F;
			TxNewMessage(b, m->CopyOid(b + 2) + 2, true, true);
			delete m;
			return;

case UPD_ST_UPLOADING:	// writing; message will be reply to a Set
				// we've already checked it's a reply to the expected 
				//		request message and not indicating any error 
				//		in the header
				// the tag should be OCTET_STRING when accessing the 
				//		Content table and INTEGER when accessing the 
				//		Area table
			m = new MibObject(p, len);
			k = (int)m->size();
			if (k == 0 || (m->tag != ASN1_TAG_INTEGER && 
						m->tag != ASN1_TAG_OCTET_STRING)) {
				goto update_failed;
			}

			if (m->tag == ASN1_TAG_INTEGER) {
					// either one of the preliminaries or setting state at end

				switch (upd_offset) {
		default:		// assume it's setting swaStatus to Valid after writing
						// 62379-1 says value may be returned as "being written" 
						//		so we allow that, though current VM code waits 
						//		until the header has been written before sending 
						//		the reply
					if (m->value != AREA_STATUS_VALID && 
										m->value != AREA_STATUS_BEING_WR) {
						goto update_failed;
					}
					delete m;
						// collect the map again; this is required when uploading 
						//		both logic and VM code to a flash in which all 
						//		the free space is in one large area, and also 
						//		lets us know whether the unit has done any 
						//		tidying up
					StartCollectFlashMap();
					return;

		case -4:		// setting swaStatus to Writing
					if (m->value != AREA_STATUS_WRITING) {
						goto update_failed;
					}
					upd_area->second.status = AREA_STATUS_WRITING;
					break;

		case -3:		// swaLength: managed unit may round it up
					if (m->value < (int)image.size()) {
						goto update_failed;
					}
					break;

		case -2:		// swaType
					if (m->value != upd_area->second.data_type) {
						goto update_failed;
					}
					break;

		case -1:		// swaSerial
					if (m->value != upd_area->second.serial) {
						goto update_failed;
					}
				}

				upd_offset += 1;
			}
			else {
					// writing data
				i = 0;
				while (i < k) {
					if (m->at(i) != image[upd_offset + i]) {
						goto update_failed;
					}
					i += 1;
				}

				upd_offset += k;
			}

			delete m;	// finished with incoming message

				// now send the next message
			b[0]  = 0x30;	// "Set" request
			b[2]  = ASN1_TAG_OID;
			b[4]  = 0x28;	// OID begins 1.0.62379.1.1.5
			b[5]  = 0x83;
			b[6]  = 0xE7;
			b[7]  = 0x2B;
			b[8]  = 1;
			b[9]  = 1;
			b[10] = 5;
				// bytes 11-13 are the last 3 arcs before the index(es)
			b[12] = 1;		// this arc is 1 in all cases
			p = b + 14;
			AddIndex(p, upd_area->first);	// first index in all cases

			if (upd_offset < 0) {
					// we're still in the preliminaries
					// OID = 1.0.62379.1.1.5.1.1.c.a (c = column, a = area)
					// <p> points to where the tag byte for the value will go
				b[3] = (uint8_t)((p - b) - 4);
				b[11] = 1;
				b[13] = upd_offset + 8;

				switch (upd_offset) {
			default:	// assume -3: swaLength (c = 5)
					i = (int)image.size(); // new length
					break;

			case -2:	// swaType (c = 6)
					i = upd_area->second.data_type;	// new type
					break;

			case -1:	// swaSerial (c = 7)
					i = upd_area->second.serial;	// new serial number
				}

				AddInteger(p, i); // write the new value
			}
			else {
					// sending data: set <k> to bytes still to be sent
				k = (int)image.size() - upd_offset;
				if (k > 0) {
						// send a tranche of data
						// OID = 1.0.62379.1.1.5.2.1.4.a.o.l
						// <p> points to where the second index will go
					if (k > MAX_DATA_LENGTH) k = MAX_DATA_LENGTH;

					b[11] = 2;
					b[13] = 4;	// column number for swcData

					AddIndex(p, upd_offset);
					AddIndex(p, k);
						// now <p> points to where the tag byte for the value will go
					b[3] = (uint8_t)((p - b) - 4);
					*p++ = ASN1_TAG_OCTET_STRING;
					AddLength(p, k);
						// copy the data
					i = 0;
					do *p++ = image[upd_offset + i++]; while (i < k);
				}
				else {
						// here when the last tranche has been acknowledged
						// set the area to "valid"
						// OID = 1.0.62379.1.1.5.1.1.4.a (a = area)
						// <p> points to where the tag byte for the value will go
					b[11] = 1;
					b[13] = 4;	// column number for swaStatus
					b[3] = (uint8_t)((p - b) - 4);
					*p++ = ASN1_TAG_INTEGER;
					*p++ = 1;
					*p++ = AREA_STATUS_VALID;
				}
			}

				// now <b> points to the first byte of the message and <p> to the 
				//		byte after last
			TxNewMessage(b, (int)(p - b), true, true);
			return;

case UPD_ST_ERASING:	// tidying away old or invalid areas
				// we've already checked it's a reply to the expected request 
				//		message and not indicating any error in the header
				// the tag should be INTEGER
			m = new MibObject(p, len);
			k = (int)m->size();
			if (k == 0 || (m->tag != ASN1_TAG_INTEGER) || 
										m->value != AREA_STATUS_ERASING) {
				goto update_failed;
			}
			delete m;	// finished with incoming message

			SendNextErase();
			return;
		}

update_failed:
		delete m;
		upd_state = UPD_ST_FAILED;
		return;
	}
	else unless (b[0] & 0x80) return; // not a response

		// here to add all the objects to the MIB
		// <m> should still be NULL at this point; code above that 
		//		collects VarBinds exits without adding the objects to 
		//		the MIB because we don't want to clutter it up with, 
		//		e.g., fragments of flash
	int block_id;
	void * q;
	POSITION q2;
	PortList * list;
	bool value_is_new;

	while (len > 0) {
			// p -> where the OID tag should be
			// len = number of bytes left in message
		m = new MibObject(p, len);
		if (m->tag == TAG_INVALID) {
			delete m;
			m = NULL;
			break;
		}

			// objects that are part of the flash map don't get recorded in 
			//		the MIB; we assume that if it's a GetNext we're not 
			//		interested in any other objects that may follow the table
		if (m->oid.Left(16) == "1.0.62379.1.1.5.") {
			delete m;	// we haven't stored it in the MIB
			if ((b[0] & 0xF0) != 0xA0) return;
				// here if in a status broadcast
			m = NULL;
			continue;
		}

		m->recd = _time64(NULL);
		m->msg_type = b[0] & 0xF0;

			// now <m> holds the object we've extracted from the message
			// <p> and <len> have been updated
		if (mib_map.Lookup(m->oid, q)) {
				// it's already in the MIB, at position q
				// we replace it anyway, because we want the new <recd>, 
				//		but only signal a change if the new value is 
				//		different
				// note that the map doesn't change because we keep the 
				//		same list entry
			MibObject * m_old = mib.GetAt((POSITION)q); // previous value
			mib.SetAt((POSITION)q, m);
			value_is_new = m->ChangedFrom(m_old);
			delete m_old;
			if (value_is_new) {
				theApp.mib_changed = true;
				UpdateDisplay();
			}
		}
		else {
				// it's new: add to MIB and map
			mib.AddTail(m);
			mib_map.SetAt(m->oid, mib.GetTailPosition());
			value_is_new = true;
			theApp.mib_changed = true;
			UpdateDisplay();
		}

			// here to update lists etc
		if (m->oid.Left(10) != "1.0.62379.") continue;
		rel_oid = m->oid.Mid(10);

			// here if it may be one of the objects we want to list
			// we check the lists even if the value is unchanged in the 
			//		MIB; I think there may be circumstances in which 
			//		both the previous and the new flows for a port are 
			//		in the MIB, and we want to be sure we have the one 
			//		that is now being reported
			// +++ maybe now that we don't report Transferred flows that is no 
			//		longer an issue; however we still need to do it for the link 
			//		partner address in case it reconnects to the same port
		if (rel_oid.Left(6) == "5.1.1.") {
			if (rel_oid.Mid(6, 8) == "2.1.1.7." && m->value != 15) {
					// usState, value is not "Transferred"
				i = rel_oid.ReverseFind('.');	// last arc is the block id
				output_flows.SetAt(rel_oid.Mid(i + 1), rel_oid.Mid(14));
				continue;
			}

			if (rel_oid.Mid(6, 8) == "3.3.1.2.") {
					// udNetBlockId
				if (m->tag != ASN1_TAG_INTEGER) continue;
				q = input_port_list.Find(m->value);
				if (q) {
						// it's an input port so remember the flow; we don't 
						//		include flows through network ports, especially 
						//		in <flow_senders>
					s.Format("%d", m->value);
					input_flows.SetAt(s, rel_oid.Mid(14));
					theApp.flow_senders.SetAt(rel_oid.Mid(14), this);
					continue;
				}
			}

			if (value_is_new && rel_oid.Mid(6, 8) == "1.1.1.3.") {
					// state of a network port
				block_id = IntIndex(rel_oid.Mid(14));
				if (block_id < 0) continue;
				i = m->IntegerValue();
				if (i < 0) continue;
				unless (net_port_state.NewState(block_id, i)) continue;
					// here if new state is _LINK_UP and have set 
					//		_WAITING
					// +++ at this point earlier versions checked to 
					//		see whether they needed to upload any 
					//		configuration or connect any virtual 
					//		links; now that that's not needed maybe 
					//		we don't need _WAITING state?
				net_port_state[block_id] = NET_PORT_STATE_LINK_UP;
				continue;
			}

/*			if (rel_oid.Mid(6, 8) == "1.1.1.7.") {
					// link partner address for a network port
				if (m->empty()) continue;
				MgtSocket * q;
					// +++ previously if the managed unit cleared the 
					//		call down, e.g. bad password, we would 
					//		keep setting up new calls; the logic needs
					//		tidying up potentially there's something 
					//		we need to look out for here
				if (theApp.unit_addrs.Lookup(ByteArrayToHex(*m), 
										(void *&)q) && q == this) {
					ASSERT(state = MGT_ST_CLOSED);
					state = MGT_ST_ACTIVE;
				}
				continue;
			}
*/
			continue;
		}

		if (rel_oid == "1.1.1.16.0") {
				// unitIdentifier; note that this is an addition to Table 1 
				//		of 62379-1:2007
			unless (m->size() == 8) continue;
			unit_id = m->at(0);
			i = 0;
			do unit_id = (unit_id << 8) | m->at(++i); while (i < 7);
			continue;
		}

		if (rel_oid == "1.1.1.8.0") {
				// unitFirmwareVersion; assume we already have unitIdentity
			if (upd_state == UPD_ST_NO_INFO) upd_state = UPD_ST_BEGIN;
			continue;
		}

			// we've now checked all the ones for which we may want to do 
			//		something when it's not new
		unless (value_is_new) continue;

		if (rel_oid == "1.1.1.1.0") {
				// unitName
				// +++ NOTE: not currently included in status broadcasts, 
				//		so we don't get told if another management terminal 
				//		changes it
			unit_name = m->StringValue();
			theApp.controller_doc->NameChanged(this);
			continue;
		}

		if (rel_oid.Left(12) == "2.1.1.1.1.2." || 
										rel_oid.Left(12) == "3.1.1.1.1.2.") {
				// direction of an audio or video port
				// we choose the list according to the value, and don't 
				//		store it at all if it's neither input nor output
			block_id = IntIndex(rel_oid.Mid(12));
			if (block_id < 0) continue;
			switch (m->value) {
default:		continue;

case DIRECTION_IN:
				list = &input_port_list;
				break;

case DIRECTION_OUT:
				list = &output_port_list;
			}

			q = PortLocation(list, block_id);
			if (q == NULL) list->AddHead(block_id);
			else unless (list->GetAt((POSITION)q) == block_id) 
								list->InsertAfter((POSITION)q, block_id);
			continue;
		}
	}

		// here when can't extract the next (OID, value) pair
		// <m> points to the last object that was added to the MIB 
		//		(which for Get and Set should be the only one in the 
		//		message), or NULL if no objects or the last one was 
		//		invalid or not an object we store in the MIB 
		//		(currently that means one in the table of software 
		//		areas)
	if ((b[0] & 0xF0) != 0xA0) {
			// here if not a status report, so should be a Get &c 
			//		response
		unless (call_id_error || ((b[0] & 0xF0) == 0x80 && m && 
						m->oid == "1.0.62379.5.1.1.3.2.0")) return;

			// here if a GetResponse for unitNextCallId
			// the loop here is somewhat convoluted because <ci> 
			//		has to be initialised at the point where it's 
			//		declared, which must therefore be inside the loop, 
			//		and so its scope can't extend beyond the loop
			// the issues with ByteString and CByteArray (see 
			//		declaration of <ConnReqInfo>) make it really 
			//		quite inefficient, too
		i = (int)conn_pend.size();
		while (i > 0) {
			i -= 1;
			ConnReqInfo& ci = conn_pend[i];
			unless (ci.m[0][1] == b[1]) continue; // ser nr mismatch
			unless (ci.m[0][0] == 0) continue;	// not a Get request

				// here with <ci> being the entry waiting for this 
				//		message
			if (call_id_error) {
					// error response (which will have been added to 
					//		<theApp.err_msgs>)
					// remove the entry (invalidates <ci>)
				conn_pend.erase(conn_pend.begin() + i);
				return;
			}

				// here if an OK response
				// convert the call id into a flow id by adding 
				//		path ref 1, direction towards the owner, 
				//		and flow ref 1
			m->push_back(3);
			m->push_back(0);
			m->push_back(0);
			m->push_back(1);

				// send Set for udDestBlockId: must be done first, to 
				//		create srce rec
			ByteString msg;
			msg.resize(15);
			msg[0]  = 0x30;			// (1) is msg ident
			msg[2]  = ASN1_TAG_OID;	// (3) is OID length
			msg[4]  = 40;				// 1.0
			msg[5]  = 0x83;
			msg[6]  = 0xE7;
			msg[7]  = 0x2B;			// 62379
			msg[8]  = 5;
			msg[9]  = 1;
			msg[10] = 1;
			msg[11] = 3;
			msg[12] = 3;
			msg[13] = 1;
			msg[14] = 21; 
			if (!m->AppendAsIndexTo(msg)) return;
			int v_posn = (int)msg.size();	// offset to where value goes
			msg[3] = v_posn - 4;		// length of OID
			msg.push_back(ASN1_TAG_INTEGER);
			k = 1;
			while (ci.dest_port >= (128 << (8 * (k - 1))) && k < 4) k += 1;
			msg.push_back(k);
			do { msg.push_back(ci.dest_port >> (8 * (k - 1))); k -= 1; } while (k > 0);
			TxNewMessage(msg);

			ByteString bs;
			k = (int)msg.size();
			bs.resize(k);
			j = 0;
			do {
				bs[j] = msg[j];
				j += 1;
			} while (j < k);

			s.Format("%d", ci.dest_port);
			s = GetStringObject("1.0.62379.2.1.1.1.1.5." + s); // aPortName
		/*	if (s.IsEmpty() || !theApp.name_translations.Lookup(s, s) || 
								sscanf_s(s, " (%u", &j) != 1)*/ ci.m.resize(4);
		/*	else {
					// have an audio port name including an importance
				ci.m.resize(5);
				ci.m[4] = bs;
					// send Set for importance
				msg.SetAt(14, 14);				// column; rest of OID is the same
				msg.SetAt(v_posn, ASN1_TAG_INTEGER);
				if (j < 128) {
					k = v_posn + 3;
					msg.SetSize(k);
					msg.SetAt(v_posn + 1, 1);		// length
					msg.SetAt(v_posn + 2, j);		// value
				}
				else {
					k = v_posn + 4;
					msg.SetSize(k);
					msg.SetAt(v_posn + 1, 2);		// length
					msg.SetAt(v_posn + 2, j >> 8);	// value
					msg.SetAt(v_posn + 3, j);
				}
				TxNewMessage(msg);

				bs.resize(k);
				j = 0;
				do {
					bs[j] = msg[j];
					j += 1;
				} while (j < k);
			}*/
			ci.m[3] = bs;


				// send Set for udSourceAddrType
			k = v_posn + 9;
			msg.resize(k);
			msg[14] = 4;				// column; rest of OID is the same
			msg[v_posn] = ASN1_TAG_OID;
			msg[v_posn + 1] = 7;		// length
			msg[v_posn + 2] = 40;		// 1.0
			msg[v_posn + 3] = 0x83;
			msg[v_posn + 4] = 0xE7;
			msg[v_posn + 5] = 0x2B;	// 62379
			msg[v_posn + 6] = 5;
			msg[v_posn + 7] = 2;
			msg[v_posn + 8] = 2;
			TxNewMessage(msg);

			bs.resize(k);
			j = 0;
			do {
				bs[j] = msg[j];
				j += 1;
			} while (j < k);
			ci.m[2] = bs;

				// send Set for udSourceAddress
			msg.resize(v_posn + 2);
			msg[14] = 5;				// column; rest of OID is the same
			msg[v_posn] = ASN1_TAG_OCTET_STRING;
			ByteString& uta = ci.srce_addr;
			k = (int)(uta.size());
			msg[v_posn + 1] = (uint8_t)k;
			j = 0;
			while (j < k) msg.push_back(uta[j++]);
			TxNewMessage(msg);

			k = (int)msg.size();
			bs.resize(k);
			j = 0;
			do {
				bs[j] = msg[j];
				j += 1;
			} while (j < k);
			ci.m[1] = bs;

				// send Set for udState: must be done last, to send FindRoute
			k = v_posn + 3;
			msg.resize(k);
			msg[14] = 9;				// column; rest of OID is the same
			msg[v_posn] = ASN1_TAG_INTEGER;
			msg[v_posn + 1] = 1;		// length
			msg[v_posn + 2] = 1;		// 1 = readyToConnect
			TxNewMessage(msg);

			bs.resize(k);
			j = 0;
			do {
				bs[j] = msg[j];
				j += 1;
			} while (j < k);
			ci.m[0] = bs;
		}
		return;
	}

		// here if a status report; nothing more to do if a change message
	if (b[1] == 0) return;

		// here if an in-cycle message
	if (len != 0) {
			// still something left in the message but not a valid object
		next_seq = 0;	// no longer sure of the integrity of this cycle
		return;
	}

	if (next_seq < 2 || (b[0] & 0x0F) != 0x0F) return;

		// here if end of an OK cycle
		// request another; don't need to do this every time but it's not 
		//		not much of an overhead to do so
	RequestStatus(false);

		// go through the MIB removing anything that should have been reported 
		//		during this cycle but wasn't
	q2 = mib.GetHeadPosition();
	while (q2 != NULL) {
		q = q2;
		m = mib.GetNext(q2);
			// don't remove objects that were reported in the reply to a Get 
			//		or GetNext (such as UnitIdentity) 
		if ((m->msg_type & 0x20) == 0) continue;

		if (m->recd >= start_cycle_time) continue; // been reported this cycle

			// here if <m>, which is at position <q>, was last reported in a 
			//		Status Response or Set Response before the current cycle 
			//		so must now have become obsolete; Set Response includes 
			//		dest list entries for temporary flow ids and when setting 
			//		state to "terminating"
			// remove from <mib> and <mib_map>
		mib.RemoveAt((POSITION)q);
		mib_map.RemoveKey(m->oid);

		if (m->oid.Left(10) != "1.0.62379.") {
			delete m;
			continue;
		}

			// here if it might be in one of the lists
		rel_oid = m->oid.Mid(10);
		bool integer = (m->tag == ASN1_TAG_INTEGER);
		int value = m->value;
		delete m;

			// the rest of the tidying up has been done, so we just need to 
			//		find and remove the entry for <rel_oid>, if any
		if (rel_oid.Left(6) == "5.1.1.") {
			if (rel_oid.Mid(6, 8) == "1.1.1.3.") {
					// state of a network port; note that <erase> does 
					//		nothing if the key isn't found, which will be 
					//		the case if IntIndex returns -1
					// NB we don't expect this to happen for physical ports
				net_port_state.Remove(IntIndex(rel_oid.Mid(14)));
				continue;
			}

			continue;
		}

		if (integer && (rel_oid.Left(12) == "2.1.1.1.1.2." || 
										rel_oid.Left(12) == "3.1.1.1.1.2.")) {
				// direction of an audio or video port
				// we choose the list according to the value, and don't 
				//		store it at all if it's neither input nor output
			block_id = IntIndex(rel_oid.Mid(12));
			if (block_id < 0) continue;
			switch (value) {
default:		continue;

case DIRECTION_IN:
				list = &input_port_list;
				break;

case DIRECTION_OUT:
				list = &output_port_list;
			}

			q = list->Find(block_id);
			if (q != NULL) list->RemoveAt((POSITION)q);
			continue;
		}
	}
}


// see whether there are any areas marked INVALID, and if so send the 
//		Erase request; set <upd_state> accordingly
// searches the whole map each time, in case it has changed
void MgtSocket::SendNextErase() {
	upd_area = flash.begin();
	while (true) {
		if (upd_area == flash.end()) {
				// none found
			upd_state = (upd_state == UPD_ST_ERASING ? UPD_ST_WAITING : 
							upd_state == UPD_ST_MAKE_SPACE ? 
								UPD_ST_BAD_FLASH : UPD_ST_UP_TO_DATE);
			return;
		}
		if (upd_area->second.status == AREA_STATUS_INVALID) break;
		upd_area++;
	}

		// here if we've found an area to be erased
	unless (OkToUpdate()) return;

	uint8_t b[24];
	b[0]  = 0x30;	// "Set" request
	b[2]  = ASN1_TAG_OID;
	b[4]  = 0x28;	// OID 1.0.62379.1.1.5.1.1.4.a
	b[5]  = 0x83;
	b[6]  = 0xE7;
	b[7]  = 0x2B;
	b[8]  = 1;
	b[9]  = 1;
	b[10] = 5;
	b[11] = 1;
	b[12] = 1;
	b[13] = 4;
	uint8_t * p = b + 14;
	AddIndex(p, upd_area->first);	// index: 1 or 2 bytes
	b[3] = (uint8_t)((p - b) - 4);	// OID size, assumed < 128
	AddInteger(p, AREA_STATUS_ERASING); // new value: 3 bytes

	TxNewMessage(b, (int)(p - b), true, true);

	upd_area->second.status = AREA_STATUS_ERASING;
	upd_state = UPD_ST_ERASING;
}


// add arc <n> to an OID (e.g. for an integer index)
// <p> points to where to put first byte; on exit points to byte after last
// if n < 0 just writes a zero
void FlexilinkSocket::AddIndex(uint8_t * &p, int n) {
	if (n < 0) n = 0;

		// set <k> to 7 * ((bytes to add) - 1)
	int k = 0;
	while (n >= (128 << k) && k < 28) k += 7;
	while (k > 0) { *p++ = (uint8_t)((n >> k) | 0x80); k -= 7; }
	*p++ = (uint8_t)(n & 0x7F);
}


// add arc <n> to the OID at the end of <m>
// if n < 0 just writes a zero
void FlexilinkSocket::AddIndex(ByteString& m, int n) {
	if (n < 0) n = 0;

		// set <k> to 7 * ((bytes to add) - 1)
	int k = 0;
	while (n >= (128 << k) && k < 28) k += 7;
	while (k > 0) { m.push_back((uint8_t)((n >> k) | 0x80)); k -= 7; }
	 m.push_back((uint8_t)(n & 0x7F));
}


// add <n> as the "length" field for a value
// <p> points to where to put first byte; on exit points to byte after last
// if n < 0 just writes a zero
void FlexilinkSocket::AddLength(uint8_t * &p, int n) {
	if (n < 0) n = 0;

	if (n < 128) { *p++ = (uint8_t)n; return; }

		// set <k> to the number of bytes needed for the length
	int k = 1;
	while (n >= (1 << (k * 8)) && k < 3) k += 1;
	*p++ = (uint8_t)(0x80 + k);
	while (--k > 0) *p++ = (uint8_t)(n >> (k * 8));
		// I don't trust C to get shift-by-zero right so have taken the 
		//		last byte out of the loop
	*p++ = (uint8_t)n;
}


// add integer value <n> to a message
// <p> points to where to put first byte; on exit points to byte after last
void FlexilinkSocket::AddInteger(uint8_t * &p, int64_t n) {
	*p++ = ASN1_TAG_INTEGER;
	int64_t j;
	uint8_t k = 1;
	if (n < 0) {
		j = -128;
		while (n < j) { k += 1; if (k >= 8) break; j = j << 8; }
	}
	else {
		j = 128;
		while (n >= j) { k += 1; if (k >= 8) break; j = j << 8; }
	}

	*p++ = k;	// length
	while (--k > 0) *p++ = (uint8_t)(n >> (k * 8));
		// I don't trust C to get shift-by-zero right so have taken the 
		//		last byte out of the loop
	*p++ = (uint8_t)n;
}


// add integer value <n> to the end of <m> as an ASN.1 integer
void FlexilinkSocket::AddInteger(ByteString& m, int64_t n) {
	m.push_back(ASN1_TAG_INTEGER);
	int64_t j;
	uint8_t k = 1;
	if (n < 0) {
		j = -128;
		while (n < j) { k += 1; if (k >= 8) break; j = j << 8; }
	}
	else {
		j = 128;
		while (n >= j) { k += 1; if (k >= 8) break; j = j << 8; }
	}

	m.push_back(k);	// length
	while (--k > 0) m.push_back((uint8_t)(n >> (k * 8)));
		// I don't trust C to get shift-by-zero right so have taken the 
		//		last byte out of the loop
	m.push_back((uint8_t)n);
}


// return the text description used in crosspoint windows
CString MgtSocket::DisplayName() {
	CString str = GetStringObject("1.0.62379.1.1.1.1.0"); // unit name
	CString s = GetStringObject("1.0.62379.1.1.1.2.0"); // location
	unless (str.IsEmpty()) {
		if (s.IsEmpty()) return str;
		return str + " in " + s;
	}

	unless (s.IsEmpty()) return s;

		// product name + unit id or MAC address
	str = GetStringObject("1.0.62379.1.1.1.6.0");	// product name
	s = GetStringHex("1.0.62379.1.1.1.16.0", 4); // unit id
	if (s.IsEmpty()) s = GetStringHex("1.0.62379.1.1.1.3.0", 1); // MAC addr
	if (s.IsEmpty()) return str;
	if (str.IsEmpty()) return "unit " + s;
	return str + ' ' + s;
}

// if <s> is a decimal integer, return its value; otherwise return -1
// intended for retrieving an index value from the end of an OID
int MgtSocket::IntIndex(CString s) {
	int i;
	int n = -1;
	sscanf_s(s, "%d%n", &i, &n);
	return n == s.GetLength() ? i : -1;
}


// return the last entry in the list that is less than or equal to n, or 
//		NULL if none
// we search from the end because entries will often appear in the correct 
//		order, so new entries always go onto the end
POSITION MgtSocket::PortLocation(PortList * list, int n) {
	POSITION p = list->GetTailPosition();
	while (p != NULL) {
		POSITION q = p;
		int i = list->GetPrev(p);
		if (i <= n) return q;
	}

	return NULL;
}


// first character is:
//		't' for transmitted message,
//		'e' for transmission error,
//		'r' for an OK received message
// the above in upper case for signalling messages; also
//		'o' for an overlength received message,
//		'?' for a received message without a valid AES51 header
//		'H' for a dump of an iteration of the hash calculation
// also flags that the state has changed
void FlexilinkSocket::SaveMessage(uint8_t * b, int len, char flag)
{
		// only save the last 300-ish; remove several at a time in case 
		//		removal involves a lot of copying
	if (msgs.GetSize() > 300) msgs.RemoveAt(0,10);

	if (len == 0) {
		msgs.Add("*** empty message ***");
		UpdateMibDisplay();
		return;
	}

		// only save the first 160 bytes if 3 chars/byte; longer lines 
		//		exceed the maximum window size
	CString s;
//	s.Format("%c%4d", flag, len);
	s.Format("%c%4d%2d", flag, len, state); // +++ TEMP
	if (flag == 'H') {
		if (len > 200) len = 200;
		int i = 0;
		while (i < len) {
			s.AppendFormat(((i & 7) == 0) ? " %02X" : "%02X", (int) b[i^7]);
			i += 1;
		}
	}
	else {
		if (len > 160) len = 160;
		int i = 0;
		while (i < len) {
			s.AppendFormat(" %02X", (int) b[i]);
			i += 1;
		}
	}
	msgs.Add(s);

	UpdateMibDisplay();
}

/*
// as above but with the message in a ByteString
void  FlexilinkSocket::SaveMessage(ByteString b, char flag)
{
		// only save the last 300-ish; remove several at a time in case 
		//		removal involves a lot of copying
	if (msgs.GetSize() > 300) msgs.RemoveAt(0,10);

		// only save the first 160 bytes; longer lines exceed the maximum 
		//		window size
	int len = (int)b.size();
	if (len == 0) {
		msgs.Add("*** empty message ***");
		UpdateMibDisplay();
		return;
	}

	CString s;
	s.Format("%c%4d", flag, len);
	if (len > 160) len = 160;

	int i = 0;
	while (i < len) {
		s.AppendFormat(" %02X", (int) b[i]);
		i += 1;
	}
	msgs.Add(s);

	UpdateMibDisplay();
}
*/

// add text <b> to the console output
// also switches to console display if this unit is in the controller window
void FlexilinkSocket::SaveConsole(unsigned char * b, INT_PTR len)
{
	std::string s;
	int i;
	if (b[0] == '\n') i = 0;
	else {
			// start by adding text to the current last line
		i = (int)console.size();
		if (i > 0) {
			s = console.at(i - 1);
			console.pop_back();
		}
		i = -1;
	}

	while (++i < len) {
		if (b[i] == '\n') {
			console.push_back(s);
			s.clear();
		}
		else s += b[i];
	}

	ConsoleLine(s);
}

// add line <s> to the console output
void FlexilinkSocket::ConsoleLine(std::string s)
{
	console.push_back(s);

		// only save the last 2000-ish lines; remove several at a time in case 
		//		removal involves a lot of copying
	int i = (int)console.size() - 2000;
	if (i > 25) console.erase(console.begin(), console.begin() + i);

	UpdateDisplay();
}


// check whether anything needs to be repeated: called every 1/2 sec 
//		as long as the link is up
// sets the state to "timed out" if the unit seems to have stopped 
//		responding
// note that if we get no reply to a request because the link has gone 
//		down we repeat it if a new link comes up; +++ that assumes 
//		that all state is at layer 5, which I don't think is the case 
//		with updating the flash, and we really ought to separate 
//		session-layer state from application-layer state; most of the 
//		MIB accesses are stateless as far as the managed unit is 
//		concerned, though
// does nothing (so is safe) if <this> is NULL

// base class version; returns whether OK
bool FlexilinkSocket::PollAwaitingAck()
{
	if (this == NULL || 
			theApp.link_socket->state != LINK_ST_ACTIVE) return false;

	if (keep_alive_count > 130) {
			// seen nothing for 1 min 5 secs; ought to have had three 
			//		status broadcasts in that time
		SetStateTimedOut();
		return false;
	}
	keep_alive_count += 1;

	if (state > MGT_ST_MAX_OK) {
			// various kinds of failure; in general if, say, a 
			//		connection is cleared down we retry it immediately 
			//		and if that doesn't work retry again every 10 
			//		secs, also whenever the MIB changes and the new  
			//		configuration suggests we ought to be able to 
			//		connect
			// +++ for the ones we don't re-try, ought to wait a bit 
			//		and then delete the MgtSocket object in case it's 
			//		a problem such as having got a password wrong
		if (state >= MGT_ST_MIN_RETRY || 
								keep_alive_count >= 20) SendConnReq();
		return false;
	}

	if (mgt_msg.m.empty()) return true;

	bool ok;
	if (mgt_msg.count >= MAX_REPEAT_COUNT) {
		SetStateTimedOut();
		return false;
	}
	mgt_msg.count += 1;
	if (mgt_msg.count > 1) {
			// resend the message
		if (state == MGT_ST_CONN_REQ) {
				// it's a signalling message
			ok = theApp.link_socket->TxMessage(mgt_msg.m);
			SaveMessage(mgt_msg.m.data(), mgt_msg.m.size(), (ok ? 'T' : 'E'));
			unless (ok) {
				SetStateFailed();
				return false;
			}
		}
		else TxMessage(mgt_msg.m); // NB simply repeats the payload verbatim
	}
	return true;
}


// version for management sockets
// +++ currently only called in one place which ignores the result
// NB analyser sockets just use the base class version
bool MgtSocket::PollAwaitingAck()
{
	unless (FlexilinkSocket::PollAwaitingAck()) return false;

	int i;
	unless (upd_msg.m.empty()) {
		if (upd_msg.count >= MAX_REPEAT_COUNT) {
			SetStateTimedOut();
			return true;
		}
		upd_msg.count += 1;
		if (upd_msg.count > 1) {
				// resend the message
			TxMessage(upd_msg.m);
		}
	}

	i = (int)conn_pend.size();
	while (i > 0) {
		ConnReqInfo& inf = conn_pend.at(--i);
		if (inf.count >= MAX_REPEAT_COUNT) {
			SetStateTimedOut();
			return true;
		}
		inf.count += 1;
		if (inf.count > 1) {
				// resend the messages
			int j = (int)inf.m.size();
			while (j > 0) TxMessage(inf.m[--j]);
		}
	}
	return true;
}
