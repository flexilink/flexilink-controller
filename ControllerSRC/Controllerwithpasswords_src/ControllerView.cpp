// ControllerView.cpp : implementation of the CControllerView class
//

#include "stdafx.h"
#include "Controller.h"
#include "AnalyserDoc.h"	// includes ControllerDoc.h
#include "ControllerView.h"
//#include ".\controllerview.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// change to separate H and V margins if required
#define MARGIN_SIZE 10


// CControllerView

IMPLEMENT_DYNCREATE(CControllerView, CScrollView)

BEGIN_MESSAGE_MAP(CControllerView, CScrollView)
	//{{AFX_MSG_MAP(CControllerView)
	ON_WM_CHAR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// CControllerView construction/destruction

CControllerView::CControllerView()
{
	Char5Width = 40;
	CharHeight = 12;
	text_font = FindFont(CharHeight);
}

CControllerView::~CControllerView()
{
}

BOOL CControllerView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CScrollView::PreCreateWindow(cs);
}

// CControllerView drawing

void CControllerView::OnDraw(CDC* pDC)
{
	CControllerDoc* pDoc = GetDocument();
	if (pDoc == NULL) return;
	ASSERT_VALID(pDoc);

	if (fonts.IsEmpty()) return;

	pDC->SelectObject(text_font);
	pDC->SetTextAlign(TA_LEFT | TA_TOP);	// | TA_UPDATECP

	CSize size5 = pDC->GetOutputTextExtent("12345");
	Char5Width = size5.cx;
	CharHeight = size5.cy;

		// Initialisation
	int mgn_left = MARGIN_SIZE;
	int mgn_top = MARGIN_SIZE;
	int x = mgn_left;
	int y = mgn_top;

	int i;
	int j;
	INT_PTR n;
	CString str;
	CString s3;
	MgtSocket * m;

	if (pDoc->display_select >= 0) {
			// display a SCP interface
		AnalyserDoc * a = theApp.scp.at(pDoc->display_select);
		if (a == NULL) {
			pDC->SetTextColor(0x0000FF); // red
			pDC->TextOut(x, y, "Connection to SCP server lost");
			return;
		}

			// signalling messages
		if (a->display_messages) {
			y = a->AddMsgsToDisplay(x, y, CharHeight, pDC);
			y += CharHeight / 2;
			pDC->SetTextColor(0); // black
			pDC->SetBkColor(0xFFFFFF); // on white
			s3.Format("tx flow %d", a->tx_flow);
			pDC->TextOut(x, y, s3);
			y += CharHeight * 2;
		}

			// output text
		pDC->SetTextColor(0); // black
		pDC->SetBkColor(0xFFFFFF); // on white
		n = a->console.size();
		i = -1;
		while (++i < n) {
			pDC->TextOut(x, y, a->console.at(i).c_str());
			y += CharHeight;
		}
		y += CharHeight;

			// command input
		pDC->SetBkColor(0x40FFFF); // pale yellow highlight
		str = "       ";
		str += a->pending_console_input.c_str();
		pDC->TextOut(x, y, str + "       ");
		y += CharHeight * 2;
		pDC->SetBkColor(0xFFFFFF); // remove highlight
		pDC->SetTextColor(0); // black

			// unit id
		str.Format("1.0.62379.5.1.1.1.1.1.9.%d", a->partner_port); // nPortScpDevice
		str = a->partner->GetStringHex(str, 1);
		if (str == "00 90 A8 00") str = "debug server";
		else str = "SCP server type " + str;
		m = a->Host();
		if (m && (s3 = m->DisplayName(), !s3.IsEmpty())) str += " in " + s3;
		else {
			str.Format(" via port %d of ", a->partner_port);
			str += a->partner->DisplayName();
		}
		pDC->TextOut(x, y, str);
		y += CharHeight;

		switch (a->state) {
case MGT_ST_BEGIN:
			str = "New SCP client";
			break;

case MGT_ST_CONN_REQ:
			str = "Connection requested ...";
			break;

case MGT_ST_CONN_ACK:
			str = "Connecting ...";
			break;

case MGT_ST_ACTIVE:
			s3 = a->ServerState().c_str();
			unless (s3.IsEmpty()) {
				pDC->TextOut(x, y, s3);
				return;
			}
case MGT_ST_CONN_MADE:
			str = "Awaiting status ...";
			break;

case MGT_ST_NOT_CONN:
			str = "Unable to connect to SCP server";
			break;

case MGT_ST_FAILED:
			str = "Connection to IP network failed";
			break;

case MGT_ST_CLOSED:
			str = "Connection to SCP server has been closed";
			break;

case MGT_ST_ERROR:
			str = "Protocol error";
			break;

default:
			str.Format("Unknown protocol state, code %d", a->state);
		}

			// red for errors, magenta if not connected yet
		pDC->SetTextColor((a->state > MGT_ST_MAX_OK) ? 0x0000FF : 0xFF00FF);
		pDC->TextOut(x, y, str);
		return;
	}

		// here if not a SCP interface
	m = pDoc->unit;

	unless (pDoc->failure_notice.IsEmpty()) {
		pDC->SetTextColor(0x0000FF); // red
		pDC->TextOut(x, y, pDoc->failure_notice);
		y += CharHeight * 2;
	}

	if (theApp.link_socket == NULL) 
			str = "No local termination for link to Flexilink network";
	else switch (theApp.link_socket->state) {
case LINK_ST_BEGIN:
		str = "Link to Flexilink cloud not initialised";
		break;

case LINK_ST_REQ:
		str = "Searching for Flexilink cloud ...";
		break;

case LINK_ST_ACTIVE:
		goto after_link_state;

case LINK_ST_CLOSED:
		str = "Link to Flexilink cloud has been disconnected";
		break;

case LINK_ST_FAILED:
		str = "Connection to IP network failed";
		break;

case LINK_ST_ERROR:
		str = "Protocol error on link to Flexilink cloud";
		break;

default:
		str.Format("Unknown link state, code %d", 
											theApp.link_socket->state);
	}

	pDC->SetTextColor(0x0000FF); // red
	pDC->TextOut(x, y, str);
	if (theApp.pre_connection) {
		y += CharHeight;
		pDC->TextOut(x, y, "Are you sure Windows Firewall has "
								"this program enabled as an exception?");
	}
	y += CharHeight * 2;

	if (m == NULL) {
		pDC->TextOut(x, y, "No unit selected for display");
		return;
	}

after_link_state:
	pDC->SetTextColor(0xFF00FF); // magenta

	switch (m->state) {
case MGT_ST_BEGIN:
		str = "New management process";
		break;

case MGT_ST_CONN_REQ:
		str = "Connection requested ...";
		break;

case MGT_ST_CONN_ACK:
		str = "Connecting ...";
		break;

case MGT_ST_CONN_MADE:
		str = "Awaiting unit identification";
		break;

case MGT_ST_HAVE_INFO:
		str = "Awaiting first status report";	// awaiting ack to request
		pDC->TextOut(x, y, str);
		y += CharHeight * 2;
case MGT_ST_ACTIVE:
		pDC->SetTextColor(0); // black
		str = m->GetStringObject("1.0.62379.1.1.1.5.0"); // mfr name
		unless (str.IsEmpty()) {
			pDC->TextOut(x, y, str);
			y += CharHeight;
		}
		str = m->GetStringObject("1.0.62379.1.1.1.6.0"); // product name
		unless (str.IsEmpty()) {
			pDC->TextOut(x, y, str);
			y += CharHeight;
		}
		str = m->GetStringObject("1.0.62379.1.1.1.7.0"); // serial number
		unless (str.IsEmpty()) {
			pDC->TextOut(x, y, "Serial number " + str);
			y += CharHeight;
		}
		str = m->GetStringObject("1.0.62379.1.1.1.8.0"); // firmware
		unless (str.IsEmpty()) {
			pDC->TextOut(x, y, "Firmware " + str);
			y += CharHeight;
		}

		str = m->GetStringHex("1.0.62379.1.1.1.16.0", 1); // unit identification
		if (str.IsEmpty()) {
			str = m->GetStringHex("1.0.62379.1.1.1.3.0", 1); // MAC address
			if (str.IsEmpty()) str = "No MAC address reported";
			else str = "Unit MAC address " + str;
		}
		else str = "Unit identifier " + str;
		pDC->TextOut(x, y, str);
		y += CharHeight * 2;

		str = theApp.link_socket->standard_format ? 
				"Link uses standard format" : "Link uses legacy format";
		break;

case MGT_ST_NOT_CONN:
		str = "Unable to set up management connection";
		break;

case MGT_ST_FAILED:
		str = "Connection to IP network failed";
		break;

case MGT_ST_CLOSED:
		str = "Management connection has been closed";
		break;

case MGT_ST_TIMEOUT:
		str = "Unit not responding";
		break;

case MGT_ST_ERROR:
		str = "Management protocol error";
		break;

default:
		str.Format("Unknown management protocol state, code %d", m->state);
	}

	if (m->state > MGT_ST_MAX_OK) pDC->SetTextColor(0x0000FF); // red
	pDC->TextOut(x, y, str);
	y += CharHeight * 2;


	pDC->SetTextColor(0xFF00FF); // magenta
	switch (m->upd_state) {
case UPD_ST_NO_INFO:
		str = "Waiting for version etc info";
		break;

case UPD_ST_BEGIN:
		str = "Version etc info received";
		break;

case UPD_ST_NOT_RECOGD:
		str = "Product type not recognised";
		break;

case UPD_ST_NO_P_FILE:
		str.Format("Couldn't open product~%d-%d-%d-%d.9t3prd or .9t4prd", 
						m->product_code[0], m->product_code[1], 
							m->product_code[2], m->product_code[3]);
		break;

case UPD_ST_BAD_P_FILE:
		str.Format("Couldn't find current software versions in "
										   "product~%d-%d-%d-%d file", 
						m->product_code[0], m->product_code[1], 
							m->product_code[2], m->product_code[3]);
		break;

case UPD_ST_NO_ACTION:
		str = "Unit's software is up-to-date";
		pDC->SetTextColor(0x40BF00); // green
		break;

case UPD_ST_FROM_ISE:
		str = "Software not loaded from flash";
		pDC->SetTextColor(0xFF0000); // blue
		break;

case UPD_ST_COLLECT_MAP:
		str = "Reading contents of unit's flash";
		break;

case UPD_ST_HAVE_MAP:
		str = "Flash map downloaded";
		break;

case UPD_ST_UP_TO_DATE:
		str = "Unit needs reboot to run current software";
		pDC->SetTextColor(0x0000FF); // red
		break;

case UPD_ST_NOT_MAINT:
		str = "Insufficient privilege to access flash";
		pDC->SetTextColor(0xFF0000); // blue
		break;

case UPD_ST_USER_REFUSED:
		str = "User refused software update";
		pDC->SetTextColor(0xFF0000); // blue
		break;

case UPD_ST_BAD_FLASH:
		str = "Existing flash contents malformed";
		pDC->SetTextColor(0x0000FF); // red
		break;

case UPD_ST_NO_S_FILE:
		str = "Failed to collect data from image file";
		break;

case UPD_ST_NO_LOGIC:
		str = "Failed to open FPGA logic image file";
		break;

case UPD_ST_BAD_LOGIC:
		str = "FPGA logic image file empty or corrupt";
		break;

case UPD_ST_NO_VM_FILE:
		str = "Failed to open VM software image file";
		break;

case UPD_ST_BAD_VM_FILE:
		str = "VM software image file empty or corrupt";
		break;

case UPD_ST_UPLOADING:
		str = "Writing data to flash";
		break;

case UPD_ST_TIDY_UP:
		str = "Checking for areas that should be erased";
		break;

case UPD_ST_MAKE_SPACE:
		str = "Flash is full; checking for areas that should be erased";
		pDC->SetTextColor(0x0000FF); // red
		break;

case UPD_ST_ERASING:
		str = "Tidying away old data from flash";
		break;

case UPD_ST_WAITING:
		str = "Erase requested";	// +++ click to check whether complete
		pDC->SetTextColor(0xFF0000); // blue
		break;

case UPD_ST_FAILED:
		str = "Protocol failure in uploading process";
		break;

default:
		str.Format("Unknown state of uploading process, code %d", m->upd_state);
	}

	pDC->TextOut(x, y, str);
	y += CharHeight * 2;

	if (theApp.update_flags > 0) {
		pDC->SetTextColor(0x0000FF); // red
		pDC->TextOut(x, y, "Software update enabled for all units");
		y += CharHeight * 2;
	}
	pDC->SetTextColor(0); // black
	if (theApp.update_flags == 0) {
		pDC->TextOut(x, y, "Software update disabled for all units");
		y += CharHeight * 2;
	}

	if (pDoc->display_select == CONSOLE_DISPLAY) {
		pDC->TextOut(x, y, "Console output");
		y += CharHeight * 2;

		n = m->console.size();
		i = -1;
		while (++i < n) {
			pDC->TextOut(x, y, m->console.at(i).c_str());
			y += CharHeight;
		}

		return;
	}

		// else dump the MIB
	POSITION p = m->mib.GetHeadPosition();
	if (p != NULL) {
		do {
			MibObject * obj = m->mib.GetNext(p);
			s3 = obj->oid + " =";
			n = obj->GetCount();

			switch (obj->tag) {
case ASN1_TAG_INTEGER:
				s3.AppendFormat(" 0x%X = %i", obj->value, obj->value);
				goto write_line;

case ASN1_TAG_OCTET_STRING:
				j = 0;
				while (j < n) {
					uint8_t c = obj->GetAt(j);
					if (c < 0x20 || c > 0x7E) goto in_hex;
					j += 1;
				}

				s3 += " \"" + obj->StringValue() + '\"';
				goto write_line;

case ASN1_TAG_OID:
				str = obj->ConvertOid();
				if (!str.IsEmpty()) {
                    s3 += ' ' + str;
					goto write_line;
				}                        
default:
				s3.AppendFormat(" [tag %02X]", obj->tag);
			}

in_hex:		j = 0;
			while (j < n && j < 75) {
				unsigned int i = obj->GetAt(j);
				s3.AppendFormat(" %02X", i);
				j += 1;
			}
			if (j < n) s3 += " ...";

write_line:	pDC->TextOut(x, y, s3);
			y += CharHeight;
		} while (p != NULL);

		y += CharHeight;
	}

	if (m->mib_map.GetCount() != m->mib.GetCount()) {
		str.Format("*** %d in MIB, %d in map ***", m->mib.GetCount(), m->mib_map.GetCount());
		y += CharHeight;
		pDC->SetTextColor(0x0000FF); // red
		pDC->TextOut(x, y, str);
		y += CharHeight;
	}

		// dump the input and output flows
	y += CharHeight;
	p = m->output_flows.GetStartPosition();
	while (p) {
		m->output_flows.GetNextAssoc(p, str, s3);
		pDC->TextOut(x, y, "out " + str + ' ' + s3);
		y += CharHeight;
	}
	p = m->input_flows.GetStartPosition();
	while (p) {
		m->input_flows.GetNextAssoc(p, str, s3);
		pDC->TextOut(x, y, "in " + str + ' ' + s3);
		y += CharHeight;
	}

		// dump the management messages
	y += CharHeight;
	m->AddMsgsToDisplay(x, y, CharHeight, pDC);
}


// list the stored messages
// returns new <y> value; colours on return are undefined
int FlexilinkSocket::AddMsgsToDisplay(int x, int y, int CharHeight, CDC * pDC)
{
	int i = -1;
	while (++i < msgs.GetSize()) {
		switch (msgs.GetAt(i)[0]) {
case 'T':	pDC->SetTextColor(0); // black
			pDC->SetBkColor(0x00FFFF); // yellow
			break;

case 't':	pDC->SetTextColor(0); // black
			pDC->SetBkColor(0xFFFFFF); // white
			break;

case 'R':	pDC->SetTextColor(0x40BF00); // green
			pDC->SetBkColor(0x00FFFF); // yellow
			break;

case 'r':	pDC->SetTextColor(0x40BF00); // green
			pDC->SetBkColor(0xFFFFFF); // white
			break;

case 'E':	pDC->SetTextColor(0x0000FF); // red
			pDC->SetBkColor(0x00FFFF); // yellow
			break;

case 'H':	pDC->SetTextColor(0x0000FF); // red
			pDC->SetBkColor(0xFFFF00); // cyan
			break;

default:	pDC->SetTextColor(0x0000FF); // red
			pDC->SetBkColor(0xFFFFFF); // white
		}

		pDC->TextOut(x, y, msgs.GetAt(i).Mid(1));
		y += CharHeight;
	}

	return y;
}


void CControllerView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	CControllerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	MgtSocket * m = pDoc == NULL ? NULL : pDoc->unit;

	CRect r; GetClientRect(&r);
	SIZE area;
	int h, w;
	w = 400;		// Fix w ???

	if (m == NULL) h = 12;
	else if (pDoc->display_select >= 0) {
		AnalyserDoc * a = theApp.scp.at(pDoc->display_select);
		h = 20 + a->msgs.GetSize() + a->console.size();
	}
	else if (m->state < 0) h = 12;
	else if (pDoc->display_select == CONSOLE_DISPLAY) h = (int)m->console.size() + 20;
	else h = (int)(m->mib.GetCount() + m->msgs.GetSize() + m->mib.GetCount() + 23);

	area.cx = (20) + ((Char5Width * w)/4); // +++ ought to be /5 but then the messages don't fit
	area.cy = (10) + (CharHeight * h);
	CSize page(r.right/3, r.bottom - CharHeight);
	CSize line(Char5Width, CharHeight);
	SetScrollSizes(MM_TEXT, area, page, line);

	CScrollView::OnUpdate(pSender, lHint, pHint);
}


// over-ride of a routine that should field all "normal" characters that are typed 
//		into the window
// I don't call the base class version because I don't think it can do anything useful
// space switches to the console view, or the MIB view if already in the console view; 
//		'#' switches to the SCP server for a crashed VM; '@' switches to the SCP 
//		server for this unit; anything else switches to the console view and is sent 
//		in a console message; we don't look for an acknowledgement because the user 
//		will repeat it if there is no reply
// for the SCP server cases, we search for an existing object and if not found create 
//		a new one; if it isn't connected we initiate setting up the connection
void CControllerView::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
		// ignore if, say, user has restored the window when it should be minimised
		// +++ however, we mustn't rely on the Cantroller being well-behaved because 
		//		an attacker might write their own; defence against uauthorised use 
		//		of console and SCP commands must be done by the unit.
	unless (theApp.privilege == PRIV_MAINTENANCE) return;

	CControllerDoc* pDoc = GetDocument();
	if (pDoc == NULL) return;
	if (nChar == 0x1b) {	// escape
		pDoc->display_select = (pDoc->display_select == CONSOLE_DISPLAY) ? 
													MIB_DISPLAY : CONSOLE_DISPLAY;
		pDoc->UpdateAllViews(NULL);
		return;
	}

	if (nChar == '@') {
			// move to next SCP session
		pDoc->SelectNextScp();
		return;
	}

	AnalyserDoc * a;
	int n;
	if (pDoc->display_select < 0){
		MgtSocket * m = pDoc->unit;
		if (m == NULL) return;

		uint8_t b[14];
		b[0]  = 0x70;
		b[2]  = (uint8_t)nChar;
		m->TxNewMessage(b, 3, false);
		pDoc->display_select = CONSOLE_DISPLAY;
		pDoc->UpdateAllViews(NULL);
		return;
	}

		// here if a SCP session is selected
	if (theApp.scp.empty()) {
			// there aren't any
		pDoc->display_select = CONSOLE_DISPLAY;
		return;
	}

		// check that the selected session hasn't been cleared down; if it has, 
		//		select the next that hasn't; if they all have, switch to the 
		//		console view
	n = pDoc->display_select;
	until ((a = theApp.scp.at(n), a)) {
		n += 1;
		if ((unsigned int)n >= theApp.scp.size()) n = 0;
		if (n == pDoc->display_select) {
			pDoc->display_select = CONSOLE_DISPLAY;
			pDoc->UpdateAllViews(NULL);
			return;
		}
	}

	if (nChar == '@') {
			// nothing more to do
		pDoc->UpdateAllViews(NULL);
		return;
	}

		// here with <a> pointing to the selected session, which is not NULL
	if (nChar == '~') {
			// toggle inclusion of messages in the display
		a->display_messages = !a->display_messages;
		pDoc->UpdateAllViews(NULL);
		return;
	}

/*	if (nChar == 'x') {*
			// select SCP server for a crashed VM
		a = theApp.FindScpServer(0, -1);
		if (a == NULL) return;
		pDoc->display_select = a->label - 256;
		pDoc->UpdateAllViews(NULL);
		return;
	}
*/
		// here for other characters
	a->InputChar(nChar);

//	CScrollView::OnChar(nChar, nRepCnt, nFlags);
}


// CControllerView diagnostics

#ifdef _DEBUG
void CControllerView::AssertValid() const
{
	CScrollView::AssertValid();
}

void CControllerView::Dump(CDumpContext& dc) const
{
	CScrollView::Dump(dc);
}

CControllerDoc* CControllerView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CControllerDoc)));
	return (CControllerDoc*)m_pDocument;
}
#endif //_DEBUG


// CControllerView message handlers

void CControllerView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();

//	GetParent()->GetParent()->ShowWindow(SW_MINIMIZE);
}
