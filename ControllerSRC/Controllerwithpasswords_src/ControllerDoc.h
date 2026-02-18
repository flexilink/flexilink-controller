// ControllerDoc.h : interface of the MgtSocket and CControllerDoc classes
// Copyright (c) 2014-2023 Nine Tiles

// This is the "document" class that provides the user interface for one of 
//		the Flexilink units

#pragma once
#include "../common/string_extras.h"
#include "MgtSocket.h"
#include "Query.h"
#include <queue>

// NOTE: versions before 2023 read data from up to three files:
//		- name_translations.txt contained names to be assigned to ports 
//				as a simple substitution for the port's default name
//		- flash_config_9t.txt was intended to be used for an image of the 
//				flash page in each unit that contains its "sticky" MIB 
//				values; the code to upload it was never implemented
//		- dynamic_config_9t.txt held MIB objects to be uploaded when 
//				connecting to a unit
//	All the above have now been superseded by the implementation of "non-
//				volatile Set" in the management protocols.


class CControllerDoc : public CDocument
{
	friend class CControllerView;
protected: // create from serialization only
	CControllerDoc();
	DECLARE_DYNCREATE(CControllerDoc)

// Implementation
public:
	virtual ~CControllerDoc();

	int display_select;	// -2 for console, -1 for MIB, <n> for <theApp.scp[n]>
#define CONSOLE_DISPLAY -2
#define MIB_DISPLAY -1
	void SelectNextScp(); // as when typing '@'

protected:
	class MgtSocket * unit;	// the unit being displayed; NULL if none

public:
		// set <unit> to <u> and update display; does nothing if <this> is NULL
	void SetUnit(MgtSocket * u);
		// give the focus to the Controller window
	void BringToFront();
		// name of <u> has changed: update title
	void UpdateTitle();
	void NameChanged(MgtSocket * u) { if (UnitIs(u)) UpdateTitle(); }
		// whether <unit == u>; returns <false> if <this> is NULL
	bool UnitIs(MgtSocket * u) { return this && (u == unit); }
		// whether <u> is selected for display; returns <false> if <this> is NULL
	bool Selected(class FlexilinkSocket * u);
		// update display (safe if <this == NULL>)
	void UpdateDisplay() { if (this) UpdateAllViews(NULL); }

	CString failure_notice;

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()

// Overrides
public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
};


// structure to show what (if any) acknowledgement a thread is awaiting
struct MessageAckState {
	ByteString m;	// the message; empty if none; serial number in 2nd byte
	int count;	// number of timer ticks since first sent (rubbish if m empty)
};
#define MAX_REPEAT_COUNT	20	// give up after 10 secs

// structure describing a connection request
// if <m[0]> is a Get, we are waiting for the call id; else we are waiting 
//		for acks to one or more Set requests
struct ConnReqInfo {
	ByteString srce_addr;
	int dest_port;
	std::vector<ByteString> m;	// the messages (at least one, all at least 2 bytes)
	int count;		// number of timer ticks since first sent
};


// class used for keeping track of the unit's network ports
// key is port number, value is NetPortState coding from the MIB except that 
//		NET_PORT_WAITING is used instead when entering linkUp state
// states; +ve values are NetPortState code points
#define NET_PORT_STATE_WAITING (-1) // see above
#define NET_PORT_STATE_DISABLED	 1	// disabled
#define NET_PORT_STATE_LINK_UP	 4	// linkUp
#define NET_PORT_STATE_PT_PT	 5	// pointToPoint
class NetPortList : public std::map<int, int> {
public:
	NetPortList();
	~NetPortList();

		// set state <st>; return whether it set NET_PORT_STATE_WAITING
	bool NewState(int port, int st);
		// set state _LINK_UP, assuming current state is _WAITING
	void EndWait(int port) { at(port) = NET_PORT_STATE_LINK_UP; }
		// remove the entry for <port> from the map
	void Remove(int port) { erase(port); }
		// return the state for <port> if it's in the map
	int State(int port);
		// return whether <port> is in the list
	bool IsInList(int port) { NetPortList::iterator p = find(port);
													return (p != end()); }
};



// FlexilinkSocket is an abstract base class that provides the basic mechanism 
//		for making a connection and sending and receiving messages; derived 
//		classes are used for specific types of connection such as to a management 
//		agent or a SCP server
// NOTE: this was originally part of MgtSocket, so the implementations of 
//		routines are mixed in with those of MgtSocket

class FlexilinkSocket
{

// Implementation
public:
	FlexilinkSocket();
	virtual ~FlexilinkSocket();
#ifdef _DEBUG
//	virtual void AssertValid() const;
//	virtual void Dump(CDumpContext& dc) const;
#endif

		// send FindRoute request
//	void Init(ByteString call_addr);
	virtual void SendConnReq() = 0;

		// send a message (named to match the one in the LinkSocket class)
	void TxMessage(uint8_t * b, int len);
	void TxMessage(ByteString& m) { TxMessage(m.data(), (int)m.size()); }

		// process an incoming signalling message
	void ReceiveSignalling(uint8_t * b, int len);
		// called by the above when the message shows the connection has been made
	virtual void ConnectionMade(uint8_t * b) = 0; // b -> tag of Password IE, NULL if none
	virtual void ReceiveData(uint8_t * b, int len) = 0;

		// call reference for the connection to the management agent: -1 if 
		//		undefined, else structured as (from ms end):
		//	 1 bit:  always 0 else (so is non-negative)
		//	15 bits: incremented each time the call needs to be re-established
		//	 8 bits: 0 = management socket, 1 = SCP socket, 2-255 reserved
		//	 8 bits: index in <theApp.units> or <theApp.scp>
		// the ls 13 bits are also used as the flow label for rcv packets
	int call_ref;
		// flow label for tx packets; -1 until FindRoute response rec'd
	int tx_flow;

		// current state: one of the following
		// +++ most of this is purely session layer state; the update process has 
		//		its own state (though I'm not convinced it will survive a change 
		//		of lower-layer connection) and I think everything else is 
		//		stateless at the protocol level; the link socket state also needs 
		//		to be checked if e.g. a reply fails to arrive
		// +++ however, states 3 and 4 are really layer 7
#define MGT_ST_BEGIN	 0	// nothing done yet
#define MGT_ST_CONN_REQ	 1	// FindRoute request sent
#define MGT_ST_CONN_ACK	 2	// FindRoute request acknowledged
#define MGT_ST_CONN_MADE 3	// <tx_flow> valid; initial GetNext sent
#define MGT_ST_HAVE_INFO 4	// unitName etc valid; Status Request has been sent
#define MGT_ST_ACTIVE	 5	// Status Response has been received (normal state)
#define MGT_ST_MAX_OK	 5	// codes above this are for "failed" states
#define MGT_ST_NOT_CONN	 6	// failed to connect
#define MGT_ST_FAILED	 7	// call to base class (Create, Connect, Send) failed
#define MGT_ST_TIMEOUT	 8	// any kind of timeout
#define MGT_ST_MIN_RETRY 9	// wait before retry connection for codes below this
#define MGT_ST_CLOSED	 9	// target has disconnected
#define MGT_ST_ERROR	11	// any other kind of protocol error in flash map
	int state;
		// set state to _FAILED; might want to do some tidying up too
	void SetStateFailed() { state = MGT_ST_FAILED; UpdateDisplay(); 
												theApp.mib_changed = true; }
	void SetStateTimedOut() { state = MGT_ST_TIMEOUT; UpdateDisplay(); 
												theApp.mib_changed = true; }
	void SetStateError() { state = MGT_ST_ERROR; UpdateDisplay(); 
												theApp.mib_changed = true; }

		// add an index arc to an OID; add the "length" field for a value; 
		//		add an integer value to a message
	void AddIndex(uint8_t * &p, int n);
	void AddIndex(ByteString& m, int n);
	void AddLength(uint8_t * &p, int n);
	void AddInteger(uint8_t * &p, int64_t n);
	void AddInteger(ByteString& m, int64_t n);


		// serial number to go in next transmitted management message
	uint8_t next_serial;

		// messages: only save the last 300 or so
		// first character is 't' for transmitted message, 'o' for overlength 
		//		received message, 'r' for any other received message
	CStringArray msgs;		// in hex
	void SaveMessage(uint8_t * b, int len, char flag);
	void SaveMessage(ByteString b, char flag) 
							{ SaveMessage(b.data(), (int)b.size(), flag); }
	int AddMsgsToDisplay(int x, int y, int CharHeight, CDC * pDC);

		// console text: only save the last 2000 lines (HyperTerminal saves about 500)
		// each string is 1 line
	std::vector<std::string> console;
	void SaveConsole(unsigned char * b, INT_PTR len);
	void ConsoleLine(std::string s);

		// check whether anything needs to be repeated: called every 1/2 sec
		// derived classes should call this to check on signalling messages and 
		//		override to check on their own message types if required
		// +++ NOTE: messages are not expected to include a password
	bool PollAwaitingAck();
		// count to check the unit hasn't locked up
	int keep_alive_count;
		// record for each thread
	MessageAckState mgt_msg;	// messages that affect <state>

		// update display if required
	void UpdateDisplay() { if (theApp.controller_doc->Selected(this)) 
									theApp.controller_doc->UpdateAllViews(NULL); }
	void UpdateMibDisplay() { if (theApp.controller_doc->Selected(this)) 
									theApp.controller_doc->UpdateAllViews(NULL); }

		// read text file with filename <fn>
	std::vector<std::string> ReadFile(std::string fn);

		// routine called from idle loop to check for non-urgent tasks
//	void OnIdle();

		// Hashing for Aubergine passwords
#define SHA3_HASH_COUNT_BYTES	 4 // bytes for the count
#define SHA3_HASH_BYTES			64 // bytes for the hash
#define SHA3_HASH_STRING_BYTES (SHA3_HASH_COUNT_BYTES + SHA3_HASH_BYTES)

		// Hash calculation, imported from SHA3.h so the routine can use <SaveMessage>
		// 'Words' here refers to uint64_t; <sizeof> counts octets
#define SHA3_KECCAK_SPONGE_WORDS (1600/(sizeof(uint64_t) * 8))
	typedef uint64_t sha3_sponge[SHA3_KECCAK_SPONGE_WORDS];
	void keccakf(uint64_t s[SHA3_KECCAK_SPONGE_WORDS]);

		// write <n> 64-bit words big-endianly (i.e. network byte order) to <b>
	void CopyLongwordsToNetwork(uint8_t * b, uint64_t * w, int n);
};



// There is a MgtSocket object for each unit on the Flexilink network that 
//		we know about. It collects information about the unit and looks after 
//		making and clearing down the unit's network connections.
// Whereas in the earlier versions there were separate CAsyncSocket classes for 
//		the connections to individual units, here we have a single LinkSocket object 
//		for the link to the unit that acts as the gateway to all the other units.
// Also, previous versions had a separate set of "document" objects for each unit, 
//		here we have a single such "document" and the user can select which unit it 
//		reports by clicking in one of the crosspoint windows.

// NOTE: objects of this class should only be created by CControllerApp::NewUnit(), 
//			which fills <label> in

class MgtSocket : public FlexilinkSocket
{

// Implementation
public:
	MgtSocket();
	virtual ~MgtSocket();
#ifdef _DEBUG
//	virtual void AssertValid() const;
//	virtual void Dump(CDumpContext& dc) const;
#endif

		// send FindRoute request
//	void Init(ByteString call_addr);
	void SendConnReq();

		// send a message (override for the base class version)
	void TxNewMessage(uint8_t * b, int len, bool pw = true, 
													bool flash_request = false);
	void TxMessage(uint8_t * b, int len, bool pw = true);
	void TxWithHash(uint8_t * b, int len);
//	void TxMessage(CByteArray& m);
	void TxWithHash(ByteString& m) { TxWithHash(m.data(), (int)m.size()); }
	void TxNewMessage(ByteString& m, bool pw = true)
									{ TxNewMessage(m.data(), (int)m.size(), pw); }
	void TxMessage(ByteString& m, bool pw = true)
										{ TxMessage(m.data(), (int)m.size(), pw); }

		// send a "request status reports" message
	void RequestStatus(bool req_ack);

		// process an incoming message
	void ConnectionMade(uint8_t * b); // override, see <FlexilinkSocket>
	void ReceiveData(uint8_t * b, int len);

		// <label> in the parent class is the index in <theApp.units>
		// NOTE: must be set when the object is created; see note at the top 
		//		of the class declaration

		// current state: values not specified in the parent class
// MGT_ST_CONN_MADE		<tx_flow> valid; initial GetNext sent
// MGT_ST_HAVE_INFO		unitName etc valid; Status Request has been sent
// MGT_ST_ACTIVE		Status Response has been received (normal state)
// MGT_ST_ERROR			any protocol error in flash map not otherwise defined

		// password (supplied by user) and "random string" (generated when the 
		//		call is connected)
		// words 0-3 are the password: UTF-8 with bytes stored big-endianly in 
		//		each word, padded with NULs to 32 bytes
		// words 4-5 are taken from the FindRoute response
		// words 6-7 are generated locally and sent in the FindRoute request; 
		//		the low half of word 7 is the count of messages sent with a 
		//		hash included
		// state is _NOT_USED until FindRoute response received, then as shown
	int password_state; // one of the following
#define PW_NOT_USED		0	// not rec'd FindRoute response with Password IE
#define PW_CANCELLED	1	// user unticked "use in messages"
#define PW_NOT_SET		2	// haven't asked the user for it yet
#define PW_REQUESTED	3	// have asked the user, no reply yet
#define PW_VALID		4	// have a valid password

		// words 0-3 are rubbish in all states except _VALID, words 4-7 are 
		//		rubbish in _NOT_USED state except that 6-7 are valid while 
		//		waiting for the FindRoute response
	uint64_t password_string[8];
		// return whether expect to need a password
	bool ExpectPassword() { if (theApp.privilege == PRIV_LISTENER) return false;
					unless (theApp.privilege == PRIV_MAINTENANCE) return true;
					return theApp.link_partner != this; }
		// ask the user to input the password
	void GetPassword();

		// map of the flash; areas that are to be erased are shown as INVALID 
		//		even if they are still valid in the flash, and when uploading 
		//		to an area begins its record is set to show the new state
	FlashMap flash;


// state of the process of uploading to the flash

		// reply to request whether to update software in flash: -ve if 
		//		haven't asked yet, else as <CQuery::result>
	int user_update_flags;
	bool OkToUpdate();
	void StartCollectFlashMap();

		// when writing, also when setting to Valid, the reply doesn't come 
		//		until the operation is complete; erasing simply marks the 
		//		area(s) to be erased, and as each completes its status changes 
		//		to Empty; we don't get told when the last one finishes, so we 
		//		stick in "waiting" state until the user clicks on the status 
		//		message on the screen
#define UPD_ST_NO_INFO		 0	// waiting for unitIdentity etc
#define UPD_ST_BEGIN		 1	// have unitIdentity and unitFirmwareVersion
#define UPD_ST_NOT_RECOGD	 2	// unitIdentity etc not as expected
#define UPD_ST_HAVE_SW_VER	 3	// unitIdentity etc decoded
#define UPD_ST_NO_P_FILE	 4	// couldn't open product file
#define UPD_ST_BAD_P_FILE	 5	// couldn't find req'd info in product file
#define UPD_ST_NO_ACTION	 6	// already up-to-date
#define UPD_ST_FROM_ISE		 7	// software loaded by Impact and Compiler
#define UPD_ST_COLLECT_MAP	 8	// reading contents of flash
#define UPD_ST_HAVE_MAP		 9	// ready to check what needs updating
#define UPD_ST_UP_TO_DATE	10	// flash OK; needs restart
#define UPD_ST_NOT_MAINT	11	// privilege level forbids update
#define UPD_ST_USER_REFUSED	12	// user replied refusing update
#define UPD_ST_BAD_FLASH	13	// e.g. all 16 serial numbers used
#define UPD_ST_NO_S_FILE	14	// no data read from image file
#define UPD_ST_MIN_FILE_ERR	15	// first of the "file error" codes
#define UPD_ST_NO_LOGIC		15	// couldn't open logic image file
#define UPD_ST_BAD_LOGIC	16	// e.g. no sync word found in logic image file
#define UPD_ST_NO_VM_FILE	17	// couldn't open VM software image file
#define UPD_ST_BAD_VM_FILE	18	// e.g. VM software image file empty
#define UPD_ST_MAX_FILE_ERR	18	// last of the "file error" codes
#define UPD_ST_UPLOADING	19	// writing (see <upd_area> and <upd_offset>)
#define UPD_ST_TIDY_UP		20	// check for areas that should be erased
#define UPD_ST_MAKE_SPACE	21	// as _TIDY_UP when have run out of space
#define UPD_ST_ERASING		22	// erasing (tidying up; see <upd_area>)
#define UPD_ST_WAITING		23	// for erases to complete
#define UPD_ST_FAILED		24	// failure during uploading process
	int upd_state;					// one of the above
	int upd_state_view;				// <upd_state> as displayed by the view
	FlashMap::iterator upd_area;	// for which erase or write requested
	int vl_serial[2];	// latest serial number in map; index as for <vl_type>
	VersionNumber flash_ver[2];	// version in latest area; index as for <vl_type>
		// following are valid in UPLOADING state only
		// <upd_offset> is negative (-4 to -1) for the four commands (beginning 
		//		with setting the area to "writing" state) that precede the 
		//		uploading of the data, thereafter it is the offset in <upd_area> 
	int upd_offset;				// see above
	ByteString image;			// to write to the selected area
//	int PreWriteValue();		// value for current Set if <upd_offset < 0>
	void SendNextErase();		// send Erase request if required; update state
	void StartUpload(int i);	// set up for writing flash

		// product code and software versions from MIB, valid in states > 2
	uint8_t product_code[4];	// unitIdentity
	VersionNumber sw_ver[2];	// unitFirmwareVersion; index as below

		// info from product file, set when file is read if different from 
		//		unitFirmwareVersion, cleared when in flash; "clear" is when 
		//		type is -ve
		// index is 0 for VM, 1 for logic
		// <vl_update[i]> is set if version running is not <vl_ver[i]>, cleared 
		//		if latest in flash is <vl_ver[i]> or when uploaded
	int vl_type[2];				// swaType value from product file
	CString vl_fn[2];			// filename in product file
	VersionNumber vl_ver[2];	// version number from filename
	bool vl_update[2];			// whether needs upload
	int last_serial;			// for boot logic

		// first 2 bytes of expected reply; ls half of 1st byte is coded as 0 
		//		(no error); coded as "status response" if no reply expected
	uint8_t upd_reply;		// first byte; 0xA0 = none
	uint8_t upd_msg_ser;	// serial number; rubbish if 1st byte is 0xA0

		// the managed unit's Flexilink address in the form in which it appears 
		//		in a TAddress octet string; this is the called address used 
		//		when the connection was set up; note that (unlike earlier 
		//		versions) it is not updated from the unitIdentifier MIB object
	ByteString unit_TAddress;
		// the same as a hex string (letters in upper case, 2 digits per byte) 
		//		so we can use it as the key for a map (see note where the maps 
		//		are declared in Controller.h)
	CString unit_address;
		// the unit's 64-bit identifier, from the unitIdentifier MIB object
	uint64_t unit_id;
		// the unit's name as displayed on the screen
	CString unit_name;
		// SCP interface to thes unit
	class AnalyserDoc * scp_server; // NULL if none

		// serial number in last console message received (-1 if none)
	int last_console_serial;

		// latest value for each object
		// new objects are added at the end; new values for an existing object 
		//		replace the existing object; objects missing from a report cycle 
		//		are removed from here and <mib_map>
		// see note on <dest_call_flows> re block outputs
	CList<class MibObject *, MibObject *> mib;

		// key is the dotted-decimal form of the OID, value is the POSITION in <mib>
		// there is a 1:1 correspondence between entries in <mib> and <mib_map>
		// +++ should consider replacing <map> and <mib_map> with a 
		//		std::map<std::string, MibObject *>
	CMapStringToPtr mib_map;

		// return pointer to object in the MIB, or NULL if not present
	MibObject * GetObject(CString oid) {
			void * v;
			if (mib_map.Lookup(oid, v)) return mib.GetAt((POSITION)v);
			return NULL; }

		// return value of an integer object (up to 32 bits, signed)
		// result is zero if <oid> not found in the MIB
	int GetIntegerObject(CString oid) { return GetObject(oid)->IntegerValue(); }

		// return value of octet-string object as a character string
		// result is a null string if <oid> not found in the MIB
	CString GetStringObject(CString oid) { return GetObject(oid)->StringValue(); }

		// return value of OID object as a dotted-decimal character string
		// result is a null string if <oid> not found in the MIB
	CString GetOidObject(CString oid) { return GetObject(oid)->ConvertOid(); }

		// return value of octet-string object in hex
		// result is a null string if <oid> not found in the MIB
		// if <group_size > 0>, a space is inserted after every <group_size> 
		//		bytes except at the end of the string; else no spaces inserted
	CString GetStringHex(CString oid, int group_size) {
								return GetObject(oid)->StringHex(group_size); }

		// return value of octet-string object as a byte array
		// result is a null string if <oid> not found in the MIB
	void CopyOctetStringObject(CString oid, CByteArray& b) { 
									GetObject(oid)->CopyOctetStringValue(b); }
	ByteString GetOctetStringObject(CString oid) {
									return GetObject(oid)->OctetStringValue(); }

		// text description used in crosspoint windows
	CString DisplayName();

		// lists of ports and calls we know about
typedef CList<int, int> PortList;
	int IntIndex(CString s);
		// map of network ports for which the state is in <mib>, showing state
	NetPortList net_port_state;
		// block ids for media ports for which the direction is in <mib>, 
		//		ordered from smallest to largest
	PortList input_port_list;
	PortList output_port_list;
	POSITION PortLocation(PortList * list, int n);

		// map for flows connected to output ports; key is block id as an ASCII 
		//		decimal number, value is the index arcs (including the block id 
		//		and the "number of octets" arc at the start of the flow id) in 
		//		the format in which they appear in the dotted-decimal OID
		// there is an entry in this map for the most recent (flow, block id) 
		//		pair for which a usState has been received for each block id; 
		//		note that the entry might no longer be in <mib>, in which case 
		//		the entry in this map will be deleted on the next attempt to 
		//		access it
		// NOTE: this assumes there is at most one flow connected to each port; 
		//		ideally we should model the way the audio buffers allow flows to 
		//		pick-and-mix different channels; in practice there will also be 
		//		entries for network ports, unless we remove those from the status 
		//		broadcast, but they will be ignored when building the display
	CMapStringToString output_flows;
		// similarly for input ports, except that the value doesn't include the 
		//		port id; there is an entry for each udNetBlockId for a media port
	CMapStringToString input_flows;

		// expected sequence number of next in-cycle report message, or zero if the 
		//		information is already known to be incomplete (e.g. a message has 
		//		arrived out of sequence)
	uint8_t next_seq;
		// time the previous message in the current cycle arrived (not valid if 
		//		<next_seq> is 0 or 1)
	__time64_t prev_report_time;
		// 2 secs before time of the most recent start of a cycle (not valid if 
		//		<next_seq> is 0 or 1)
		// any object that should be reported in a cycle and is older than this at 
		//		the end of an OK cycle can be assumed to have disappeared from the 
		//		MIB
	__time64_t start_cycle_time;

		// check whether anything needs to be repeated: called every 1/2 sec
	bool PollAwaitingAck();
		// record for each thread
//	MessageAckState mgt_msg;	[moved to parent class]
	MessageAckState upd_msg;	// messages that affect <upd_state>

	std::vector<ConnReqInfo> conn_pend;	// record for each conn req not completed

		// find the neighbour on port <p>
	MgtSocket * LinkPartner(int p);
		// check which neighbours are accessible
	void Trace();
	bool traced;

		// update display if required, to reflect change in MIB or console text 
		// in the console case, we also select console if this unit is displayed; 
		//		we assume it's not displaying a SCP interface because then that 
		//		(instead of the unit's console) would have fielded the input
	void UpdateMibDisplay() { if (theApp.controller_doc->UnitIs(this) && 
							theApp.controller_doc->display_select == MIB_DISPLAY) 
									theApp.controller_doc->UpdateAllViews(NULL); }
	void UpdateConsoleDisplay() { if (theApp.controller_doc->UnitIs(this)) {
						theApp.controller_doc->display_select = CONSOLE_DISPLAY; 
									theApp.controller_doc->UpdateAllViews(NULL); }}

		// routine called from idle loop to check for non-urgent tasks
	void OnIdle();
};

