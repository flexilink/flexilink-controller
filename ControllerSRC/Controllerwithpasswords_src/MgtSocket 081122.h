#pragma once


// +++ ought to read in the MIB from a .9tlrem file in CControllerDoc::Serialize()
struct ObjectInfo {
	CString caption;
	CByteArray oid;
	int ttl;			// secs until must re-fetch; -ve if <value> invalid
	CByteArray value;	// as a byte string; for integers is always 4 bytes
	bool octet_string;	// else is an integer
};

// Structure to hold info about a network port
struct NetPortInfo {
	int block_id;
	CString name;
	int state;
	int partner_addr_type;		// see below
	unsigned __int64 partner_eui; // zero if none or not known
};
#define ADDR_TYPE_UNKNOWN	0	// not been reported or not recognised
#define ADDR_TYPE_FLEXILINK	1	// 1.3.6.1.4.1.9940.2.1
#define ADDR_TYPE_EUI64		2	// OID not currently defined


// Structure to hold info about an audio port
struct AudioPortInfo {
	int block_id;
	CString name;
	int direction;
	int format;		// see below; following four fields are parameters
	int chan_arrangement;	// 'a' in OIDs for format codes below
	int chan_count;			// 'c' in OIDs for format codes below
	int bit_depth;			// 'b' in OIDs for format codes below
	int sampling_freq;		// 'f' in OIDs for format codes below
};
#define DIRECTION_IN	1
#define DIRECTION_OUT	2
#define AUDIO_FORMAT_UNKNOWN	 0	// not been reported or not recognised
#define AUDIO_FORMAT_NONE		 1	// 1.0.62379.2.2.1.1
#define AUDIO_FORMAT_ANALOGUE	 2	// 1.0.62379.2.2.1.2.a.c
#define AUDIO_FORMAT_PCM		 3	// 1.0.62379.2.2.1.3.a.c.b.f
#define AUDIO_FORMAT_INVALID	13	// 1.0.62379.2.2.1.13


// Structure to hold info about a connector
struct ConnectorInfo {
	int dest_block_id;
	int dest_input;
	int srce_block_id;
	int srce_output;
}


// Structure to hold info about a call source
struct SrceCallInfo {
	unsigned char flow_id[16];
	int state;
	int block_id;
	int input;
}


// Structure to hold info about a call destination
struct DestCallInfo {
	unsigned char flow_id[16];
	int state;
	CString source;
	int block_id;
	int output;
}




// There is a MgtSocket object for each unit on the Flexilink network that we 
//		know about. It collects information about the unit and looks after making 
//		and clearing down the unit's network connections. It also "owns" the SIP 
//		socket through which its connection is made; there is a 1:1 relationship 
//		between management sockets and SIP sockets.

class MgtSocket : public CAsyncSocket
{
public:
	MgtSocket();
	MgtSocket(class CControllerDoc * pDoc);
	virtual ~MgtSocket();
	virtual void OnReceive(int nErrorCode);

		// "connect" as indicated and send first request
	void Init(CString addr, UINT port);

		// request the value of object <n>
//	void RequestBasicObject(int n);

		// document which owns this socket
	class CControllerDoc * owner_doc;

		// SIP socket which this object owns
	class SipSocket * sip_skt;

		// current state: one of the following
#define MGT_ST_BEGIN	 0	// nothing done yet
#define MGT_ST_HAVE_MAC	 1	// <unit_mac_address> is valid; Status Request has been sent
#define MGT_ST_ACTIVE	 2	// Status Response has been received
#define MGT_ST_FAILED	 6	// call to base class (Create, Connect, Send) failed
#define MGT_ST_ERROR	 7	// any kind of protocol error
	int state;

		// the EUI-64 to which the management connection was addressed
	unsigned __int64 called_eui;

	unsigned char unit_info_oid[9]; // tag, length, and most of OID for unit-information
	CString unit_name;
	CString unit_location;
	unsigned char unit_mac_addr_octets[6];
	unsigned __int64 unit_mac_address;
	unsigned char unit_identity[10]; // OUI (3), mfr (2), prod code (4), mod level (1)
	CString mfr_name;
	CString product_name;
	CString serial_number;
	CString firmware_version;
	int up_time;	// in seconds

		// lists of objects reported, in same the "lexicographic order" as for GetNext 
		//		except where noted
	CList<NetPortInfo, NetPortInfo&> net_port_list;
	CList<AudioPortInfo, AudioPortInfo&> audio_port_list;
	CList<ConnectorInfo, ConnectorInfo&> connector_list;
	CList<SrceCallInfo, SrceCallInfo&> srce_call_list; // in random order
	CList<DestCallInfo, DestCallInfo&> dest_call_list; // in random order

		// record of the latest value we've received for each OID for which we've 
		//		received a value
		// <mib> holds raw messages excl 1st 3 bytes (so the first byte is the length 
		//		of the OID), in order of increasing OID, but only those for which the 
		//		length of the OID is a single byte
		// intended only for monitoring purposes; any object for which we keep the 
		//		value (e.g. port states) must have its value stored separately
	CStringList mib;
	void UpdateMib(unsigned char * b, int len);

		// messages: only save the first 100 or so
	CStringArray msgs;		// in hex
	void SaveMessage(unsigned char * b, int len, char flag);
};


