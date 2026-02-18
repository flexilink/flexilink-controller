// TeaLeaves / Flexilink platform
// Copyright (c) 2012 Nine Tiles

// MgtSocket.cpp : implementation file
//

//#import "Controller.h"
//#import "ControllerDoc.h"
//#import "CrosspointDoc.h"
#import "MgtSocket.h"
#import <sys/types.h>
#import <sys/socket.h>
#import <sys/errno.h>
#import <sys/time.h>
#import <netinet/in.h>


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
}

MibObject::~MibObject() {
}


// set <tag> and the byte array from an ASN.1 value in a message
// <p> points to the tag, <len> is the number of bytes left in the message
// updates <p> and <len> (to skip over the object) and returns <true> if OK, 
//		else returns <false> without changing anything
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

	p = b + sz.len;
	len -= (sz.hd_len + sz.len);
	return true;
}


// return value of octet-string object as a character string
// result is 0 if <this> is NULL or tag not INTEGER; saturates if doesn't 
//		fit in 32 bits
int32_t MibObject::IntegerValue() const
{
	if (this == NULL || tag != ASN1_TAG_INTEGER) return 0;
	int n = size();
	if (n <= 0) return 0;
	int32_t r = (int8_t)(at(0)); // cast to signed so will sign extend
	if (n > 4) return (r < 0) ? INT32_MIN : INT32_MAX;
	int i = 1;
	while (i < n) { r = (r << 8) | at(i); i += 1; }
	return r;
}


// return value of octet-string object as a character string
// result is a null string if <this> is NULL or tag not OCTET STRING
std::string MibObject::StringValue() const
{
	std::string s;	// initially empty
	if (this == NULL || tag != ASN1_TAG_OCTET_STRING) return s;

	int n = size();
	int i = 0;
	while (i < n) { s += (char)at(i); i += 1; }
	return s;
}


// return value of octet-string object in hex
// result is a null string if <this> is NULL or tag not OCTET STRING
// if <group_size > 0>, a space is inserted after every <group_size> bytes 
//		except at the end of the string; else no spaces inserted
std::string MibObject::StringHex(int group_size) const
{
	std::string s;	// initially empty
	if (this == NULL || tag != ASN1_TAG_OCTET_STRING) return s;

	int i = 0;	// counts bytes
	int j = 0;	// counts bytes modulo <group_size>
	uint32_t n = size();
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
std::string MibObject::TextValue() const
{
	switch (tag) {
case ASN1_TAG_INTEGER:
		return "INT: " + ToDecimal(IntegerValue());

case ASN1_TAG_OCTET_STRING:
		return "STR: " + StringHex(1);

case ASN1_TAG_OID:
		return "OID: " + OidToText(*this);
	}

		// here if tag not recognised
	return "TAG " + ToHex(tag, 2) + ": " +  StringHex(1);
}


// append the value to <b> in the format for an index in an OID; returns 
//		<true> if OK, <false> with <b> unchanged else
// currently only copes with octet strings and positive 32-bit integers
bool MibObject::AppendAsIndexTo(std::vector<uint8_t> & b) {
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


// copy the value into a message (including length but not tag)
// length uses the shortest form it can
// caller must check there is snough room (and that the length is less 
//		than 64KB)
void MibObject::CopyValue(uint8_t * p)
{
	int n = size();
	if (n < 128) *p++ = (uint8_t)n;
	else if (n < 256) { *p++ = 0x81; *p++ = (uint8_t)n; }
	else { *p++ = 0x82; *p++ = (uint8_t)(n >> 8); *p++ = (uint8_t)n; }

	int i = 0;
	while (i < n) *p++ = at(i++);
}


// --------------------------- class VersionNumber


VersionNumber::VersionNumber()
{
	valid = false;
}


// fill in from ASCII characters in an octet string formatted as "n.n.n[ BETA n]"
// sets invalid if string not in right format; returns whether valid
bool VersionNumber::FromOctetString(ByteString b)
{
	std::string s;
	int n = b.size();
	if (n < 5) { valid = false; return false; }	// shortest valid is "0.0.0"
	s.resize(n);
	while (n > 0) {
		n -= 1;
		s.at(n) = b.at(n);
	}
	int v[4];
	n = (sscanf(s.c_str(), "%d.%d.%d BETA %d", &v[0], &v[1], &v[2], &v[3]));
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
	sprintf(buf, "%d.%d.%d", (int)(vn[0]), (int)(vn[1]), (int)(vn[2]));
	s = buf;
	if (vn[3] == 0) return s;

	sprintf(buf, " BETA %d", (int)(vn[3]));
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
	sprintf(buf, "%d.%d.%d", (int)(vn[0]), (int)(vn[1]), (int)(vn[2]));
	int n = 0;
	until (buf[n] == 0) { b.push_back(buf[n]); n += 1; }
	if (vn[3] == 0) return b;

	sprintf(buf, " BETA %d", (int)(vn[3]));
	until (buf[n] == 0) { b.push_back(buf[n]); n += 1; }
	return b;
}


// --------------------------- class MgtSocket

MgtSocket::MgtSocket()
{
	handle = -1;
	owner_doc = NULL;
	next_serial = 1;
	next_seq = 0;
//	image_area = -1;
//	unit_up_time = NULL;
//	mib_changed = true;
}

MgtSocket::MgtSocket(CControllerDoc * pDoc)
{
	handle = -1;
	owner_doc = pDoc;
	next_serial = 1;
	next_seq = 0;
//	image_area = -1;
//	unit_up_time = NULL;
//	mib_changed = true;
}


MgtSocket::~MgtSocket()
{
	unless (handle == -1) close(handle);

/*	if (owner_doc != NULL) {
		owner_doc->mgt_skt = NULL;
		owner_doc->UpdateAllViews(NULL);
		owner_doc = NULL;
	}

	theApp.unit_addrs.RemoveKey(unit_address);
	p = theApp.units.Find(this);
	if (p != NULL) theApp.units.RemoveAt(p);
*/}


// Create socket and set up the remote party's address
// both parameters are in host byte order
// assumes no socket currently existing
// sets <handle> to the socket handle if successful, -1 else
// returns either an error message or a success message
std::string MgtSocket::Connect(uint32_t ip_addr, uint32_t port)
{
	std::string s;
	handle = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (handle == -1) {
		s = "failed to create socket: ";
		s += strerror(errno);
		return s;
	}

		// set timeout for rcv to 2s; 1s doesn't always seem to give the VM 
		//		enough time to process the message and send a reply in the 
		//		case of a write to flash
	timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	unless (setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0) {
		s = "failed to set timeout: ";
		s += strerror(errno);
		close(handle);
		handle = -1;
		return s;
	}

		// apparently the "length" (which is sent both in the first byte of the 
		//		structure and separately in the call) has to include eight padding 
		//		bytes (set to zero) as well as the IP address and the port number; 
		//		note that that still leaves it 4 bytes short of what would be 
		//		needed for an IPv6 address
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_len         = sizeof(addr);
	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(port);
	addr.sin_addr.s_addr = htonl(ip_addr);
	unless (connect(handle, (const struct sockaddr *) &addr, sizeof(addr)) == 0) {
		s = "failed to set up remote address: ";
		s += strerror(errno);
		close(handle);
		handle = -1;
		return s;
	}

	s = "socket intialised for ";
	s += Ip4AddrString(ip_addr);
	s += ':';
	s += ToDecimal(port);
	return s;
}

/*
// send message from <b>, total size (including header) <len>
// fills in the serial number; all else must be already there
void MgtSocket::TxMessage(uint8_t * b, int len) {
	b[1] = next_serial;
	if (next_serial == 255) next_serial = 0;
	next_serial += 1;
	SaveMessage(b, len, (send(handle, b, len, 0) == len ? 't' : 'e'));
}


// send message <msg>
// fills in the serial number; all else must be already there
void MgtSocket::TxMessage(std::vector<uint8_t> & msg) {
	msg.SetAt(1, next_serial);
	if (next_serial == 255) next_serial = 0;
	next_serial += 1;
	int len = (int)msg.size();
	uint8_t * b = msg.GetData();
	SaveMessage(b, len, (Send(b, len) == len ? 't' : 'e'));
}


// send Status Request message
void MgtSocket::RequestStatus() {
	unsigned char b2[2];
	b2[0]  = 0x20;	// "Status" request
	TxMessage(b2, 2);
}
*/

// Ethernet MTU, used for buffer lengths; we keep the outgoing messages a 
//		bit shorter
#define MAX_REPLY_LENGTH  1500
#define MAX_DATA_LENGTH	  1200	// OK for IPv6, Flexilink2 bg, Ethernet

// send request message from <t_buf>, total size (including header) <len> 
//		bytes, assumed to be a GetNext, wait for the reply and fill in 
//		<mib> from it
// fills in the serial number in <t_buf>; all else must be already there
// repeats request up to 5 times if no reply
// returns MSG_ST_ code
int MgtSocket::SendGetNextRequest(uint8_t * t_buf, int len) {
	unless (IsConnected()) return MSG_ST_NOT_CONN;
	int retry_ct = 5;
	bool send_req = true;	// whether to send the request
	uint8_t buf[MAX_REPLY_LENGTH];	// max for an Ethernet packet, incl IP&UDP hdrs
	int n;
	t_buf[1] = next_serial;
	if (next_serial == 255) next_serial = 0;
	next_serial += 1;
	while (true) {
		if (send_req) {
			retry_ct -= 1;
			SaveMessage(t_buf, len, 
						(send(handle, t_buf, len, 0) == len ? 't' : 'e'));
		}

		n = recv(handle, buf, MAX_REPLY_LENGTH, 0);
		if (n < 0) {
				// check for timeout; note that the documentation says 
				//		this could be EWOULDBLOCK or EAGAIN but in the 
				//		Apple version of errno.h, which seems to be 
				//		derived from the Berkeley version, they're the 
				//		same code
			unless (errno == EWOULDBLOCK) return MSG_ST_SKT_ERROR;
				// else assume the request got lost so send it again
			if (retry_ct < 0) return MSG_ST_TIMED_OUT;
			send_req = true;
			continue;
		}
		SaveMessage(buf, n, 'r');
		if (n < MAX_REPLY_LENGTH && n >= 2 && buf[1] == t_buf[1] && 
							((buf[0] ^ t_buf[0]) & 0xF0) == 0x80) break;

			// here if not the reply to the request in <t_buf>; go read 
			//		again in case this is, say, late reply to a previous 
			//		request
		send_req = false;
	}

		// here with <buf> holding what appears to be a valid reply <n> 
		//		bytes long
	int i = buf[0] & 0xF;	// status code in message
	unless (i == MSG_ST_OK) return i;
	uint8_t * b = buf + 2;	// points to first OID
	n -= 2;

	while (n > 0 && *b == ASN1_TAG_OID) {
			// collect value into the MIB
		ValueSizes v = ParseLength(b, n);
		if (v.hd_len < 0) return MSG_ST_BAD_MSG;
		b += v.hd_len;
		oid.resize(v.len);
		i = 0;
		while (i < v.len) oid.at(i++) = *(b++);
			// check for the OID being "end of MIB"
		if (v.len == 1 && oid.at(0) == 0x7F) return 0;
		n -= (v.hd_len + v.len);
		mib[oid].GetAsn1Value(b, n);
	}

	return 0;
}
		

// Collect all the MIB objects that GetNext gives us
void MgtSocket::CollectMib() {
	uint8_t b[130];

	b[0]  = 0x1F;	// "GetNext" request for up to 16 objects
	b[2]  = 6;		// OID tag
	b[3]  = 5;		// length of OID
	b[4]  = 0x28;	// OID = 1.0.62379.1
	b[5]  = 0x83;
	b[6]  = 0xE7;
	b[7]  = 0x2B;
	b[8]  = 1;

	int st;

	while ((st = SendGetNextRequest(b, b[3] + 4)) == MSG_ST_OK) {
		int len = oid.size();
			// stop if length is 1 (can't be a valid OID but can be the 
			//		value returned for "end of MIB"), also if too long 
			//		for the length to fit in <b[3]> (which is unlikely 
			//		with a legitimate MIB)
		if (len > 127 || len < 2) return;
		b[3] = (uint8_t)len;
		int i = 0;
		while (i < len) { b[i+4] = oid.at(i); i += 1; }
	}

	if (st == MSG_ST_NO_SUCH_NAME) return;	// normal termination

	unless (st == MSG_ST_SKT_ERROR) {
		msgs.push_back("Error code " + ToDecimal(st) + " collecting MIB");
		return;
	}

		// here if "sockets" error
	msgs.push_back(("Sockets error " + ToDecimal(errno) + 
								" collecting MIB: ") + strerror(errno));
}


// Collect the name, identity, and the contents of the flash
void MgtSocket::CollectConfig() {
	uint8_t req[131];	// max for header + OID if OID length fits in 1 byte
	uint8_t b[MAX_REPLY_LENGTH];
	uint8_t * p;

	unitName.SetInvalid();
	unitIdentity.SetInvalid();
	unitSerialNumber.SetInvalid();
	unitFirmwareVersion.SetInvalid();
	flash.clear();

		// collect the unitName value
	req[0]  = 0;	// "Get" request
	req[2]  = ASN1_TAG_OID;
	req[3]  = 8;	// length of OID
	req[4]  = 0x28;	// OID = 1.0.62379.1.1.1.1
	req[5]  = 0x83;
	req[6]  = 0xE7;
	req[7]  = 0x2B;
	req[8]  = 1;
	req[9]  = 1;
	req[10] = 1;
	req[11] = 1;
	int len = SendRequest(req, 12, b, MAX_REPLY_LENGTH, 12);
	if (len < 0) return;	// error, probably timeout
	if (len > 0) {
			// we've checked the OID is identical to the one in the 
			//		request message
		p = b + 12;
		len -= 12;
		unitName.GetAsn1Value(p, len);
	}

		// likewise unitIdentity
	req[11] = 4;		// 1.0.62379.1.1.1.4
	len = SendRequest(req, 12, b, MAX_REPLY_LENGTH, 12);
	if (len > 0) {
		p = b + 12;
		len -= 12;
		unitIdentity.GetAsn1Value(p, len);
	}

		// and unitSerialNumber
	req[11] = 7;		// 1.0.62379.1.1.1.7
	len = SendRequest(req, 12, b, MAX_REPLY_LENGTH, 12);
	if (len > 0) {
		p = b + 12;
		len -= 12;
		unitSerialNumber.GetAsn1Value(p, len);
	}

		// and unitFirmwareVersion
	req[11] = 8;		// 1.0.62379.1.1.1.8
	len = SendRequest(req, 12, b, MAX_REPLY_LENGTH, 12);
	if (len > 0) {
		p = b + 12;
		len -= 12;
		unitFirmwareVersion.GetAsn1Value(p, len);
	}

		// now collect the map of the flash; change the request message
	req[0] = 0x1F;	// GetNext with the maximum number of replies
//	req[0] = 0x1E;	// GetNext which finishes in reasonable time with debug print
	req[3] = 10;	// length of OID
	req[10] = 5;	// change OID to 1.0.62379.1.1.5.1.1.3 (swaAccess)
	req[11] = 1;
	req[12] = 1;
	req[13] = 3;

	SoftwareArea a;
	MibObject oid;	// first half of the VarBind
	MibObject v;	// second half of the VarBind
	int i,j,k,n;

	while (true) {
		len = SendRequest(req, req[3] + 4, b, MAX_REPLY_LENGTH, 2);
		if (len < 0) return;	// error, assume NoSuchName

			// now the reply is <len> bytes starting at <b>
		p = b+2;
		len -= 2;	// skip over message header
		while (oid.GetAsn1Value(p, len)) {
			unless (oid.tag == ASN1_TAG_OID) {
				msgs.push_back("OID tag not found where expected");
				return;
			}

				// check the object is in a column of a softwareAreaEntry; 
				//		exit if not, because it will either be the "end of 
				//		MIB" OID or the next part of the MIB beyond the 
				//		softwareArea table, which we're not interested in
			n = oid.size();
			unless (n > 10) return;
			i = 0;
			do {
				unless (oid.at(i) == req[i+4]) return;
				i += 1; } while (i < 9);

			j = oid.at(--n);
			if (j > 127) {
				msgs.push_back("OID finishes in the middle of an arc");
				return;
			}
			i = 7;
			while (k = oid.at(--n), k > 127) { j |= (k & 0x7F) << i; i += 7; }
			unless (n == 9) {
				msgs.push_back("OID doesn't correctly finish with area id");
				return;
			}

				// now <k> is the column number and <j> is the area id
			unless (v.GetAsn1Value(p, len)) return;	// no value present
			unless (v.tag == (k == 8 ? 
							  ASN1_TAG_OCTET_STRING : ASN1_TAG_INTEGER)) {
				msgs.push_back("value of the wrong type");
				return;
			}
			switch (k) {
		case 3:	flash[j].access = v.IntegerValue(); break;
		case 4:	flash[j].status = v.IntegerValue(); break;
		case 5:	flash[j].length = v.IntegerValue(); break;
		case 6:	flash[j].data_type = v.IntegerValue(); break;
		case 7:	flash[j].serial = v.IntegerValue(); break;
		case 8:	flash[j].vn = v.StringValue(); break;

		default:
				msgs.push_back("unexpected column number");
				return;
			}
		}

		if (oid.size() > 127) {
			msgs.push_back("OID is infeasibly long");
			return;
		}
		oid.CopyValue(req+3);	// go get the next tranche
	}
}

/*
// Collect the contents of area <image_area>
void MgtSocket::CollectData() {
	if (image_area < 0) return;
	SoftwareArea a = flash[image_area];
	flash_image.clear();

	uint8_t req[131];	// max for header + OID if OID length fits in 1 byte
	uint8_t b[MAX_REPLY_LENGTH];
	
	req[0]  = 0;	// "Get" request
	req[2]  = ASN1_TAG_OID;
	req[4]  = 0x28;	// OID = 1.0.62379.1.1.5.2.1.4
	req[5]  = 0x83;
	req[6]  = 0xE7;
	req[7]  = 0x2B;
	req[8]  = 1;
	req[9]  = 1;
	req[10] = 5;
	req[11] = 2;
	req[12] = 1;
	req[13] = 4;
	
	uint8_t * index_base;
	if (image_area < 128) {
		req[14] = (uint8_t)image_area;
		index_base = req + 15;
	}
	else {
		req[14] = 0x81;
		req[15] = (uint8_t)image_area & 0x7F;
		index_base = req + 16;
	}

		// read MAX_DATA_LENGTH bytes at a time
	int n = 0;
	do {
		uint8_t * p = index_base;
		AddIndex(p, n);		// address
		int k = a.length - n;
		n += MAX_DATA_LENGTH;
		AddIndex(p, k < MAX_DATA_LENGTH ? k : MAX_DATA_LENGTH);	// length to read
		req[3] = (uint8_t)(p - (req + 4));	// length of OID

		int len = SendRequest(req, req[3] + 4, b, MAX_REPLY_LENGTH, req[3] + 4);
		if (len < 0) return;
		
			// now the reply is <len> bytes starting at <b>
		unless ((b[0] & 15) == 0) return;	// error
		MibObject v;
		k = 4 + b[3];	// offset to start of value
		p = b + k;
		len -= k;
		unless (v.GetAsn1Value(p, len) && len == 0) return;	// error
		flash_image.insert(flash_image.end(), v.begin(), v.end());
	} while (n < a.length);
}
*/


// return value for key "software <t> 2" in product file <pf_text>, or an 
//		empty string if not found
// +++ ought to have further parameters which are the expected product code
std::string MgtSocket::SoftwarePath(std::vector<std::string> pf_text, int t)
{
		// <buf> is used as a line buffer; we ignore anything beyond the 
		//		first 100 characters
	char buf[101];
	buf[100] = 0;
	int i,j,k;
	char ch;
	std::string r;	// for the result; initialised to an empty string
	k = 0;
	do {
		std::string& s = pf_text.at(k);
		size_t n = s.find('=');
		if (n > 99) continue; // including if it's std::string::npos

			// here if the line has an '='
			// there are <n> characters before the '='
		strncpy(buf, s.c_str(), n + 1);	// including the '='
		char * q = buf + n;
		while (q > buf) {
			q -= 1;
			*q = tolower(*q);
		}

			// now <buf> holds the "keyword" part of the line in lowercase; 
			//		the last character is the '='
		if (sscanf(buf, " software %x %x %c", &i, &j, &ch) == 3 && 
										i == t && j == 2 && ch == '=') {
			j = s.size();
			do n += 1; while (n < j && isspace(s[n]));
			if (n >= j) return r;	// nothing but white space beyond the '='
			r = s.substr(n);
				// <r> starts with the first non-space character after the 
				//		'='; must now trim white space from the righthand 
				//		end, noting that there is at least one non-space 
				//		character
			n = r.size();
			do n -= 1; while (isspace(r[n]));
			r.resize(n+1);
			return r;
		}
	} while (++k < pf_text.size());

		// here if not found, with <r> still an empty string
	return r;
}


// return decimal value for keyword <key> in product file <pf_text>, 
//		or LLONG_MIN if not found
// if the keyword appears more than once, returns the last of them
int64_t MgtSocket::FindNumericValue(std::vector<std::string> pf_text, 
														std::string key)
{
	std::string fmt = ' ' + key + " = %lld";
	int64_t r = LLONG_MIN;	// for the result
	int k = pf_text.size();
	while (--k >= 0) if (sscanf(pf_text.at(k).c_str(), 
										fmt.c_str(), &r) == 1) return r;

		// here if not found, with <r> still LLONG_MIN
	return r;
}


// return value for keyword <key> in product file <pf_text>, or an 
//		empty string if not found
// result is truncated to 100 characters
std::string MgtSocket::FindStringValue(std::vector<std::string> pf_text, 
														std::string key)
{
		// <buf> is used as a line buffer; we ignore anything beyond the 
		//		first 100 characters
	char buf[101];
	std::string fmt = ' ' + key + " = %100s";
	std::string r;	// for the result; initialised to an empty string
	int k = 0;
	do if (sscanf(pf_text.at(k).c_str(), fmt.c_str(), buf) == 1) {
		r = buf;
		return r;
	} while (++k < pf_text.size());

		// here if not found, with <r> still an empty string
	return r;
}


// upload boot FPGA image to the managed unit
// +++ TEMP this is somewhat of a kludge, originally for the MicroBoard version 
//			and adapted for an Aubergine with the fixed address 192.168.4.11 on 
//			port 3 
// calls <CollectConfig> on entry; configuration may be out-of-date on exit
void MgtSocket::UploadBootLogic()
{
	CollectConfig();
	if (flash.empty()) {
		AlertUser("No map of flash available");
		return;
	}

	FlashMap::iterator p = flash.begin();
	unless (p->first == 0) {
			// maybe the managed unit doesn't think we are entitled to even 
			//		know the boot area exists
		AlertUser("Boot logic area not included in map");
		return;
	}
	switch (p->second.status) {
case AREA_STATUS_EMPTY:	break;

case AREA_STATUS_INVALID:
case AREA_STATUS_VALID:
		if (p->second.access < AREA_ACCESS_ERASE) {
			AlertUser("can't rewrite boot area");
			return;
		}
		EraseArea(p);
		return;

case AREA_STATUS_WRITING:	// left in "writing" state last time?
		unless (AskUser("Change boot area to 'valid'?")) return;
		SetAreaState(p, AREA_STATUS_VALID);
		return;

default:
		AlertUser("Boot area is in state " + ToDecimal(p->second.status));
		return;
	}

		// here if area 0 is empty, to collect the image we want to 
		//		upload
	if (boot_pf_path.empty()) boot_pf_path = AskProductFilePath();
	std::vector<std::string> pf_text = ReadTextFile(boot_pf_path);
	if (pf_text.empty()) {
		AlertUser("product file " + boot_pf_path + " not found (or is empty)");
		return;
	}

	std::string f_name = SoftwarePath(pf_text, AREA_TYPE_BOOT_LOGIC);
	if (f_name.empty()) {
		AlertUser("boot logic not found in " + boot_pf_path);
		return;
	}

	int64_t serial = FindNumericValue(pf_text, "last_serial");
	if (serial < 0) serial = 0;

		// advance to the next serial number
	serial += 1;
	while(true) {
		switch (AskUserIncDec("Serial number " + ToDecimal(serial))) {
case -1:
			serial -= 1;
			continue;
case 0:
			break;	// NB only out of the switch, not the loop
case 1:
			serial += 1;
			continue;
default:
			AlertUser("unexpected reply to query; action aborted");
			return;
		}
		break;
	}

		// +++ TO DO: write the chosen value back to the file
	int k = pf_text.size();
	while (--k >= 0) if (pf_text.at(k).substr(0, 13) == 
								"last_serial\t=") goto have_ls_line;
	k = pf_text.size();
	pf_text.resize(k+1);
have_ls_line:
		// here with <k> the line number in which to write the new 
		//		"last_serial" line, either where the current one is 
		//		or a new line added to the end of the file
		// NB we expect there to be no white space before the keyword 
		//		and just a single tab character between the keyword 
		//		and the '=', so may fail to recognise the line that 
		//		was used, in which case we'll add another one at the 
		//		end of the file which will over-ride it (unless 
		//		there's another one earlier in the file which we do 
		//		recognise, in which case the update becomes invisible)
	pf_text.at(k) = "last_serial\t= " + ToDecimal(serial);
	unless (WriteFile(boot_pf_path, pf_text)) 
				AlertUser("Failed to rewrite product file " + pf_path);

		// read the FPGA logic
	int i,j;
	j = boot_pf_path.rfind('/');		// j -> char before filename
	std::string base_f_path;
	unless (j == std::string::npos) base_f_path = boot_pf_path.substr(0, j + 1);

	BytesOnHeap logic = ReadBinaryFile(base_f_path + f_name);
	if (logic.p == NULL) {
		AlertUser("Failed to read " + 
							base_f_path + f_name + " (or file is empty)");
		return;
	}

		// look for the sync word: this allows us to skip over the Xilinx 
		//		header without needing to know what it contains (assuming 
		//		it can't contain a sync word preceded by at least 8 bytes 
		//		of FF, which is what we look for)
		// this should also work with the 62379-1 format, but see kludge 
		//		alert below
	uint8_t * fpga_base = logic.p;
	i = 0;
		// looking at a dump of the file, the header doesn't seem to be 
		//		anywhere near 2K bytes so we just search the first 2K 
		//		bytes for the sync word
	do {
		uint8_t byte = *fpga_base++;
		switch (i) {
	default:
			if (byte == 0xFF) i += 1;
			else i = 0;
			continue;

	case 8:	if (byte == 0xAA) { i = 9; continue; }
			unless (byte == 0xFF) break;
			continue;

	case 9:	if (byte == 0x99) { i = 10; continue; }
			break;

	case 10:
			if (byte == 0x55) { i = 11; continue; }
			break;

	case 11:
			if (byte == 0x66) i = 12;
		}
		break;
	} until (fpga_base > logic.p + 2048);

	unless (i == 12) {
		AlertUser("No sync word found in " + base_f_path + f_name);
		delete [] logic.p;
		return;
	}
		// move pointer back to include the sync word and 12 bytes before it
	fpga_base -= 16;
	logic.len -= (fpga_base - logic.p);
		// overwrite the first four bytes with the version number; the FPGA 
		//		image in flash will begin with the area header, version 
		//		number, product code, 8 bytes of FF, and the sync word; the 
		//		main configuration will begin in the same place that Xilinx 
		//		put it, i.e. byte address 0x44; we need to ensure that the 
		//		version number and product code can't include a sync word, 
		//		but that just needs all the components other than the beta 
		//		number to be less than 85
		// leave it as all-Fs if can't decode the filename
	char buf[32];
	buf[31] = 0;
	int vn[4];
	i = f_name.rfind('~');
	unless (i == std::string::npos) {
		strncpy(buf, f_name.substr(i+1).c_str(), 31); // part after the '~'
		vn[3] = 0;	// in case no "BETA"
		if (sscanf(buf, "%d-%d-%d-BETA-%d", 
								   &vn[0], &vn[1], &vn[2], &vn[3]) >= 3) {
			fpga_base[0] = (uint8_t)(vn[3]);	// beta number comes first
			fpga_base[1] = (uint8_t)(vn[0]);
			fpga_base[2] = (uint8_t)(vn[1]);
			fpga_base[3] = (uint8_t)(vn[2]);
		}
	}

		// +++ KLUDGE: budge the first few words up to remove the setting 
		//		of GENERAL5 and insert the product code in the gap; ought 
		//		to do this as part of the process of generating a 62379-1 
		//		format file; might be better to put the boot code in the 
		//		same product file as the normal code, though then need to 
		//		find a way to stop standard management terminals trying to 
		//		update it in the field (e.g. have separate product file 
		//		that omits it, or encourage management terminals not to 
		//		try to update areas that are read-only)
	unless (fpga_base[36] == 0x32 && fpga_base[37] == 0xE1) {
		AlertUser("Setting of GENERAL5 not where expected in " + 
													base_f_path + f_name);
		delete [] logic.p;
		return;
	}
	memmove(fpga_base+8, fpga_base+4, 32);
		// +++ KLUDGE: fixed product code 3.0.0.0
	fpga_base[4] = 0x03;
	fpga_base[5] = 0x00;
	fpga_base[6] = 0x00;
	fpga_base[7] = 0x00;

		// write the unit id over the remaining 8 FFs
		// +++ KLUDGE: assumes (but doesn't check) that <serial> is 
		//		less than 64K
	fpga_base[8] = 0x00;
	fpga_base[9] = 0x90;
	fpga_base[10] = 0xA8;
	fpga_base[11] = 0x99;
	fpga_base[12] = 0x00;
	fpga_base[13] = 0x00;
	fpga_base[14] = (char)(serial >> 8);
	fpga_base[15] = (char)serial;
	
		// now we have <logic.len> bytes starting at <fpga_base>, to be 
		//		written to the start of the area
		// <logic.p> still points to the beginning of the buffer into 
		//		which we read the file
	if (p->second.length < logic.len) {
			// area 0 too small: see about erasing some more
		delete [] logic.p;
		if (flash.size() < 2) {
			AlertUser("flash appears to be too small");
			return;
		}
		p++;
		switch (p->second.status) {
				// NB don't expect two adjacent empty areas
	case AREA_STATUS_INVALID:
	case AREA_STATUS_VALID:
			EraseArea(p);
			return;
		}

		AlertUser("Boot area too small, next is in state " + 
											ToDecimal(p->second.status));
		return;
	}

	UploadImage(0, AREA_TYPE_BOOT_LOGIC, 0, logic.len, fpga_base);
	delete [] logic.p;
}


// upload data to area <area>
// sets status to "writing", type to <new_type>, serial number to <serial>, 
//		and length to <n>; uploads <n> bytes starting at <p>, then sets 
//		status to "valid"
// if any error calls AlertUser and exits
#define MAX_DATA_LENGTH	  1200	// OK for IPv6, Flexilink2 bg, Ethernet
void MgtSocket::UploadImage(int area, uint8_t new_type, uint8_t serial, 
													int n, uint8_t * p)
{
		// message buffers: <req> allows 100 bytes for headers + OID
	uint8_t req[MAX_DATA_LENGTH + 100];	// for the request
	uint8_t b[MAX_REPLY_LENGTH];		// for the reply

		// change the area to "writing" status
	req[0]  = 0x30;	// "Set" request
	req[2]  = ASN1_TAG_OID;
	req[4]  = 0x28;	// OID = 1.0.62379.1.1.5.1.1.4.0
	req[5]  = 0x83;
	req[6]  = 0xE7;
	req[7]  = 0x2B;
	req[8]  = 1;
	req[9]  = 1;
	req[10] = 5;
	req[11] = 1;
	req[12] = 1;
	req[13] = 4;
	
	uint8_t * mp = req + 14;
	AddIndex(mp, area);
	uint8_t * p_value = mp;	// points to "tag" byte of value
	req[3] = (p_value - req) - 4;
	*mp++ = ASN1_TAG_INTEGER;
	*mp++ = 1;
	*mp++ = AREA_STATUS_WRITING;

	int j = (mp - req);	// message length
	int len = SendRequest(req, j, b, MAX_REPLY_LENGTH, j);
	unless (len > 0) {
		AlertUser("could not set area " + ToDecimal(area) + " for writing");
		return;
	}

		// set the type; <j> is still the message length
	req[13] = 6;
	p_value[2] = new_type;

	len = SendRequest(req, j, b, MAX_REPLY_LENGTH, j);
	unless (len > 0) {
		AlertUser("could not set type of area " + ToDecimal(area));
		return;
	}

		// set the serial number, similarly
	req[13] = 7;
	p_value[2] = serial;

	len = SendRequest(req, j, b, MAX_REPLY_LENGTH, j);
	unless (len > 0) {
		AlertUser("could not set serial number for area " + ToDecimal(area));
		return;
	}

	req[13] = 5;
	mp = p_value + 2;	// first byte of "value" part of value (1 byte length)
	if (n >= (1 << 16)) *mp++ = (uint8_t)(n >> 16);
	if (n >= (1 << 8)) *mp++ = (uint8_t)(n >> 8);
	*mp++ = (uint8_t)n;
	p_value[1] = (mp - p_value) - 2;	// length
	int k = (mp - req);

		// NB length will get rounded up
	len = SendRequest(req, k, b, MAX_REPLY_LENGTH, j-2);
	k = 0;
	if (len >= j) {
		mp = p_value + 1;
		j = *mp++;
		while (j > 0) { k = (k << 8) | *mp++; j -= 1; }
	}
	unless (k >= n) {
		AlertUser("could not set length of area " + ToDecimal(area));
		return;
	}

		// now write all the data
		// OID = 1.0.62379.1.1.5.2.1.4.area.offset.length
	req[11] = 2;
	req[13] = 4;
	k = 0;				// offset
	do {
			// set <j> to the number of bytes to send this time
		j = n < MAX_DATA_LENGTH ? n : MAX_DATA_LENGTH;
		mp = p_value;
		AddIndex(mp, k);
		AddIndex(mp, j);
		req[3] = (mp - req) - 4;
		*mp++ = ASN1_TAG_OCTET_STRING;
			// length will mostly be MAX_DATA_LENGTH (which is definitely 
			//		> 255 and < 64K) so always use the 16-bit format
		*mp++ = 0x82;
		*mp++ = (uint8_t)(j >> 8);
		*mp++ = (uint8_t)j;
		memcpy(mp, p, j);
		p += j;
		k += j;
		n -= MAX_DATA_LENGTH;

		j += (mp - req);	// message length
		len = SendRequest(req, j, b, MAX_REPLY_LENGTH, j);
		unless (len > 0) {
			AlertUser("error writing data");
			return;
		}
	} while (n > 0);

		// set the area to "valid"
	req[3] = (p_value - req) - 4;
	req[11] = 1;
	p_value[0] = ASN1_TAG_INTEGER;
	p_value[1] = 1;
	p_value[2] = AREA_STATUS_VALID;
	j = (p_value - req) + 3;

	len = SendRequest(req, j, b, MAX_REPLY_LENGTH, j);
	unless (len > 0) AlertUser("could not set area " + 
											ToDecimal(area) + " to 'valid'");
}


// request change to state <n> for the area to which <p> points
void MgtSocket::SetAreaState(FlashMap::iterator p, uint8_t n)
{
	uint8_t req[25];
	uint8_t b[30];
	
		// change the area to "erase" status
	req[0]  = 0x30;		// "Set" request
	req[2]  = ASN1_TAG_OID;
	req[4]  = 0x28;		// OID = 1.0.62379.1.1.5.1.1.4.n
	req[5]  = 0x83;
	req[6]  = 0xE7;
	req[7]  = 0x2B;
	req[8]  = 1;
	req[9]  = 1;
	req[10] = 5;
	req[11] = 1;
	req[12] = 1;
	req[13] = 4;
	
	uint8_t * mp = req + 14;
	AddIndex(mp, p->first);
	req[3] = (mp - req) - 4;
	*mp++ = ASN1_TAG_INTEGER;
	*mp++ = 1;
	*mp++ = n;
	int j = (mp - req);	// message length
	int len = SendRequest(req, j, b, 30, j);
	if (len > 0) p->second.status = n;
	else AlertUser("could not set area " + ToDecimal(p->first) + 
									   " to state " + ToDecimal(n));
}


// upload boot VM code to the managed unit
// +++ TEMP this is somewhat of a kludge, for the MicroBoard version
void MgtSocket::UploadBootCode()
{
	CollectConfig();
	if (flash.empty()) {
		AlertUser("No map of flash available");
		return;
	}

	FlashMap::iterator p = flash.begin();
	unless (p->first == 0) {
			// maybe the managed unit doesn't think we are entitled to even 
			//		know the boot area exists
		AlertUser("Boot area not included in map");
		return;
	}
	unless (p->second.status == AREA_STATUS_VALID && 
							p->second.data_type == AREA_TYPE_BOOT_LOGIC) {
		AlertUser("FPGA boot image must be written first");
		return;
	}
	p++;

	switch (p->second.status) {
case AREA_STATUS_EMPTY:	break;

case AREA_STATUS_INVALID:
case AREA_STATUS_VALID:
/*		if (p->second.access < AREA_ACCESS_ERASE) {
			AlertUser("Can't rewrite area" + ToDecimal(p->first));
			return;
		}
*/		EraseArea(p);
		return;

case AREA_STATUS_WRITING:	// left in "writing" state last time?
		if (AskUser("Change area " + ToDecimal(p->first) + " to 'valid'?")) 
										SetAreaState(p, AREA_STATUS_VALID);
		return;

default:
		AlertUser("Area " + ToDecimal(p->first) + 
							" is in state " + ToDecimal(p->second.status));
		return;
	}

		// here if the second area in the map is empty, to collect the code 
		//		we want to upload
	if (boot_pf_path.empty()) boot_pf_path = AskProductFilePath();
	std::vector<std::string> pf_text = ReadTextFile(boot_pf_path);
	if (pf_text.empty()) {
		AlertUser("product file " + boot_pf_path + " not found (or is empty)");
		return;
	}
	
	std::string f_name = SoftwarePath(pf_text, AREA_TYPE_BOOT_VM_CODE);
	if (f_name.empty()) {
		AlertUser("boot logic not found in " + boot_pf_path);
		return;
	}

		// read the VM code
	int j,k;
	j = boot_pf_path.rfind('/');		// j -> char before filename
	std::string base_f_path;
	unless (j == std::string::npos) base_f_path = boot_pf_path.substr(0, j + 1);

	BytesOnHeap vm_code = ReadBinaryFile(base_f_path + f_name);
	if (vm_code.p == NULL) {
		AlertUser("Failed to read " + 
							base_f_path + f_name + " (or file is empty)");
		return;
	}

		// check the header in the binary file and set <q> pointing to the 
		//		carriage return that terminates it and <k> to the data length
	uint8_t * q = (uint8_t *)(memchr(vm_code.p, 0x0D, vm_code.len));
	if (q == NULL || (q - vm_code.p) < 47 || 
			memcmp("#62379.iec.ch::memoryimagebinary:ninetiles.com#", 
													vm_code.p, 47) != 0) {
		AlertUser("First line of " + base_f_path + f_name + 
										" not correct for VM code file");
		delete [] vm_code.p;
		return;
	}
	q += 1;
	unless (sscanf((char *)q, " length = %x", &k) == 1) {
		AlertUser("'Length' not found in " + base_f_path + f_name);
		delete [] vm_code.p;
		return;
	}
	unless ((k & 3) == 0) {
		AlertUser(base_f_path + f_name + " not a whole number of words");
		delete [] vm_code.p;
		return;
	}
	q = (uint8_t *)(memchr(q, 0x0D, vm_code.len - (q - vm_code.p)));
	if (q == NULL) {
		AlertUser("second line of " + base_f_path + f_name + " not terminated");
		delete [] vm_code.p;
		return;
	}
	q += 1;
	unless (vm_code.len - (q - vm_code.p) == k) {
		AlertUser(base_f_path + f_name + " wrong length");
		delete [] vm_code.p;
		return;
	}

	if (k > p->second.length) {
			// doesn't fit in the area we have; abandon what we've done so far 
			//		and see about erasing some more
		delete [] vm_code.p;
		if (flash.size() < 3) {
			AlertUser("flash appears to be too small");
			return;
		}
		p++;
		switch (p->second.status) {
				// NB don't expect two adjacent empty areas
	case AREA_STATUS_INVALID:
	case AREA_STATUS_VALID:
			EraseArea(p);
			return;
		}

		AlertUser("Area " + ToDecimal(p->first) + " too small, "
						"next is in state " + ToDecimal(p->second.status));
		return;
	}

		// +++ ought to check the checksum too

	UploadImage(p->first, AREA_TYPE_BOOT_VM_CODE, 0, k, q);
	delete [] vm_code.p;
}


// compare flash contents against product file, and upload as required
// calls <CollectConfig> on entry; configuration may be out-of-date on exit
// +++ Kludge alert: this version is Nine Tiles specific, if not MicroBoard 
//		specific, because it makes assumptions about area numbers; it would 
//		be better to read the product file first, to find all the "software" 
//		lines, and then look for the area numbers they report; also, we ought 
//		to look inside the file to extract the version number from the first 
//		line rather than assume we can deduce it from the filename; and 
//		currently we expect logic in a Xilix-format .bit file
void MgtSocket::UpdateSoftware()
{
	CollectConfig();
	if (flash.empty()) {
		AlertUser("No map of flash available");
		return;
	}

	FlashMap::iterator p = flash.begin();
	FlashMap::iterator logic_area[16];
	FlashMap::iterator vm_code_area[16];
	FlashMap::iterator * q;
		// initialise the maps to the iterator equivalent of NULL
	int i = 16;
	do {
		logic_area[--i] = flash.end();
		vm_code_area[i] = flash.end();
	} while (i > 0);

	int j,k;
	do if (p->second.status == AREA_STATUS_VALID) {
		switch (p->second.data_type) {
default:	continue;

case AREA_TYPE_LOGIC:
			q = logic_area;
			break;

case AREA_TYPE_VM_CODE:
			q = vm_code_area;
		}

			// now <q> is the array for the relevant area type
		i = p->second.serial;
		if ((i >= 16 || q[i] != flash.end()) && AskUser("Erase area " + 
					ToDecimal(p->first) + " with invalid serial "
								"number " + ToDecimal(i) + '?')) {
			SetAreaState(p, AREA_STATUS_ERASING);
			return;
		}
		q[i] = p;
	} until (p++ == flash.end());

		// now we have maps of the serial numbers of all the "normal" code 
		//		areas in the flash
		// check the serial numbers in each; we set <last_ser[0]> to the 
		//		"latest" logic serial number, -1 if none, and <last_ser[1]> 
		//		similarly for the VM code
	int last_ser[2];
	last_ser[0] = -1;
	last_ser[1] = -1;
	i = 0;
	q = logic_area;
	do {
		j = 15;
		p = q[0];
		do {
				// <p> is <q[j+1]>; look for <p> being NULL and <q[j]> not
			if (q[j] != flash.end() && p == flash.end()) {
					// <j> is a "latest" serial number
				if (last_ser[i] >= 0) {
						// it's not the only "latest": offer to erase each 
						//		of them, and if the offers are declined 
						//		take the smaller serial number
					if (AskUser("Erase area " + ToDecimal(q[j]->first) + 
										" with duplicate serial number " + 
													ToDecimal(j) + '?')) {
						SetAreaState(q[j], AREA_STATUS_ERASING);
						return;
					}
					k = last_ser[i];
					if (AskUser("Erase area " + ToDecimal(q[k]->first) + 
										" with duplicate serial number " + 
													ToDecimal(k) + '?')) {
						SetAreaState(q[k], AREA_STATUS_ERASING);
						return;
					}
				}
				last_ser[i] = j;
			}
			p = q[j];
			j -= 1;
		} while (j >= 0);

		i += 1;
		q = vm_code_area;
	} while (i < 2);

		// now look in the product file to see what the latest versions are
		// +++ see note at the top of this routine
	if (pf_path.empty()) pf_path = AskProductFilePath();
	std::vector<std::string> pf_text = ReadTextFile(pf_path);
	if (pf_text.empty()) {
		AlertUser("product file " + pf_path + " not found (or is empty)");
		return;
	}
	j = pf_path.rfind('/');		// j -> char before filename
	std::string base_f_path;
	unless (j == std::string::npos) base_f_path = pf_path.substr(0, j + 1);
	
	std::string logic_f_name = SoftwarePath(pf_text, AREA_TYPE_LOGIC);
	if (logic_f_name.empty()) {
		AlertUser("logic not found in " + pf_path);
		return;
	}

	std::string vm_f_name = SoftwarePath(pf_text, AREA_TYPE_VM_CODE);
	if (vm_f_name.empty()) {
		AlertUser("VM code not found in " + pf_path);
		return;
	}

		// see whether the logic is up-to-date
		// set <pf_vn> to the version number as indicated by the product 
		//		file
		// +++ ought to get that by reading the logic file
	std::string pf_vn = logic_f_name.substr(logic_f_name.rfind('~') + 1);
	pf_vn = pf_vn.substr(0, pf_vn.find('.'));
	char buf[32];
	buf[31] = 0;
	int vn[4];
	if ((last_ser[0] < 0 || pf_vn != logic_area[last_ser[0]]->second.vn) && 
			AskUser(((last_ser[0] < 0) ? "upload logic " : ("replace "
							"logic " + logic_area[last_ser[0]]->second.vn + 
									" with ")) + logic_f_name + '?')) {
		k = (last_ser[0] + 1) & 15;

			// upload new logic to serial number <k>
			// <k> is 0 if there was no previous logic, also if all serial 
			//		numbers were in use
		BytesOnHeap logic = ReadBinaryFile(base_f_path + logic_f_name);
		if (logic.p == NULL) {
			AlertUser("Failed to read " + 
						base_f_path + logic_f_name + " (or file is empty)");
			return;
		}

			// look for the sync word, in the same way as in <UploadBootLogic>
			// this should also work with the 62379-1 format, but see kludge 
			//		alerts below and at the top of this routine
		uint8_t * fpga_base = logic.p;
		i = 0;
		do {
			uint8_t byte = *fpga_base++;
			switch (i) {
	default:	if (byte == 0xFF) i += 1;
				else i = 0;
				continue;

	case 8:		if (byte == 0xAA) { i = 9; continue; }
				unless (byte == 0xFF) break;
				continue;

	case 9:		if (byte == 0x99) { i = 10; continue; }
				break;

	case 10:	if (byte == 0x55) { i = 11; continue; }
				break;

	case 11:	if (byte == 0x66) i = 12;
			}
			break;
		} until (fpga_base > logic.p + 2048);

		unless (i == 12) {
			AlertUser("No sync word found in " + base_f_path + logic_f_name);
			delete [] logic.p;
			return;
		}
			// +++ KLUDGE: move the pointer on to the first of the FFs that 
			//		precede the second sync word
		fpga_base += 48;
		logic.len -= (fpga_base - logic.p);

			// now we know the length that will be required
		j = FindFreeArea(logic.len, logic_area, k);
		if (j < 0) {
				// failed to find a free area; user will already have been told
			delete [] logic.p;
			return;
		}

			// overwrite the first four bytes with the version number; the FPGA 
			//		image in flash will begin with the area header, version 
			//		number, product code, 8 bytes of FF, and the sync word; we 
			//		need to ensure that the version number and product code 
			//		can't include a sync word, but that just needs all the 
			//		components other than the beta number to be less than 85
			// leave it as all-Fs if can't decode the filename
		strncpy(buf, pf_vn.c_str(), 31);
		vn[3] = 0;	// in case no "BETA"
		if (sscanf(buf, "%d-%d-%d-BETA-%d", 
								   &vn[0], &vn[1], &vn[2], &vn[3]) >= 3) {
			fpga_base[0] = (uint8_t)(vn[3]);	// beta number comes first
			fpga_base[1] = (uint8_t)(vn[0]);
			fpga_base[2] = (uint8_t)(vn[1]);
			fpga_base[3] = (uint8_t)(vn[2]);
		}

			// +++ KLUDGE: product code is fixed at 3.0.2.0; ought to get it 
			//		from the product file
		fpga_base[4] = 0x03;
		fpga_base[5] = 0x00;
		fpga_base[6] = 0x02;
		fpga_base[7] = 0x00;

			// now we have <logic.len> bytes starting at <fpga_base>, to be 
			//		written to area <j> with serial number <k>
			// <logic.p> still points to the beginning of the buffer into 
			//		which we read the file
		UploadImage(j, AREA_TYPE_LOGIC, k, logic.len, fpga_base);
		delete [] logic.p;
	}

		// see whether the VM code is up-to-date
		// set <pf_vn> to the version number as indicated by the product 
		//		file
		// +++ ought to get that by reading the code file
	pf_vn = vm_f_name.substr(vm_f_name.rfind('~') + 1);
	pf_vn = pf_vn.substr(0, pf_vn.find('.'));
	if ((last_ser[1] < 0 || pf_vn != vm_code_area[last_ser[1]]->second.vn) && 
			AskUser(((last_ser[1] < 0) ? "upload VM code " : ("replace "
							"VM code " + vm_code_area[last_ser[1]]->second.vn + 
										" with ")) + vm_f_name + '?')) {
		k = (last_ser[1] + 1) & 15;

			// upload new VM code to serial number <k>
			// read the VM code
		BytesOnHeap vm_code = ReadBinaryFile(base_f_path + vm_f_name);
		if (vm_code.p == NULL) {
			AlertUser("Failed to read " + 
						base_f_path + vm_f_name + " (or file is empty)");
			return;
		}

			// check the header in the binary file and set <b> pointing to the 
			//		carriage return that terminates it and <i> to the data length
		uint8_t * b = (uint8_t *)(memchr(vm_code.p, 0x0D, vm_code.len));
		if (b == NULL || (b - vm_code.p) < 47 || 
				memcmp("#62379.iec.ch::memoryimagebinary:ninetiles.com#", 
														vm_code.p, 47) != 0) {
			AlertUser("First line of " + base_f_path + vm_f_name + 
											" not correct for VM code file");
			delete [] vm_code.p;
			return;
		}
		b += 1;
		unless (sscanf((char *)b, " length = %x", &i) == 1) {
			AlertUser("'Length' not found in " + base_f_path + vm_f_name);
			delete [] vm_code.p;
			return;
		}
		unless ((i & 3) == 0) {
			AlertUser(base_f_path + vm_f_name + " not a whole number of words");
			delete [] vm_code.p;
			return;
		}
		b = (uint8_t *)(memchr(b, 0x0D, vm_code.len - (b - vm_code.p)));
		if (b == NULL) {
			AlertUser("second line of " + base_f_path + vm_f_name + 
														" not terminated");
			delete [] vm_code.p;
			return;
		}
		b += 1;
		unless (vm_code.len - (b - vm_code.p) == i) {
			AlertUser(base_f_path + vm_f_name + " wrong length");
			delete [] vm_code.p;
			return;
		}

			// +++ ought to check the checksum too

			// now we know the length that will be required
		j = FindFreeArea(i, vm_code_area, k);
		if (j < 0) {
				// failed to find a free area; user will already have been told
			delete [] vm_code.p;
			return;
		}

		UploadImage(j, AREA_TYPE_VM_CODE, k, i, b);
		delete [] vm_code.p;
	}
}


// return the id of a free area with length at least <len>
// <ser> is the serial number for the new data and <q> points to a map 
//		of the 16 possible serial numbers; checks that serial number 
//		<ser> + 1 mod 16 isn't in use
// assumes either <ser> is free or all serial numbers are in use (which 
//		should be true of the way UpdateSoftware chooses serial numbers)
// returns a negative value (in practice -1) if can't find an area, after 
//		informing the user and possibly setting in train freeing of some 
//		space
// won't free up an area without asking the user, mostly because we don't 
//		have a good way to know when the "erase" operation is complete
int MgtSocket::FindFreeArea(int len, FlashMap::iterator * q, int ser)
{
	int i = (ser + 1) & 15;
	unless (q[i] == flash.end()) {
			// the next entry after the requested area is in use, so 
			//		the new image won't be "latest"; this can happen 
			//		legitimately if there are 15 versions in the flash
			// offer to erase it; note that in the case where all 16 
			//		serial numbers are used <ser> will be 0 and we'll 
			//		erase serial number 1 and then come here again 
			//		next time with <ser> = 1 to erase serial number 2
		if (AskUser("Serial number " + ToDecimal(i) + 
				" in use: erase area " + ToDecimal(q[i]->first) + 
						'?')) SetAreaState(q[i], AREA_STATUS_ERASING);
		return -1;
	}

	int j = (ser + 2) & 15;
	unless (q[j] == flash.end()) {
			// the next-but-one entry after the requested area is in use, 
			//		so there must be 14 versions already in the flash, 
			//		which seems excessive
			// offer to erase it; we can't immediately go on to write 
			//		the new data because we need to allow time for the 
			//		erase to complete
		if (AskUser("Fourteen serial numbers already in use: "
						"erase area " + ToDecimal(q[j]->first) + 
						'?')) SetAreaState(q[j], AREA_STATUS_ERASING);
		return -1;
	}

	FlashMap::iterator p = flash.begin();
	until (p == flash.end()) {
		if (p->second.status == AREA_STATUS_EMPTY && 
							p->second.length >= len) return p->first;
		p++;
	}

		// here if no free areas big enough
		// <i> is the next serial number after <ser>
		// we assume the one before <ser> is the one that's running now, 
		//		so we shouldn't try to erase it
	j = (ser - 2) & 15;
	do { i = (i + 1) & 15; p = q[i]; } while (p == flash.end() && i != j);
	if (p != flash.end() && AskUser("Erase area " + 
										ToDecimal(p->first) + '?')) {
		SetAreaState(p, AREA_STATUS_ERASING);
		return -1;
	}
		// here if no old versions, or user didn't want to erase the 
		//		oldest, to look for invalid areas
	p = flash.begin();
	until (p == flash.end()) {
		if (p->second.status == AREA_STATUS_INVALID && 
				AskUser("Erase area " + ToDecimal(p->first) + '?')) {
			SetAreaState(p, AREA_STATUS_ERASING);
			return -1;
		}
		p++;
	}
		// else give up
		// +++ ought to go looking for old versions of other area types 
		//		before giving up
	AlertUser("Not enough free space in flash");
	return -1;
}


// send request message from <t_buf>, total size (including header) <t_len> 
//		bytes, and collect the reply into <r_buf> which is <r_len> bytes
// <r_buf> must be separate from <t_buf>, because we may need to 
//		retransmit the request after having received a packet which turns 
//		out not to be the reply
// fills in the serial number in <t_buf>; all else must be already there
// repeats request up to 5 times if no reply
// returns the number of bytes in <r_buf>, or adds a line containing an 
//		error message to <msgs> and returns one of the negative MSG_ST_ 
//		codes
// checks the first <chk_len> bytes of the reply for status = 0 and from 
//		the 3rd byte onwards that the reply is identical to the request, 
//		e.g. set <chk_len> to 0 to check nothing, to the length up to 
//		the end of the OID to check it's as expected, to the whole length 
//		of a write to check the value written in the MIB is identical to 
//		that requested; note that if the first 4 bits and the serial 
//		number don't match the message is assumed not to be the one we're 
//		waiting for
int MgtSocket::SendRequest(uint8_t * t_buf, int t_len, 
								uint8_t * r_buf, int r_len, int chk_len)
{
	unless (IsConnected()) {
		msgs.push_back("Socket not connected");
		return MSG_ST_NOT_CONN;
	}
	int retry_ct = 5;
	bool send_req = true;	// whether to send the request
	int n;
	t_buf[1] = next_serial;
	if (next_serial == 255) next_serial = 0;
	next_serial += 1;
	while (true) {
		if (send_req) {
			retry_ct -= 1;
			SaveMessage(t_buf, t_len, (send(handle, 
								t_buf, t_len, 0) == t_len ? 't' : 'e'));
		}

		n = recv(handle, r_buf, r_len, 0);
		if (n < 0) {
				// check for timeout; note that the documentation says 
				//		this could be EWOULDBLOCK or EAGAIN but in the 
				//		Apple version of errno.h, which seems to be 
				//		derived from the Berkeley version, they're the 
				//		same code
			unless (errno == EWOULDBLOCK) {
				msgs.push_back(("Sockets error " + ToDecimal(errno) + 
											": ") + strerror(errno));
				return MSG_ST_SKT_ERROR;
			}
				// else assume the request got lost so send it again
			if (retry_ct < 0) {
				msgs.push_back("No reply received");
				return MSG_ST_TIMED_OUT;
			}
			send_req = true;
			continue;
		}
		SaveMessage(r_buf, n, 'r');
		if (n >= 2 && r_buf[1] == t_buf[1] && 
								((r_buf[0] ^ t_buf[0]) & 0xF0) == 0x80) {
			if (chk_len == 0) return n;
			if (r_buf[0] & 0x0F) {
				msgs.push_back("Nonzero status in reply");
				return MSG_ST_BAD_MSG;
			}
			if (chk_len < 3) return n;
			if (n < chk_len) {
				msgs.push_back("Reply shorter than expected");
				return MSG_ST_BAD_MSG;
			}
			if (memcmp(t_buf+2, r_buf+2, chk_len-2) == 0) return n;
			msgs.push_back("OID (or value) in reply not as expected");
			return MSG_ST_BAD_MSG;
		}

			// here if not the reply to the request in <t_buf>; go read 
			//		again in case this is, say, a late reply to a previous 
			//		request
		send_req = false;
	}
}


// add index value <n> to an OID
// <p> points to where to put first byte; on exit points to byte after last
// if n < 0 just writes a zero
void MgtSocket::AddIndex(uint8_t * &p, int n) {
	if (n < 0) n = 0;
	
		// set <k> to 7 * ((bytes to add) - 1)
	int k = 0;
	while (n >= (128 << k) && k < 28) k += 7;
	while (k > 0) { *p++ = (uint8_t)((n >> k) | 0x80); k -= 7; }
	*p++ = (uint8_t)(n & 0x7F);
}
		

// first character is:
//		't' for transmitted message,
//		'e' for transmission error,
//		'r' for any received message
void MgtSocket::SaveMessage(unsigned char * b, int len, char flag)
{
		// only save the last 100 messages; erase two at a time
	if (msgs.size() > 100) msgs.erase(msgs.begin(), msgs.begin()+1);
		// only save 200 bytes of each message
	int n = (len > 200) ? 195 : len;

	std::string s;
	s = flag;
	int i = 0;
	while (i < n) {
		s += ' ';
		s += ToHex(b[i], 2);
		i += 1;
	}
	if (i < len) {
		s += " ...";
		i = len - 5;
		while (i < len) {
			s += ' ';
			s += ToHex(b[i], 2);
			i += 1;
		}
	}
	msgs.push_back(s);
}

