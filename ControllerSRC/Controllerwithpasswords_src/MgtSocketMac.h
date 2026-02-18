// TeaLeaves / Flexilink platform
// Copyright (c) 2012 Nine Tiles

#import <list>
#import <map>
#import "../Common/string_extras.h"

/////////////////////////////////////////////////////////////////////////////
// globals

// put <msg> on the screen; no reply (other than "OK") required
extern void AlertUser(std::string msg);
// put <msg> on the screen with YES and NO buttons
extern bool AskUser(std::string msg);
// put <msg> on the screen with OK and INC and DEC buttons
extern int AskUserIncDec(std::string msg);


// +++ ought to read in the MIB from a .9tlrem file; should have an initial 
//		MIB that covers all units and includes the product code, and then a 
//		"proper" MIB for each product

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


// type for storing a MIB using the OID as a key
// +++ the value ought also to include the time it was last retrieved; it 
//		isn't used for software upload but will be needed for receiving 
//		status broadcasts
typedef std::map<ByteString, class MibObject> MibMap;


// object, as in a MIB
// the vector holds the value, in the same format as in ASN.1 BER (omitting 
//		the tag and length)
class MibObject : public ByteString
{
public:
	MibObject();
	virtual ~MibObject();

		// set <tag> and the byte array from an ASN.1 value in a message
		// <b> points to the tag, <len> is the number of bytes left in the 
		//		message
		// updates <b> and <len> and returns <true> if OK, else sets <tag> 
		//		to -1 and empties the byte array and returns <false> 
	bool GetAsn1Value(uint8_t * &b, int &len);

	int tag;			// see below
//	__time64_t recd;	// time the value was reported
//	uint8_t msg_type;	// first 4 bits of message in which reported (in high half)

//#define TAG_INTEGER_OUT_OF_RANGE	-2 // tag was ASN1_TAG_INTEGER, length not 1-4
#define TAG_INVALID					-1 // other fields not valid, byte array empty
#define ASN1_TAG_INTEGER			 2
#define ASN1_TAG_OCTET_STRING		 4
#define ASN1_TAG_OID				 6

//#define NET_PORT_STATE_PT_PT		 5 // pointToPoint codepoint for NetPortState

	int IntegerValue() const;
		// return value of object as a character string if an octet-string
	std::string StringValue() const;
		// return value of object in hex if an octet-string
	std::string StringHex(int group_size) const;
		// return value of object as text
	std::string TextValue() const;

		// append the value to <b> in the format for an index in an OID; returns 
		//		<true> if OK, <false> with <b> unchanged else
	bool AppendAsIndexTo(std::vector<uint8_t> & b);

		// copy the value into a message (including length but not tag)
	void CopyValue(uint8_t * p);

		// clear out any value that was stored
	void SetInvalid() { clear(); tag = TAG_INVALID; }

		// return whether another object has a significantly different value
//	bool ChangedFrom(MibObject * m);
};


class VersionNumber
{
public:
	VersionNumber();
	bool IsValid() { return valid; }
	void Invalidate() { valid = false; }
	
	bool FromOctetString(ByteString s);	
	std::string ToText();
	ByteString ToOctetString();
	
private:
	bool valid;
	uint8_t vn[4];	// major, minor, release, beta
};


// descriptor for an "area": each field is 1.0.62379.1.1.5.1.1.f.n, where f is 
//		listed in the comments below and n is the area id, which the standard 
//		allows to be any strictly-positive int32_t value and in our case will 
//		be the block number in the flash
// we don't store the class because we expect it to be always 2 (firmware)
struct SoftwareArea {
	int32_t length;		// 5 size of area (octets)
	uint8_t access;		// 3 access rights: each includes all smaller numbers
	uint8_t status;		// 4 current state of area
	uint8_t data_type;	// 6 "type" code
	uint8_t serial;		// 7 serial number (of contents; 0-15, 16 if none)
	std::string vn;		// 8 version number (of contents)
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
// Flexilink codes
#define AREA_TYPE_BOOT_LOGIC	4
#define AREA_TYPE_BOOT_VM_CODE	5
#define AREA_TYPE_LOGIC			6
#define AREA_TYPE_VM_CODE		7
// map of the flash, indexed by area id
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
// Additional status codes returned by MgtSocket::Send[GetNext]Request
#define MSG_ST_SKT_ERROR	(-1) // "sockets" call failed, code in <errno>
#define MSG_ST_NOT_CONN		(-2) // <handle> is -1
#define MSG_ST_TIMED_OUT	(-3) // no reply received
#define MSG_ST_BAD_MSG		(-4) // could not decode reply

// There is a MgtSocket object for each unit that we know about. It collects 
//		information about the unit and will looks after setting up and clearing 
//		down the unit's network connections when we have units with media portt. 
//		It also looks after updating the unit's software.
// Currently we assume the communication with the managed unit has to be over 
//		UDP; ideally it would use FN flows, or failing that AES51 packets.
// We also assume there's no point in trying to set the program up to receive 
//		status broadcasts (though I think that would be viable under Windows), 
//		so just poll periodically.
class MgtSocket
{
public:
	MgtSocket();
	MgtSocket(class CControllerDoc * pDoc);
	virtual ~MgtSocket();

		// create the socket and set up its addresses etc; <handle> is -1 if 
		//		this hasn't been done (not attempted or tried and failed)
	std::string Connect(uint32_t ip_addr, uint32_t port);
	int32_t handle;	// for the socket

		// send a request message and wait for and process the reply
	int SendGetNextRequest(uint8_t * b, int len);
		// last OID returned in reply
	ByteString oid;

	void CollectMib();
	void CollectConfig();
//	void CollectData();
	std::string SoftwarePath(std::vector<std::string> pf_text, int t);
	int64_t FindNumericValue(std::vector<std::string> pf_text, std::string key);
	std::string FindStringValue(std::vector<std::string> pf_text, std::string key);
	void UploadBootLogic();
	void UploadImage(int area, uint8_t new_type, uint8_t serial, 
														int n, uint8_t * p);
	void SetAreaState(FlashMap::iterator p, uint8_t n);
	void EraseArea(FlashMap::iterator p) {
				if (AskUser("Erase area " + ToDecimal(p->first) + '?')) 
									SetAreaState(p, AREA_STATUS_ERASING); }
	void UploadBootCode();
	void UpdateSoftware();
	void ReadProductFile(std::string f, int * vn);
	int UploadFpgaImage(std::string f, FlashMap::iterator p, uint8_t t);
	int UploadVmCode(std::string f, FlashMap::iterator p, uint8_t t);
	int FindFreeArea(int len, FlashMap::iterator * q, int ser);
	int SendRequest(uint8_t * t_buf, int t_len, 
									uint8_t * r_buf, int r_len, int chk_len);

		// document which owns this socket
	class CControllerDoc * owner_doc;

		// product file paths (NB must re-read contents each time in case 
		//		compiler etc has updated it)
	std::string boot_pf_path;
	std::string pf_path;

		// the managed unit's Flexilink address in hex; the address to which 
		//		the management call was made if that was not "any", else from 
		//		unitAddress by inserting FFFD in the middle (which is how the 
		//		current Flexilink software constructs its <own_ident>) and 
		//		adding 05 (the tag for an EUI64) on the front
	std::string unit_address;

		// the managed unit's Flexilink address in the form in which it appears 
		//		in a TAddress octet string
	ByteString unit_TAddress;

		// serial number to go in next transmitted message
	uint8_t next_serial;

		// copy of the MIB
		// key is the OID, value is a <MibObject>
		// +++ currently not used, but may be needed to receive status broadcasts
	MibMap mib;

		// standard MIB objects
	MibObject unitName;				// 1.0.62379.1.1.1.1
	MibObject unitIdentity;			// 1.0.62379.1.1.1.4
	MibObject unitSerialNumber;		// 1.0.62379.1.1.1.7
	MibObject unitFirmwareVersion;	// 1.0.62379.1.1.1.8

		// map of the flash
	FlashMap flash;
		// copy of what is in one of the areas
//	int image_area;	// -1 if none
//	ByteString flash_image;

		// add an index value to an OID
	void AddIndex(uint8_t * &p, int n);

		// return value of an integer object (up to 32 bits, signed)
		// result is zero if <oid> not found in the MIB
	int GetIntegerObject(ByteString oid) {
			MibMap::iterator p = mib.find(oid);
			return (p == mib.end()) ? 0 : p->second.IntegerValue(); }

		// return value of octet-string object as a character string
		// result is a null string if <oid> not found in the MIB
	std::string GetStringObject(ByteString oid) { 
			MibMap::iterator p = mib.find(oid);
			return (p == mib.end()) ? std::string() : p->second.StringValue(); }

		// return value of octet-string object in hex
		// result is a null string if <oid> not found in the MIB
		// if <group_size > 0>, a space is inserted after every <group_size> 
		//		bytes except at the end of the string; else no spaces inserted
	std::string GetStringHex(ByteString oid, int group_size) {
			MibMap::iterator p = mib.find(oid);
			return (p == mib.end()) ? std::string() : 
											p->second.StringHex(group_size); }

		// expected sequence number of next in-cycle report message, or zero if the 
		//		information is already known to be incomplete (e.g. a message has 
		//		arrived out of sequence)
	uint8_t next_seq;
		// time the previous message in the current cycle arrived (not valid if 
		//		<next_seq> is 0 or 1)
//	__time64_t prev_report_time;
		// 2 secs before time of the most recent start of a cycle (not valid if 
		//		<next_seq> is 0 or 1)
		// any object that should be reported in a cycle and is older than this at 
		//		the end of an OK cycle can be assumed to have disappeared from the 
		//		MIB
//	__time64_t start_cycle_time;

		// flag which is set whenever anything in the MIB is updated and cleared when 
		//		repainting of the Controller screen is requested
		// if clear, we know the screen is up-to-date; the screen could also be up-to-
		//		date if it is set, for instance if the update happened before the 
		//		object was displayed or was to an object that doesn't affect the 
		//		displayed information
//	bool mib_changed;

		// return whether the handle is valid
	bool IsConnected() { return handle != -1; }

		// messages: only save the first 100 or so
		// first character is 't' for transmitted message, 'r' for an OK 
		//		received message, 'e' for receive error
	std::vector<std::string> msgs;		// in hex
	void SaveMessage(unsigned char * b, int len, char flag);

		// platform-specific routines
	std::string GetProductDirectoryPath();
	std::string AskProductFilePath(std::string fn);
	std::string AskProductFilePath() { return AskProductFilePath(std::string()); }
	std::vector<std::string> ReadTextFile(std::string f);
	BytesOnHeap ReadBinaryFile(std::string f);
	bool WriteFile(std::string f, std::vector<std::string> txt);
};

