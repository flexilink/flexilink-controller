// Controller.cpp : Defines the class behaviours for the application.
//
// Copyright (c) 2014-2022 Nine Tiles

#include "stdafx.h"
#include "Controller.h"
#include "MainFrm.h"
#include "ChildFrm.h"
#include "AnalyserDoc.h"
#include "ControllerView.h"
#include "CrosspointDoc.h"
#include "CrosspointView.h"
//#include ".\controller.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CControllerApp

BEGIN_MESSAGE_MAP(CControllerApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
END_MESSAGE_MAP()


// CControllerApp construction

CControllerApp::CControllerApp()
{
		// Microsoft says "Place all significant initialization in InitInstance"
		// I moved most of it out of here in case that was the reason the pointers 
		//		were getting overwritten (it turned out to be the CByteArray 
		//		members corrupting them, fixed by doing "rebuild solution")
	next_param = 0;
	privilege = PRIV_OPERATOR;
//	include_console = false;
	vm4scp = false;
	link_socket = NULL;
	link_partner = NULL;
	pre_connection = true;
	last_unit_allocd = 0;
	controller_doc = NULL;
	input_list = NULL;
	output_list = NULL;
	mib_changed = false;
	update_flags = -1;
}

CCommandLineOptions::CCommandLineOptions()
{
}

CCommandLineOptions::~CCommandLineOptions()
{
}


// The one and only CControllerApp object

CControllerApp theApp;

// other globals

FontList fonts;
ByteString aes51_data_hdr;


// search <fonts> for a font the right height, creating it if necessary
// may not be called by "low layer" code
CFont * FindFont(int height)
{
	CFont * TextFont;
	POSITION next_font = fonts.GetHeadPosition();

		// search the list
	while (next_font) {
		TextFont = fonts.GetNext(next_font);
		LOGFONT info;
		TextFont->GetLogFont(&info);
		if (info.lfHeight == height) return TextFont; }

		// here if not found, to create a new one
	TextFont = new CFont();
	if (TextFont->CreateFont(height, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, 
						OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, 
						FIXED_PITCH|FF_MODERN, (LPCTSTR)"Lucida Console")) {
		fonts.AddHead(TextFont);
		return TextFont;
	}

		// if failed to create
	AfxMessageBox("Cannot create font", MB_OK | MB_ICONEXCLAMATION);
	delete TextFont;
	return NULL;
}

// CControllerApp initialization

// parsing parameters: called for each word <pszParam> on the command line
// <bFlag> is true if '/' or '-' was removed from the front of <pszParam>
// <bLast> is true for the last word on the command line
// if the parameter isn't recognised, we ignore it if bFlag is true, pass 
//		it to the base class else
void CCommandLineOptions::ParseParam(const TCHAR* pszParam, 
													BOOL bFlag, BOOL bLast)
{
	if (bFlag) {
		CString s = pszParam;
		s.MakeLower();

		if (s == "server") {
			theApp.next_param = 1;	// if parameter is "-server"
			return;
		}

		if (s == "privilege" || s == "priv") {
			theApp.next_param = 2;	// if parameter is "-privilege"
			return;
		}

		if (s == "vm4") theApp.vm4scp = true;
		else if (s == "listener") theApp.privilege = PRIV_LISTENER;
		else if (s == "operator") theApp.privilege = PRIV_OPERATOR;
		else if (s == "supervisor") theApp.privilege = PRIV_SUPERVISOR;
//		else if (s == "debug") theApp.include_console = true;
		else if (s == "maintenance") {
				// send link request to a "link-local" address, which is 
				//		not supposed to be forwarded by routers, to 
				//		restrict maintenance-level controllers to physical 
				//		connections
			theApp.server_addr = "169.254.9.9";
			theApp.privilege = PRIV_MAINTENANCE;
//			theApp.include_console = true;
		}
		else {
				// we didn't recognise it, let the base class deal with it
			CCommandLineInfo::ParseParam(pszParam, bFlag, bLast);
			theApp.next_param = 3;
			return;
		}
	}
	else switch (theApp.next_param) {
case 1:		// is an address following "-server"
		theApp.server_addr = pszParam;
		break;

case 2:		// is a number following "-privilege"
		int n;
		if (sscanf_s(pszParam, "%i", &n) == 1 && n > 0 && n <= 4) {
			theApp.privilege = n;
			break;
		}
		
			// else set invalid
		theApp.privilege = 0;
		break;

case 3:		// we didn't recognise it, let the base class deal with it
		CCommandLineInfo::ParseParam(pszParam, bFlag, bLast);
		return;
	}

	theApp.next_param = 0;
}


BOOL CControllerApp::InitInstance()
{
		// see notes in the constructor
	input_list = NULL;
	output_list = NULL;

	aes51_data_hdr.resize(10);
	aes51_data_hdr[0] = 2;
	aes51_data_hdr[1] = 0x26;
	aes51_data_hdr[2] = 0xFF;
	aes51_data_hdr[3] = 0xFF;
	aes51_data_hdr[4] = 0xFF;
	aes51_data_hdr[5] = 0xFF;

	units.SetSize(1);	// create the dummy entry for flow label 0

		// InitCommonControls() is required on Windows XP if an application
		// manifest specifies use of ComCtl32.dll version 6 or later to enable
		// visual styles.  Otherwise, any window creation will fail.
	InitCommonControls();

	CWinApp::InitInstance();

	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

		// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}
	AfxEnableControlContainer();

		// Standard initialization
		// If you are not using these features and wish to reduce the size
		// of your final executable, you should remove from the following
		// the specific initialization routines you do not need

		// Set the registry key under which our settings are stored.
	SetRegistryKey("Nine Tiles Controller");

	LoadStdProfileSettings(16);  // Load standard INI file options (including MRU)

		// Register the application's document templates.  Document templates
		//  serve as the connection between documents, frame windows and views
	pDocTemplate = new CMultiDocTemplate(IDR_ControllerTYPE,
		RUNTIME_CLASS(CControllerDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CControllerView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);

	pCrosspointTemplate = new CMultiDocTemplate(IDR_CrosspointTYPE,
		RUNTIME_CLASS(CCrosspointDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CCrosspointView));
	if (!pCrosspointTemplate)
		return FALSE;
	AddDocTemplate(pCrosspointTemplate);

		// create main MDI Frame window
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
	m_pMainWnd = pMainFrame;

		// call DragAcceptFiles only if there's a suffix
		//  In an MDI app, this should occur immediately after setting m_pMainWnd
		// Enable drag/drop open
//	m_pMainWnd->DragAcceptFiles();
		// Enable DDE Execute open
	EnableShellOpen();
	RegisterShellFileTypes(TRUE);

		// Parse command line: see <CCommandLineOptions::ParseParam> above
	next_param = 0;
	CCommandLineOptions cmdInfo;
	ParseCommandLine(cmdInfo);

		// create the two "crosspoint routing" documents, one for inputs and 
		//		the other for outputs
		// Note that we call OpenDocumentFile() because CreateNewDocument() 
		//		doesn't call OnNewDocument()
	if (pCrosspointTemplate->OpenDocumentFile(NULL) == NULL) {
		AfxMessageBox("Could not create the list-of-destinations \"document\"");
		return FALSE;
	}
	if (pCrosspointTemplate->OpenDocumentFile(NULL) == NULL) {
		AfxMessageBox("Could not create the list-of-sources \"document\"");
		return FALSE;
	}

		// similarly create the "Controller" document
	if (pDocTemplate->OpenDocumentFile(NULL) == NULL) {
		AfxMessageBox("Could not create the controller \"document\"\n"
										  "Check the network is connected");
		return FALSE;
	}

	unless (privilege == PRIV_MAINTENANCE) {
			// minimise the "Controller" window, so that tiling will fill 
			//		the area with the two crosspoint windows; note that 
			//		we still need it because it reads the configuration 
			//		files and creates the <LinkSocket> object (see 
			//		<CControllerDoc::OnNewDocument>) though maybe all 
			//		that could be done here instead?
			// +++ KLUDGE ALERT: I'm not sure why CloseWindow has to be 
			//		applied to the grandparent; <v> seems to be the area 
			//		that OnDraw writes and minimmising it just puts a 
			//		blank rectangle in the top left; the same happens if 
			//		the parent is minimised, also if the "minimise" flag is 
			//		set in <cs.style> in CControllerView::PreCreateWindow
		POSITION pos = controller_doc->GetFirstViewPosition();
		while (pos != NULL) {
			CControllerView * v = dynamic_cast<CControllerView *>
										(controller_doc->GetNextView(pos));
				// NB CloseWindow minimises; what is usually thought of as 
				//		closing is done by DestroyWindow
			if (v) v->GetParent()->GetParent()->CloseWindow();
		}
	}

		// The main window has been initialized, so show and update it
	STATIC_DOWNCAST(CMainFrame, m_pMainWnd)->MDITile(MDITILE_VERTICAL);
//	STATIC_DOWNCAST(CMainFrame, m_pMainWnd)->MDITile(MDITILE_HORIZONTAL);
	m_pMainWnd->ShowWindow(m_nCmdShow);
//	m_pMainWnd->ShowWindow(SW_NORMAL);
	m_pMainWnd->UpdateWindow();
	return TRUE;
}


// tidy up on exit
int CControllerApp::ExitInstance() {
	int r = CWinApp::ExitInstance();
	while (!fonts.IsEmpty()) delete fonts.RemoveHead();
		// remove all the management sockets; note that they send 
		//		ClearDown messages, so we can't remove the link socket 
		//		until afterwards
		// we clear <controller_doc> so the socket currently being 
		//		displayed won't try to do anything with parts of it 
		//		that have already been removed
	controller_doc = NULL;
	int i = (int) units.GetCount();
	while (--i >= 0) { delete units.GetAt(i); units.SetAt(i, NULL); }
	i = (int) scp.size();
	while (--i >= 0) { delete scp.at(i); scp.at(i) = NULL; }
	delete link_socket;
	return r;
}


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	// Implementation
protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnStnClicked65535();
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	ON_STN_CLICKED(65535, &CAboutDlg::OnStnClicked65535)
END_MESSAGE_MAP()

// App command to run the dialog
void CControllerApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}


// routine called after messages processed; return whether should be 
//		called again immediately
// <lCount> is the number of previous calls since the last message
BOOL CControllerApp::OnIdle(LONG lCount)
{
	BOOL r = CWinApp::OnIdle(lCount);
	if (lCount < 3) return TRUE;

	MgtSocket * m;
	int i;
	switch (lCount) {
case 3:
			// check whether MgtSocket objects have anything to do
		i = (int)(units.GetCount());
		while (--i > 0) {
			m = units.GetAt(i);
				// sanity check on <m> includes look for 0xFEEEFEEE
			if (m && m->state >= 0) m->OnIdle();
		}
		return TRUE;

case 4:
		unless (mib_changed) return r;
		mib_changed = false;

			// check whether there are any units we don't have management 
			//		sockets for yet, or for which it needs to reconnect
			// +++ ought not to do this unless the change to the MIB affects 
			//		the connectivity
		if (link_partner && link_partner->state == MGT_ST_ACTIVE) {
			i = (int)(units.GetCount());
			while (--i > 0) {
				m = units.GetAt(i);
				if (m) m->traced = false;
			}

			link_partner->Trace();
		}

			// redraw windows
		if (input_list != NULL) input_list->UpdateAllViews(NULL);
		if (output_list != NULL) output_list->UpdateAllViews(NULL);
	}
	return r;
}


// called by the link socket when the Link Accept is received; <id> is the link 
//		partner's address
// creates a management socket for the link partner if we don't already have one
// prompts any existing management sockets to attempt reconnection
void CControllerApp::NewLinkPartner(ByteString id)
{
	int i = (int)units.GetCount();
	if (i > 1) {
			// have management sockets from a previous link
			// reconnect them
		do {
			MgtSocket * m_skt = units.GetAt(--i);
			if (m_skt) m_skt->SendConnReq();
		} while (i > 0);

		void * q;
		if (unit_addrs.Lookup(ByteArrayToHex(id), q)) {
				// the new link partner is among them
			link_partner = (MgtSocket *)q;
			return;
		}
	}

		// here to create a management object for the link partner
	link_partner = NewUnit(id);
	if (link_partner) controller_doc->SetUnit(link_partner);
}


// create a MgtSocket object and add it to <units>; returns a pointer to the 
//		new object, or NULL if failed
class MgtSocket * CControllerApp::NewUnit(ByteString call_addr)
{
	MgtSocket * m = new MgtSocket();
	unless (m) return NULL;

		// Allocate a flow label; if the table isn't very large, just allocate 
		//		a new one, else see whether there's one we can re-use
	int n = (int)units.GetCount();
	if (n < 50) goto extend_list;
	int j = last_unit_allocd;
	do {
		j += 1;
		if (j >= n) j = 1;
		if (j == last_unit_allocd) {
			if (n < 256) goto extend_list;
			return NULL;	// no labels left in the range we use for units
		}
	} while (units.GetAt(j) != NULL);
	units.SetAt(j, m);
	n = j;
	goto have_index;

extend_list:
	units.Add(m);
have_index:	// here when the unit has been added at index <n>
	last_unit_allocd = n;
	m->call_ref = n;
	m->unit_name.Format("[unit %d]", n);
	m->unit_TAddress = call_addr;
	m->unit_address = ByteArrayToHex(call_addr);
	unit_addrs.SetAt(m->unit_address, (void *)m);
	m->SendConnReq();
	return m;
}


// search <scp> for an object with partner <u> and partner port <p> or, if <p> 
//		is negative, with host <u>; returns a pointer to the object if found, 
//		NULL else
// <steps> shows how far to take the process in the first case: 0 = just search 
//		the table, 1 = if not found and there's space available in <scp> create 
//		a new one and return that, 2 (default) = also initiate connection if in 
//		_BEGIN state
// won't ask for connection if partner doesn't have a "random string" available
// when <p> is negative, the <steps> value is ignored and treated as 0
class AnalyserDoc * CControllerApp::FindScpServer(MgtSocket * u, int p, int steps)
{
		// <u> must not be NULL
	if (u == NULL) return NULL;

		// search the table for the unit id, also for a free location
	int i = scp.size();
	int j = -1;
	AnalyserDoc * a;

	while (--i >= 0) {
		a = scp.at(i);
		if (a == NULL) {
			j = i;
			continue;
		}
		if (p < 0) { unless (a->Host() == u) continue; }
		else unless (a->partner == u && (a->partner_port == p)) continue;

		if (steps > 1 && (a->state == MGT_ST_BEGIN || a->state > MGT_ST_MAX_OK)) {
			a->SendConnReq(); // (re)try connection
			return a;
		}
	}

		// here if not found
	if (steps == 0 || p < 0 || u == NULL) return NULL;
		// here to create a new <AnalyserDoc> object
		// there are no free entries if <j> is negative, else entry <j> is free
	if (j < 0) {
			// need a new entry
		j = scp.size();
		if (j > 255) return NULL; // can't add any more
		scp.push_back(NULL);
	}
	a = new AnalyserDoc(u, p);
	scp.at(j) = a;
	a->call_ref = j | 256;

	if (steps > 1) a->SendConnReq();
	return a;
}


// check all <Analyser> objects for references to <u>; if <partner> delete the 
//		<Analyser> object, if <host> set it to NULL
void CControllerApp::RemoveFromAnalysers(MgtSocket * u)
{
	int i = scp.size();
	AnalyserDoc * a;

	while (--i >= 0) {
		a = scp[i];
		if (a == NULL) continue;
		if (a->partner == u) delete a; // destructor sets <scp[i]> to NULL
	}
}


// return the object to which to route packets with label <n>, NULL if none
class FlexilinkSocket * CControllerApp::FindSocket(int n)
{
	if (n > 0 && n < units.GetSize()) return units.GetAt(n);
	n -= 256;
	if (n >= 0 && n < (int)scp.size()) return scp.at(n);
	return NULL;
}


/////////////////////////////////////////////////////////////////////////////

// utility routines: similar to string_extras but Microsoft-specific

CString ByteArrayToHex(CByteArray& b)
{
	CString s;
	int i = 0;
	INT_PTR n = b.GetSize();
	uint8_t * p = b.GetData();
	while (i < n) {
		int j = *p++;
		s.AppendFormat("%02X", j);
		++i;
	}
	return s;
}

CString ByteArrayToHex(ByteString& b)
{
	CString s;
	int i = 0;
	INT_PTR n = b.size();
	while (i < n) {
		int j = b[i];
		s.AppendFormat("%02X", j);
		++i;
	}
	return s;
}

void CAboutDlg::OnStnClicked65535()
{
	// TODO: Add your control notification handler code here
}
