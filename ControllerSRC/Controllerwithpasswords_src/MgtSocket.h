// Copyright (c) 2014-2015 Nine Tiles

// NOTE: for historical reasons, the MgtSocket class is declared in ControllerDoc.h

#pragma once

#include "../Common/string_extras.h"

#define PORT_TYPE_AUDIO		2
#define PORT_TYPE_VIDEO		3

// "direction" etc values for audio ports
#define DIRECTION_IN	1
#define DIRECTION_OUT	2
#define AUDIO_FORMAT_UNKNOWN	 0	// not been reported or not recognised
#define AUDIO_FORMAT_NONE		 1	// 1.0.62379.2.2.1.1
#define AUDIO_FORMAT_ANALOGUE	 2	// 1.0.62379.2.2.1.2.a.c
#define AUDIO_FORMAT_PCM		 3	// 1.0.62379.2.2.1.3.a.c.b.f
#define AUDIO_FORMAT_INVALID	13	// 1.0.62379.2.2.1.13


// structure to hold the result of parsing the "length" field
struct ValueSizes {
	int hd_len;		// bytes of tag + length (-1 if invalid)
	int len;		// bytes of value
};

extern ValueSizes ParseLength(uint8_t * b, int len);


// object, as in a MIB
// CByteArray holds the value, in the same format as in ASN.1 BER
class MibObject : public ByteString
//class MibObject : public CByteArray
{
public:
	MibObject();
	MibObject(uint8_t * &p, int &len);	// reads a VarBind
	virtual ~MibObject();

		// redefining CByteArray members for backwards compatibility
		// +++ I have removed the <const> suffixes below because calls of these 
		//		routines get faulted "cannot convert 'this' pointer from 'const 
		//		MibObject' to 'MibObject &' if they're left in", even though 
		//		<size> etc are defined as <const> too
	int GetCount() { return (int)(size()); }
	int GetUpperBound() { return (int)(size()) - 1; }
	uint8_t GetAt(int i) { return at(i); }
	uint8_t * GetData() { return data(); }
	void SetSize(int n) { resize(n); }
	void Add(uint8_t b) { push_back(b); }

	CString oid;		// as ASCII in the dotted-decimal form
	ByteString oid_ber;	// as in the message (excluding tag and length)
	int tag;			// see below
	int value;			// valid only if <tag> is ASN1_TAG_INTEGER
	__time64_t recd;	// time the value was reported
	uint8_t msg_type;	// first 4 bits of message in which reported (in high half)

#define TAG_INTEGER_OUT_OF_RANGE	-2 // tag was ASN1_TAG_INTEGER, length not 1-4
#define TAG_INVALID					-1 // other fields not valid, byte array empty
#define ASN1_TAG_INTEGER			 2
#define ASN1_TAG_OCTET_STRING		 4
#define ASN1_TAG_OID				 6

	int IntegerValue() { if (this == NULL || tag != ASN1_TAG_INTEGER) return 0;
						 return value; }
	CString StringValue();// const;
	CString StringHex(int group_size);// const;
	void CopyOctetStringValue(CByteArray& b);
	ByteString OctetStringValue() { if (this == NULL || 
				tag != ASN1_TAG_OCTET_STRING) return ByteString(); return *this; }


		// return value of object as a character string if an octet-string
	std::string StdStringValue();// const;
		// return value of object in hex if an octet-string
	std::string StdStringHex(int group_size);// const;
		// return value of object as text
	std::string TextValue();// const;

		// set <tag>, <value>, and the byte array from an ASN.1 value in a message
		// <b> points to the tag, <len> is the number of bytes left in the message
		// updates <b> and <len> and returns <true> if OK, else sets <tag> to -1 
		//		and empties the byte array and returns <false> 
	bool GetAsn1Value(uint8_t * &b, int &len);

		// return the value in dotted-decimal form if it is a valid OID, else an 
		//		empty string
	CString ConvertOid();

		// append the value to <b> in the format for an index in an OID; returns 
		//		<true> if OK, <false> with <b> unchanged else
//	bool AppendAsIndexTo(CByteArray & b);
	bool AppendAsIndexTo(ByteString & b);

		// copy the OID into a message (including tag and length)
	int CopyOid(uint8_t * p);

		// clear out any value that was stored
	void SetInvalid() { clear(); tag = TAG_INVALID; }

		// return whether another object has a significantly different value
	bool ChangedFrom(MibObject * m);
};


class VersionNumber
{
public:
	VersionNumber();
	bool IsValid() { return valid; }
	void Invalidate() { valid = false; }
	
	bool FromOctetString(ByteString& s);	
	bool FromFilename(ByteString& s);	
	bool FromFilename(CString s);	
	std::string ToText();
	ByteString ToOctetString();
	CString ToFilename();
	bool operator== (VersionNumber v);
	bool operator!= (VersionNumber v);
	int operator[] (int n) { return (!valid || n < 0 || n > 3) ? -1 : vn[n]; }

private:
	bool valid;
	uint8_t vn[4];	// major, minor, release, beta
};


// Ethernet MTU, used for buffer lengths; we keep the outgoing messages a 
//		bit shorter; in practice incoming messages won't be the full length 
//		because the IP and UDP headers will have been removed
#define MAX_REPLY_LENGTH  1512	// max for an Ethernet packet
// Tranche size when accessing the flash; this is the size of the octet 
//		string, not including the message header, OID, and tag+length which 
//		we assume will total less than 80 bytes
#define MAX_DATA_LENGTH	  1200	// OK for IPv6, Flexilink async, Ethernet


// descriptor for an "area": each field is 1.0.62379.1.1.5.1.1.f.n, where f is 
//		listed in the comments below and n is the area id, which the standard 
//		allows to be any strictly-positive int32_t value and in our case will 
//		be the block number in the flash
// we don't store the class because we expect it to be always 2 (firmware)
// <access> is always as reported by the unit
// <status> is as reported by the unit except that we set it to INvALID to 
//		mark the area as needing to be erased
// during upload the other fields show the state expected when the upload is 
//		complete
struct SoftwareArea {
	int32_t length;		// 5 size of area (octets)
	uint8_t access;		// 3 access rights: each includes all smaller numbers
	uint8_t status;		// 4 current state of area
	uint8_t data_type;	// 6 "type" code
	uint8_t serial;		// 7 serial number (of contents; 0-15, 16 if none)
	VersionNumber vn;	// 8 version number (decoded assuming Flexilink)
};
// codes defined in IEC62379-1
#define AREA_ACCESS_READONLY	1
#define AREA_ACCESS_ERASE		3
#define AREA_STATUS_EMPTY		1
#define AREA_STATUS_ERASING		2
#define AREA_STATUS_INVALID		3
#define AREA_STATUS_WRITING		4
#define AREA_STATUS_BEING_WR	5
#define AREA_STATUS_VALID		6
// other codes
#define AREA_TYPE_BOOT_LOGIC	4
#define AREA_TYPE_BOOT_VM_CODE	5
#define AREA_TYPE_LOGIC			6
#define AREA_TYPE_VM_CODE		7
#define AREA_ACTION_NONE		0
#define AREA_ACTION_WRITE		1
#define AREA_ACTION_ERASE		2
#define AREA_ACTION_REWRITE		3 // erase then write; = _ERASE + _WRITE
// map of the flash, indexed by area id; I think we can assume new entries 
//		are initialised all-zero, so fields for which no VarBind has been 
//		received will be zero
typedef std::map<int32_t, SoftwareArea> FlashMap;


// Status codes in low half of first word of message; 0-5 are as in SNMP
#define MSG_ST_OK			 0
#define MSG_ST_TOO_BIG		 1
#define MSG_ST_NO_SUCH_NAME	 2
#define MSG_ST_BAD_VALUE	 3
#define MSG_ST_READ_ONLY	 4
#define MSG_ST_GEN_ERR		 5
#define MSG_ST_TEMP_ERR		14 // temporarily unable to access object
#define MSG_ST_END_CYCLE	15 // last Status Response of cycle
// Additional status codes returned by LinkSocket::Send[GetNext]Request
#define MSG_ST_SKT_ERROR	(-1) // "sockets" call failed, code in <errno>
#define MSG_ST_NOT_CONN		(-2) // <handle> is -1
#define MSG_ST_TIMED_OUT	(-3) // no reply received
#define MSG_ST_BAD_MSG		(-4) // could not decode reply



// In this version, a single LinkSocket object manages the link to the unit that 
//		acts as our gateway into the Flexilink network, and through which the 
//		MgtSocket objects send and receive packets.
// The flow number for each MgtSocket is its index in theApp.units.

class LinkSocket : public CAsyncSocket
{
public:
	LinkSocket();
//	LinkSocket(class CControllerDoc * pDoc);
	virtual ~LinkSocket();

		// send the Link Request; returns whether successful
	BOOL Init();

		// send a message (SendMessage is already defined)
	bool TxMessage(uint8_t * b, int len, int flow = 0);
//	bool TxMessage(CByteArray & m, int flow = 0);
	bool TxMessage(ByteString m, int flow = 0) 
						{ return TxMessage(m.data(), (int)m.size(), flow); }

		// process an incoming message
	virtual void OnReceive(int nErrorCode);

		// information about the network connection that carries the link
		// if <standard_format> is true the "length" in IT packet headers is 
		//		n-1 where n is the number of payload bytes; if false (the 
		//		default) it is n+3
	CString link_ip_addr;	// IPv4 address for gateway into Flexilink network
#define AES51_PORT	 35037	// port number
//	UINT remote_mgt_port;
	bool standard_format;	// IT packet headers conform to ETSI GS NIN 005
	char our_ident[8];		// byte order as in network messages
	int tx_timer_count;		// timer ticks until send keepalive
	int rcv_timer_count;	// timer ticks until link times out
//#define LINK_RCV_TIMEOUT 14		// 7 sec
//#define LINK_KEEPALIVE_PERIOD 4	// 2 sec
#define LINK_RCV_TIMEOUT 7		// 7 sec
#define LINK_KEEPALIVE_PERIOD 2	// 2 sec
	bool PollKeepalives();	// send and check for rcv

		// current state: one of the following
#define LINK_ST_BEGIN	 0	// nothing done yet
#define LINK_ST_REQ		 1	// Link Request sent
#define LINK_ST_ACTIVE	 2	// Link Accept has been received
#define LINK_ST_MAX_OK	 5	// codes above this are for "failed" states
#define LINK_ST_CLOSED	 8	// target has disconnected
#define LINK_ST_FAILED	 9	// call to base class (Create, Connect, Send) failed
#define LINK_ST_ERROR	10	// any kind of protocol error
	int state;

		// the link partner's 64-bit identifier; valid in ACTIVE state only
	CByteArray link_partner_id;
};

