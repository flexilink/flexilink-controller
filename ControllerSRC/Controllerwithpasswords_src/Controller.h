// Controller.h : main header file for the Controller application
//
// Copyright (c) 2014-2022 Nine Tiles

#pragma once
#include "../Common/string_extras.h"

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

// globals
extern class CControllerApp theApp;

// list of fonts we've created
// the only time any of the fonts is destroyed is on exit when the list is 
//		emptied; therefore, any pointer to a font can be expected to be valid 
//		as long as the list is nonempty
typedef CList<CFont *, CFont *> FontList;
extern FontList fonts;

extern CFont * FindFont(int height);


// value with which to initialise byte arrays for messages
// AES51 header with type 0x26 is set up on entry, IT header flow label set up 
//		for a signalling message once the link is up, length not initialised
extern ByteString aes51_data_hdr;	// 10 bytes


/////////////////////////////////////////////////////////////////////////////
// CCommandLineOptions:
// allows the -call etc command line options to be recognised
//

class CCommandLineOptions : public CCommandLineInfo
{
public:
	CCommandLineOptions();
	virtual ~CCommandLineOptions();

// Overrides
	//{{AFX_VIRTUAL(CCommandLineOptions)
	virtual void ParseParam(const TCHAR* pszParam, BOOL bFlag, BOOL bLast);
	//}}AFX_VIRTUAL
};


// CControllerApp:
// See Controller.cpp for the implementation of this class
//

class CControllerApp : public CWinApp
{
public:
	CControllerApp();
	virtual int ExitInstance();

	CMultiDocTemplate* pDocTemplate;
	CMultiDocTemplate* pCrosspointTemplate;

		// context while parsing command line if no '-' or '/'
		// 1 = server_addr, 2 = privilege value, 3 = unknown, 
		//		0 = nothing expected
	int next_param;
		// information from command line
	uint8_t privilege; // as in 62379-1, defaults to PRIV_OPERATOR, 0 = invalid
#define PRIV_LISTENER	 1 // min privilege, no password needed
#define PRIV_OPERATOR	 2
#define PRIV_SUPERVISOR	 3
#define PRIV_MAINTENANCE 4
		// flag to show whether to include the "console" window but actually 
		//		it's always there and can be restored by clicking in the bottom 
		//		left of the workspace
//	bool include_console; // maintenance level or "-debug"
	bool vm4scp; // true to use VM4 formats for SCP debug, false for VM3.2

		// information for LinkSocket::Init
		// may be filled in from command line, else defaults to empty
		// unlike previous versions, we don't fill in the actual address here, 
		//		only in <link_socket->link_ip_addr>
	CString server_addr;	// dotted-decimal IP address; use b'cast if empty

		// list of all the sockets and "documents"
		// these members are public and are administered by the objects they 
		//		point to; it would have been more within the spirit of C++ for 
		//		the app class to administer them
		// <link_socket> is NULL until the Controller document has been created, 
		//		thereafter it is always a valid pointer, though the object it 
		//		points to may be changed if the link fails and the timeout 
		//		routine replaces it; note that no management sockets are created 
		//		until after a link has been connected, so management sockets can 
		//		assume <link_socket> is valid
		// <link_partner> is NULL if there is no management socket for the link 
		//		partner; if no link is connected it will apply to the previous 
		//		link partner
		// the index to <units> is the flow number used on the link; the entry 
		//		with index 0 is not used; all unused entries are NULL
	class LinkSocket * link_socket;
	class MgtSocket * link_partner;
	bool pre_connection;	// haven't connected to anything
	CArray<class MgtSocket *, MgtSocket *> units;
	int last_unit_allocd;
	class CControllerDoc * controller_doc;
	class CCrosspointDoc * input_list;
	class CCrosspointDoc * output_list;
		// create a MgtSocket object and add it to <units>
	class MgtSocket * NewUnit(ByteString call_addr);
		// routine called by the link socket when the link comes up
	void NewLinkPartner(ByteString id);
		// find unit in <scp> or add a new one
	class AnalyserDoc * FindScpServer(MgtSocket * u, int p, int steps = 2);
		// remove references to <u> from <Analyser> objects
	void RemoveFromAnalysers(MgtSocket * u);
		// find socket with label <n>
	class FlexilinkSocket * FindSocket(int n);

		// list of all the connections to SCP servers; NULL means the object no 
		//		longer exists; we don't shuffle them to close the gap because 
		//		the indexes mustn't change
	std::vector<AnalyserDoc *> scp;

		// list of all the addresses of units we know about
		// value is a <MgtSocket *>; may be NULL
		// +++ it would be safer for the value to be an index into <units> but 
		//		there's nothing like CMapStringToInt; ought to change to use 
		//		something like std::map<std::string, int> (or with ByteString 
		//		as the key)
		// key is the nPortPartnerAddr in Flexilink format (05 + 16 hex digits 
		//		of EUI64, as a string of ASCII hex digits), or derived from 
		//		unitAddress in the case of the first unit connected to
	CMapStringToPtr unit_addrs;

		// list of senders of all the flows we know about
		// value is a <MgtSocket *>; may be NULL
		// +++ see note above
		// key is the flow id in dotted decimal, including the initial 16 arc
		// we don't prune entries when flows are disconnected; they need to be 
		//		pruned if we remove a unit, and over time the table might get 
		//		unreasonably large
	CMapStringToPtr flow_senders;

		// flag which is set whenever anything (in any of the MIBs) that is 
		//		relevant to the "crosspoint" windows is updated, and cleared 
		//		when the windows are repainted
		// if clear, we know the screen is up-to-date; the screen could also be 
		//		up-to-date if it is set, for instance if the update happened 
		//		before the object was displayed or was to an object that doesn't 
		//		affect the displayed information
		// NOTE: I have been lazy here; it is set whenever anything changes in 
		//		any of the MIBs, even if the thing that has changed doesn't 
		//		affect the information that is displayed
	bool mib_changed;

		// report of most recent error responses (if any)
	CStringArray err_msgs;

		// list of names to be translated: this was really a kludge to be used 
		//		until we could set port names in a non-volatile fashion in the 
		//		boxes, but may be useful in other ways
//	CMapStringToString name_translations;

	int update_flags;	// -ve = haven't asked yet, 0 = no, +ve = yes

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
	virtual BOOL OnIdle(LONG lCount);
};

extern CControllerApp theApp;


/////////////////////////////////////////////////////////////////////////////

// utility routines: similar to string_extras but Microsoft-specific

extern CString ByteArrayToHex(CByteArray& b);
extern CString ByteArrayToHex(ByteString& b);

