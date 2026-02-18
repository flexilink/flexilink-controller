// CrosspointView.cpp : implementation file
//

#include "stdafx.h"
#include "Controller.h"
#include "AnalyserDoc.h"	// includes ControllerDoc.h
#include "MgtSocket.h"
#include "CrosspointDoc.h"
#include "CrosspointView.h"
#include "PcodeChange.h"
//#include ".\crosspointview.h"

// change to separate H and V margins if required
#define MARGIN_SIZE 10


// CCrosspointView

IMPLEMENT_DYNCREATE(CCrosspointView, CScrollView)

CCrosspointView::CCrosspointView()
{
	Char5Width = 40;
	CharHeight = 12;
	text_font = FindFont(CharHeight);
	sel_port.p.unit = NULL;
}

CCrosspointView::~CCrosspointView()
{
}


// replace any pointers to <u> in <locs> with NULL
// safe if <this> is NULL
void CCrosspointView::RemoveFromLocs(MgtSocket * u) {
	if (this == NULL) return;
	if (sel_port.p.unit == u) sel_port.p.unit = NULL;
	INT_PTR n = locs.GetSize();
	int i = 0;
	while (i < n) {
		if (locs[i].p.unit == u) locs[i].p.unit = NULL;
		i += 1;
	}
}


BEGIN_MESSAGE_MAP(CCrosspointView, CScrollView)
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()


// CCrosspointView drawing

void CCrosspointView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	CScrollView::OnUpdate(pSender, lHint, pHint);

	CSize sizeTotal;
	sizeTotal.cx = Char5Width * 20;	// +++ 100 chars: rethink if names can be longer
	sizeTotal.cy = (LONG)(CharHeight * ((theApp.units.GetCount() * 8) + 3));
	SetScrollSizes(MM_TEXT, sizeTotal);
}

void CCrosspointView::OnDraw(CDC* pDC)
{
	if (fonts.IsEmpty()) return;
	pDC->SelectObject(text_font);
	pDC->SetTextAlign(TA_LEFT | TA_TOP);	// | TA_UPDATECP

	CCrosspointDoc* pDoc = GetDocument();
	bool inputs = pDoc->inputs;

	CSize size5 = pDC->GetOutputTextExtent("12345");
	Char5Width = size5.cx;
	CharHeight = size5.cy;

		// Initialisation
	int mgn_left = MARGIN_SIZE;
	int mgn_top = MARGIN_SIZE;
	int x = mgn_left;
	int y = mgn_top;

	INT_PTR n = theApp.units.GetUpperBound();
	int i = 0;
	int j = (int)n;
	while (++i <= n) if (theApp.units.GetAt(i) == NULL) --j;

	CString str;
	if (theApp.privilege >= PRIV_SUPERVISOR) {
		if (j <= 0) str = "No";
		else str.Format("%d", j);
		str += " management connection";
		if (j != 1) str += 's';
	}
	else if (j > 0) str = inputs ? "Media sources" : "Media destinations";
	else {
		if (theApp.link_socket == NULL) str = "No local termination for link to Flexilink network";
		else switch (theApp.link_socket->state) {
		case LINK_ST_BEGIN:
		str = "Link to Flexilink cloud not initialised";
		break;

		case LINK_ST_REQ:
		str = "Searching for Flexilink cloud ...";
		break;

		case LINK_ST_ACTIVE:
		str = "Link active but no management connection";
		break;

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
		str.Format("Unknown link state, code %d", theApp.link_socket->state);
		}

		pDC->SetTextColor(0x0000FF); // red
		pDC->TextOut(x, y, str);
		if (theApp.pre_connection) {
			pDC->TextOut(x, y + CharHeight, "Are you sure Windows Firewall "
						 "has this program enabled as an exception?");
		}
		return;
	}

	pDC->TextOut(x, y, str);
	y += CharHeight * 2;

	locs.RemoveAll();

	MibObject * obj;
	CString s;
	CString s2;
	MgtSocket * m;
	MgtSocket * h;
	AnalyserDoc * a;
	j = 0;
	PortLocation loc;
	while (++j <= n) { // for each unit
		m = theApp.units.GetAt(j);
		if (m == NULL) continue;
		if (m->state < 0) {
				// illegal value, maybe 0xFEEEFEEE
			if (theApp.privilege >= PRIV_SUPERVISOR) {
				pDC->SetTextColor(0x0000FF); // red
				pDC->SetBkColor(0xFFFFFF); // white
				pDC->TextOut(x, y, "Unit's description corrupted");
				y += (CharHeight * 3) / 2;
			}
			continue;
		}

			// here for each management socket m
		loc.p.unit = m;

		if (theApp.privilege > PRIV_LISTENER && (m->password_state == PW_NOT_SET || 
						m->password_state == PW_CANCELLED || theApp.privilege == 
//						PRIV_MAINTENANCE || sel_port.p.unit->state != MGT_ST_ACTIVE)) {
// +++ the above crashes if <sel_port.p.unit> is NULL; I'm not quite sure what was intended
						PRIV_MAINTENANCE || m->state != MGT_ST_ACTIVE)) {
				// include the unit's name etc so can click on it
			loc.p.port = -1;
			loc.y = y;
			locs.Add(loc);

			str = m->DisplayName();
			if (str.IsEmpty()) {
				pDC->SetTextColor(0x0000FF); // red
				pDC->SetBkColor(0xFFFFFF); // white
				pDC->TextOut(x, y, "Unidentified unit");
				y += (CharHeight * 3) / 2;
				continue;
			}

			if (inputs) {
				s = m->GetStringObject("1.0.62379.1.1.1.6.0"); // product name
				unless (str.Left(s.GetLength()) == s) str += " - " + s;
				s.Format(" - %d-%d-%d-%d", m->product_code[0], 
					m->product_code[1], m->product_code[2], m->product_code[3]);
				str += s;
			}
			else {
				obj = m->GetObject("1.0.62379.1.1.4.7");
				bool in_paren = (obj != NULL);
				if (in_paren) str.AppendFormat(" (sync %d", obj->IntegerValue());

				obj = m->GetObject("1.0.62379.1.1.4.6");
				unless (obj == NULL) {
					if (in_paren) str += ", ";
					else { str += " ("; in_paren = true; }
					str.AppendFormat("timing %02X %d %d", obj->at(0), 
							obj->at(8), (obj->at(9) << 8 | obj->at(10)) ^ 0xFFFF);
				}

				obj = m->GetObject("1.0.62379.1.1.4.8");
				unless (obj == NULL) {
					if (in_paren) str += ", ";
					else { str += " ("; in_paren = true; }
					str.AppendFormat("framing %02X %d ", obj->at(0), obj->at(8));
					i = (obj->at(9) << 8 | obj->at(10)) ^ 0xFFFF; // "uncertainty" value
					if (i >= 0x2000) {
						str.AppendFormat("%d+", i >> 13);
						i &= 0x1FFF;
					}
					str.AppendFormat("%d", i);
				}

				if (in_paren) str += ')';
			}

			if (theApp.link_socket == NULL || theApp.link_socket->state > LINK_ST_MAX_OK) 
													pDC->SetTextColor(0x0000FF); // red
			else if (theApp.link_socket->state != LINK_ST_ACTIVE) 
													pDC->SetTextColor(0xFF00FF); // magenta
			else if (m->state > MGT_ST_MAX_OK) pDC->SetTextColor(0x0000FF); // red
			else if (m->state != MGT_ST_ACTIVE) pDC->SetTextColor(0xFF00FF); // magenta
			else if (m->upd_state == UPD_ST_NO_ACTION) pDC->SetTextColor(0x40BF00); // green
			else pDC->SetTextColor(0xFF0000); // blue

			pDC->SetBkColor(0xFFFFFF); // white
			pDC->TextOut(x, y, str);
			y += (CharHeight * 3) / 2;

			if (theApp.privilege == PRIV_MAINTENANCE && m->unit_id) {
					// list any SCP servers accessible via this unit in sources window, 
					//		transparent tunnels in destinations window
				NetPortList::iterator q = m->net_port_state.begin();
				unless (q == m->net_port_state.end()) do {
// +++ NetPortState codings need review; currently SCP is AwaitSync which is shown 
//		as link down
//					unless (q->second >= NET_PORT_STATE_LINK_UP) continue;
					loc.p.port = q->first;
					s2.Format("%d", loc.p.port);
					if (inputs) {
							// get nPortScpDevice from MIB
						s = m->GetStringHex("1.0.62379.5.1.1.1.1.1.9." + s2, 1);
						if (s.IsEmpty()) continue;
						if (s == "00 90 A8 00") {
							i = 1;
							str = s2 + ": debug server";
						}
						else {
							i = 0;
							str = s2 + ": SCP server type " + s;
						}
						h = m->LinkPartner(loc.p.port);
						if (h) {
							s = h->DisplayName();
							unless (s.IsEmpty()) str += " in " + s;
						}
						pDC->SetTextColor(0); // black
						pDC->SetBkColor(0xFFFFFF); // white
						if (i) {
								// add the VM state
							a = theApp.FindScpServer(m, loc.p.port, 0);
							if (a && a->state >= MGT_ST_CONN_MADE && a->server_state != 0) {
									// <server_state> is valid so take the VM state from that
								pDC->SetTextColor(0xFF0000); // blue
								i = ((a->server_state >> 24) & 3) | 4;
							}
							else i = m->GetIntegerObject("1.0.62379.5.1.1.1.1.1.10." + s2);
							switch (i) {
					case 4:		str += " (CPU initialising)";
								pDC->SetBkColor(0xA0FF80); // pale green
								break;
					case 5:		str += " (CPU waiting)";
								pDC->SetBkColor(0xA0FF80); // pale green
								break;
					case 6:		str += " (CPU running)";
								break;
					case 7:		str += " (CPU stopped)";
								pDC->SetBkColor(0x8080FF); // pale red
								break;
					default:	pDC->SetTextColor(0x0000FF); // red
							}
						}

						loc.y = y;
						locs.Add(loc);
					}
					else {
							// get nPortTransparentPartner and nPortState from MIB
						s = m->GetStringHex("1.0.62379.5.1.1.1.1.1.11." + s2, 1);
						str = "Network port " + s2;
						i = m->GetIntegerObject("1.0.62379.5.1.1.1.1.1.3."+ s2);
						switch (i) {
					default: continue;
					case 3:		// link down
							if (s.IsEmpty()) continue; // null string or not reported
							pDC->SetTextColor(0x00A0A0); // yellow
							break;
					case 1: pDC->SetTextColor(0x0000FF); // red
							str += " reserved";
							unless (s.IsEmpty()) str += "; transparent partner " + s;
							break;
					case 8: pDC->SetTextColor(0x40BF00); break; // green
						}
						unless (i == 1) {
							str += " transparent";
							unless (s.IsEmpty()) str += " partner " + s;
						}
						pDC->SetBkColor(0xFFFFFF); // white
					}
					pDC->TextOut(x + Char5Width, y, str);
					y += CharHeight;
				} until (++q == m->net_port_state.end());
			}
		}

		POSITION port_ptr = (inputs ? m->input_port_list : 
										m->output_port_list).GetHeadPosition();
		while (port_ptr != NULL) {
			int colour = 4; // d2 set if not connected, d1-0 ms 2 bits of importance
			if (inputs) {
				loc.p.port = m->input_port_list.GetNext(port_ptr);
				s2.Format("%d", loc.p.port);
				str = m->GetStringObject("1.0.62379.2.1.1.1.1.5." + s2); // aPortName
				unless (str.IsEmpty()) {
						// have an audio port name
		//			if (theApp.name_translations.Lookup(str, s)) str = s;
						// see if it's receiving and if so add the sampling rate
					s = m->GetOidObject("1.0.62379.2.1.1.1.1.3." + s2); // aPortFormat
					unless (s.Left(18) == "1.0.62379.2.2.1.3.") goto input_done;
					str += " (" + s.Mid(s.ReverseFind('.') + 1) + ')';
					goto input_done;
				}

				str = m->GetStringObject("1.0.62379.3.1.1.1.1.5." + s2); // vPortName
				if (str.IsEmpty()) {
					str.Format("[input port %d]", loc.p.port);
					goto input_done;
				}

					// here if have a video port name
		//			if (theApp.name_translations.Lookup(str, s)) str = s;
					// see if it's receiving and if so add the sampling rate
				s = m->GetOidObject("1.0.62379.3.1.1.1.1.3." + s2); // vPortFormat
				i = 0;
				sscanf_s(s, "1.0.62379.3.2.1.%u", &i);
				if (i == 1) goto input_done; // no signal
				int k0, k1, k2, k3;
				if (i == 2) {
					str += " (invalid format)";
					goto input_done;
				}
				if (i == 0) {
					str += " (unspecified format)";
					goto input_done;
				}
				if (i != 3 || sscanf_s(s.Mid(17), ".%u.%u.%u.%u", 
														&k0, &k1, &k2, &k3) < 4) {
					str += " (unrecognised format)";
					goto input_done;
				}
					// here if videoSource (see 4.1.3.5 of 62379-3)
				str += " (";
				switch (k1) {
			case 1: str += "SD "; break;
			case 2: str += "HD "; break;
			case 3: str += "4k "; break;
			case 4: str += "8k "; break;
				}
				str.AppendFormat("%u", k2);
				if (k3 == 1) str.AppendFormat("p%u", k0/1000);
				else if (k3 == 2) str.AppendFormat("i%u", k0/500);
				str += ')';
	
		input_done:	// here when any indication of what input is present on the port 
					//		has been added to <str>
				if (!m->input_flows.Lookup(s2, s)) {
						// no flow for the port
					goto write_port;
				}

				obj = m->GetObject("1.0.62379.5.1.1.3.3.1.9." + s);
				if (obj == NULL) {
						// udState for the flow no longer in the MIB, so assume it 
						//		has been cleared down
					m->input_flows.RemoveKey(s2);
					goto write_port;
				}

				i = obj->IntegerValue();
				if (i == 6 || i == 9) {	// "disconnected" or "finished"
					m->input_flows.RemoveKey(s2);
					goto write_port;
				}

				str += " -> flow " + s;
				if (i == 4) colour = 	// udImportance
					m->GetIntegerObject("1.0.62379.5.1.1.3.3.1.14." + s) >> 6;
				else colour = 7; // magenta if flow not active
			}
			else {
					// output port
				loc.p.port = m->output_port_list.GetNext(port_ptr);
				s2.Format("%d", loc.p.port);
				str = m->GetStringObject("1.0.62379.2.1.1.1.1.5." + s2); // aPortName
				if (str.IsEmpty()) {
					str = m->GetStringObject("1.0.62379.3.1.1.1.1.5." + s2); // vPortName
					if (str.IsEmpty()) str.Format("[input port %d]", loc.p.port);
				}

		//		if (theApp.name_translations.Lookup(str, s)) str = s;

					// set colour from aPortImportance
					// +++ also need to support vPortImportance; ought to begin by 
					//		finding what kind of port it is (audio, video, ...) and 
					//		then look for the appropriate Importance etc objects, 
					//		or even find a way to make Importance etc independent 
					//		of the type of port
				colour = m->GetIntegerObject("1.0.62379.2.1.1.1.1.6." + s2) >> 6;

				unless (m->output_flows.Lookup(s2, s)) {
						// no flow for the port
					colour |= 4;
					goto write_port;
				}

				obj = m->GetObject("1.0.62379.5.1.1.2.1.1.7." + s); // usState
				if (obj == NULL) {
						// usState for the flow no longer in the MIB, so assume it 
						//		has been cleared down
					m->output_flows.RemoveKey(s2);
					colour |= 4;
					goto write_port;
				}

				i = obj->IntegerValue();
				if (i == 6 || i == 9) {	// "disconnected" or "finished"
					m->output_flows.RemoveKey(s2);
					colour |= 4;
					goto write_port;
				}

				unless (i == 4) colour = 7; // magenta if not active

				i = m->GetIntegerObject("1.0.62379.2.1.1.4.1.3." + s2); // aLockedSamplesInserted
				if (i > 0) str.AppendFormat(" %d ins", i);

				i = m->GetIntegerObject("1.0.62379.2.1.1.4.1.4." + s2); // aLockedSamplesDropped
				if (i > 0) str.AppendFormat(" %d drop", i);

				str += " <- ";
				int k = m->GetIntegerObject("1.0.62379.2.1.1.4.1.2." + s2); // aLockedTime
				MgtSocket * sender;
				s = s.Left(s.ReverseFind('.'));	// flow id
				if (theApp.flow_senders.Lookup(s, (void *&)sender) && sender) {
						// we've found the unit transmitting the flow
					i = sender->GetIntegerObject("1.0.62379.5.1.1.3.3.1.2." + s);
					if (i) {
							// sending from port <i>: find aPortName
						s2.Format("%d", i);
						s2 = sender->GetStringObject("1.0.62379.2.1.1.1.1.5." + s2);
						if (s2.IsEmpty()) s2 = 
								sender->GetStringObject("1.0.62379.3.1.1.1.1.5." + s2);
						if (!s2.IsEmpty()) {
						/*	if (theApp.name_translations.Lookup(s2, s)) str += s;
							else*/ str += s2;
							goto add_time;
						}
							// could default to the unit name plus default port name, 
							//		but the Aubergine creates its own default values 
							//		for aPortName and vPortname so in practice it'll 
							//		always be in the MIB
					}
				}

					// here if couldn't find the sending unit and get the name of its port
				str += "flow " + s;

add_time:			// add the "locked time" if nonzero
				if (k <= 0) goto write_port;
				str += ' ';;
				if (k >= 3600) str.AppendFormat("%dh", k/3600);
				if (k >= 60) str.AppendFormat("%dm", (k/60)%60);
				str.AppendFormat("%ds", k%60);
			}
write_port:
				// here when <colour> is set and <str> is the output line
			switch (colour) {
			case 0: pDC->SetTextColor(0x40BF00); // green
				break;
			case 1: pDC->SetTextColor(0xFF0000); // blue
				break;
			case 2: pDC->SetTextColor(0x3366CC); // orange/brown
				break;
			case 3: pDC->SetTextColor(0x0000FF); // red
				break;
			case 4:
			case 5: pDC->SetTextColor(0); // black
				break;
			default: pDC->SetTextColor(0xFF00FF); // magenta
			}
			if (m == pDoc->sel_port.unit && 
						loc.p.port == pDoc->sel_port.port) 
										pDC->SetBkColor(0x00FFFF); // yellow
				else pDC->SetBkColor(0xFFFFFF); // white
			loc.y = y;
			locs.Add(loc);
			pDC->TextOut(x + Char5Width, y, str);
			y += CharHeight;
		}

		y += CharHeight / 2;
	}

	if (inputs) return;
		// here to add the latest error messages at the end of the outputs list
	n = theApp.err_msgs.GetSize();
	if (n <= 0) return;
	pDC->SetTextColor(0x0000FF); // red
	i = 0;
	do {
		pDC->TextOut(x, y, theApp.err_msgs[i++]);
		y += CharHeight;
	} while (i < n);
}


// CCrosspointView diagnostics

#ifdef _DEBUG
void CCrosspointView::AssertValid() const
{
	CScrollView::AssertValid();
}

void CCrosspointView::Dump(CDumpContext& dc) const
{
	CScrollView::Dump(dc);
}

CCrosspointDoc* CCrosspointView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CCrosspointDoc)));
	return (CCrosspointDoc*)m_pDocument;
}
#endif //_DEBUG


// CCrosspointView message handlers

void CCrosspointView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CScrollView::OnLButtonDown(nFlags, point);

		// find <y> such that of we are pointing to the middle of the line with 
		//		location <loc> we will have <y == loc.y>
	CPoint hit = point + GetDeviceScrollPosition();
	int y = hit.y - (CharHeight / 2);

	CCrosspointDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	INT_PTR n = locs.GetSize();
	if (n <= 0) return;	// if list of ports is invalid

	PortLocation loc;
	int i = 0;
	while (i < n) {
		loc = locs.GetAt(i);
		if (loc.y >= y) break;
		i += 1;
	}

		// now <i> is the index to <loc> which is the first line below <y>, 
		//		unless <y> is below the last line in which case <loc> is the 
		//		last line and <i == n>
		// if pointing between two lines and nearer to the line above, select 
		//		the line above, else select <loc>
	if (i <= 0 || i >= n || (((sel_port = locs.GetAt(i-1)).y + 
											loc.y) / 2) < y) sel_port = loc;

	if (sel_port.p.port < 0) {
			// left clicking on a heading: select in the console window if 
			//		maintenance
		unless (theApp.privilege == PRIV_MAINTENANCE) return;
		theApp.controller_doc->SetUnit(sel_port.p.unit);
		return;
	}

	class AnalyserDoc * a;
	if (sel_port.p.unit->net_port_state.IsInList(sel_port.p.port)) {
			// left clicking on a "device" line (because it's a network port)
		ASSERT(theApp.privilege == PRIV_MAINTENANCE);
		a = theApp.FindScpServer(sel_port.p.unit, sel_port.p.port);
		if (a == NULL) return;
		theApp.controller_doc->display_select = a->call_ref & 255;
		theApp.controller_doc->BringToFront();
		theApp.controller_doc->UpdateDisplay();
		return;
	}

	pDoc->sel_port = sel_port.p;

		// get the view redrawn
	pDoc->UpdateAllViews(NULL);

		// if an input, nothing else to do
	if (pDoc->inputs) return;

		// if an output, request the connection
		// check there is a selected input; else leave the output selected 
		//		(as a hint the connection isn't happening)
	if (theApp.input_list->sel_port.unit == NULL) return;

		// check privilege against CallId requirement & against port importance
	if (theApp.privilege < PRIV_OPERATOR) return;
	CString s2;
	s2.Format("%d", sel_port.p.port);
	MibObject * obj = sel_port.p.unit->GetObject("1.0.62379.2.1.1.1.1.6." + s2);
	if (obj == NULL) obj = sel_port.p.unit->GetObject("1.0.62379.3.1.1.1.1.6." + s2);
	if (obj == NULL || theApp.privilege <= (obj->IntegerValue() >> 6)) return;

		// here if OK to begin by asking for a CallId; first remove any error 
		//		messages from previous requests
	theApp.err_msgs.RemoveAll();
	ByteString msg;
	msg.resize(14);

	msg[0]  = 0;		// "Get" request
	msg[2]  = 6;		// OID tag
	msg[3]  = 10;		// length of OID
	msg[4]  = 0x28;		// OID = 1.0.62379.5.1.1.3.2.0
	msg[5]  = 0x83;
	msg[6]  = 0xE7;
	msg[7]  = 0x2B;
	msg[8]  = 5;
	msg[9]  = 1;
	msg[10] = 1;
	msg[11] = 3;
	msg[12] = 2;
	msg[13] = 0;

	sel_port.p.unit->TxNewMessage(msg);
	if (sel_port.p.unit == NULL) return;	// transmission must have failed

		// add an entry to the list of pending connections
	ConnReqInfo ci;
	ci.dest_port = sel_port.p.port;
	ci.m.push_back(msg);
	ci.count = 0;

	msg.clear();
	msg.push_back(0);
	ByteString& uta = theApp.input_list->sel_port.unit->unit_TAddress;
	int k = (int)(uta.size());
	msg.push_back((uint8_t)k);
	i = 0;
	while (i < k) msg.push_back(uta[i++]);
	msg.push_back(9);
	i = theApp.input_list->sel_port.port;
	k = 24;						// always send 32 bits
	do { msg.push_back(i >> k); k -= 8; } while (k >= 0);
	ci.srce_addr = msg;

	sel_port.p.unit->conn_pend.push_back(ci);
}


void CCrosspointView::OnRButtonDown(UINT nFlags, CPoint point)
{
	CScrollView::OnRButtonDown(nFlags, point);

		// find <y> such that of we are pointing to the middle of the line with 
		//		location <loc> we will have <y == loc.y>
	CPoint hit = point + GetDeviceScrollPosition();
	int y = hit.y - (CharHeight / 2);

	CCrosspointDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	INT_PTR n = locs.GetSize();
	if (n <= 0) return;	// if list of ports is invalid

	PortLocation loc;
	int i = 0;
	while (i < n) {
		loc = locs.GetAt(i);
		if (loc.y >= y) break;
		i += 1;
	}

	ByteString b;
		// now <i> is the index to <loc> which is the first line below <y>, 
		//		unless <y> is below the last line in which case <loc> is the 
		//		last line and <i == n>
		// if pointing between two lines and nearer to the line above, select 
		//		the line above, else select <loc>
	PortLocation sel_port;
	if (i <= 0 || i >= n || (((sel_port = locs.GetAt(i-1)).y + 
											loc.y) / 2) < y) sel_port = loc;
	if (sel_port.p.unit == NULL) return;
	if (sel_port.p.port < 0) {
			// right click on unit name:
			// remove the unit if not connected 
		unless (sel_port.p.unit->state == MGT_ST_ACTIVE) {
			sel_port.p.unit->state = MGT_ST_CLOSED; // so won't send ClearDown
			delete sel_port.p.unit;
			if (theApp.privilege == PRIV_MAINTENANCE) 
										theApp.controller_doc->UpdateDisplay();
			return;
		}

			// else display the "change product code" dialogue (which includes 
			//		setting the password) if maintenance level, "supply 
			//		password" if any other level 
		unless (theApp.privilege == PRIV_MAINTENANCE) {
			if (sel_port.p.unit->password_state == PW_NOT_USED) return;
			if (sel_port.p.unit->password_state == PW_REQUESTED) return;
			sel_port.p.unit->GetPassword();
			return;
		}

			// here if maintenance privilege
		CPcodeChange q; // created with <include_password> ticked, others not
			// set existing product code and password
		if (sel_port.p.unit->upd_state > 2 && 
									sel_port.p.unit->upd_state != UPD_ST_FROM_ISE)
				q.pcode.Format("%d-%d-%d-%d", sel_port.p.unit->product_code[0], 
				sel_port.p.unit->product_code[1], sel_port.p.unit->product_code[2], 
												sel_port.p.unit->product_code[3]);
		if (sel_port.p.unit == theApp.link_partner) q.include_password = BST_UNCHECKED;

			// do dialogue; repeat if new product code not well-formed
		unsigned int pc[4]; // for the product code
		bool ch;	// whether it has changed
		while (true) {
			i = q.DoModal();
			unless (i == IDOK) {
				theApp.controller_doc->UpdateDisplay();
				return;
			}

				// product code
			ch = false; // whether the product code has changed
			if (q.pcode.IsEmpty()) break;
			if (sscanf_s(q.pcode, "%u-%u-%u-%u", &pc[0], &pc[1], &pc[2], &pc[3]) == 4 && 
						pc[0] <= 255 && pc[1] <= 255 && pc[2] <= 255 && pc[3] <= 255) {
					// the new code is valid and <pc> holds the four individual numbers
					// retrieve the new product code
				i = 4;
				do {
					i -= 1;
					if (sel_port.p.unit->product_code[i] == pc[i]) continue;
					sel_port.p.unit->product_code[i] = pc[i];
					ch = true;
				} while (i > 0);
				break;
			}

				// here if not valid; set the text red and add a note, and repeat
				// +++ line below throws an exception, not sure why
	//		COleControl * c = dynamic_cast<COleControl *>(q.GetDlgItem(IDC_EDIT1));
	//		if (c) c->SetForeColor(0x000000FF); // red
			if (q.pcode.GetLength() < 20) q.pcode += " (must be n-n-n-n, all < 256)";
		}

			// here with <q> holding the result having already updated the product code
		if (ch) {
				// product code has changed
			sel_port.p.unit->sw_ver[0].Invalidate();
			sel_port.p.unit->sw_ver[1].Invalidate();
			sel_port.p.unit->upd_state = UPD_ST_HAVE_SW_VER;
		}
		if (ch || q.reset_update_state == BST_CHECKED) {
			sel_port.p.unit->user_update_flags = -1;
			theApp.update_flags = -1;
		}

			// collect the password if required
		if (q.change_password == BST_CHECKED) {
			StringToPassword(q.pw, sel_port.p.unit->password_string);
			sel_port.p.unit->password_state = PW_VALID;
		}
				
		if (q.include_password == BST_UNCHECKED) 
								sel_port.p.unit->password_state = PW_CANCELLED;

		if (q.configure_tunnel == BST_CHECKED) {
			if (q.remote_unit == 0) {
				b.resize(18);
				b[17] = 0;
			}
			else {
				b.resize(31);
				b[17] = 13;
				b[18] = 0;
				b[19] = 9;
				b[20] = 5;
				b[21] = 0;
				b[22] = 0x90;
				b[23] = 0xA8;
				b[24] = 0x99;
				b[25] = 0;
				b[26] = 0;
				b[27] = 0;
				b[28] = q.remote_unit;
				b[29] = 9;
				b[30] = q.remote_port;
			}
			b[0]  = 0x40;	// "non-volatile Set" request
			b[2]  = 6;		// OID tag
			b[3]  = 12;
			b[4]  = 0x28;	// OID 1.0.62379.5.1.1.1.1.1.11.[local_port]
			b[5]  = 0x83;
			b[6]  = 0xE7;
			b[7]  = 0x2B;
			b[8]  = 5;
			b[9]  = 1;
			b[10] = 1;
			b[11] = 1;
			b[12] = 1;
			b[13] = 1;
			b[14] = 11;
			b[15] = q.local_port;
			b[16] = ASN1_TAG_OCTET_STRING;
			sel_port.p.unit->TxNewMessage(b);
		}

		theApp.controller_doc->UpdateDisplay();
		return;
	}

		// else <sel_port> shows the port to which the mouse is pointing
		// request disconnection by setting usState or udState (as appropriate) 
		//		to terminating (3)
	CString s;
	s.Format("%d", sel_port.p.port);
	if (pDoc->inputs) {
		if (!sel_port.p.unit->input_flows.Lookup(s, s)) return;
	}
	else if (!sel_port.p.unit->output_flows.Lookup(s, s)) return;

		// in each case, <s> is the "index" part of the OID
		// first remove any error messages from previous requests
	theApp.err_msgs.RemoveAll();

	b.resize(15);	// fill in the first 15 bytes using SetAt

	b[0]  = 0x30;	// "Set" request
	b[2]  = 6;		// OID tag
	b[4]  = 0x28;	// OID begins with 1.0.62379.5.1.1
	b[5]  = 0x83;
	b[6]  = 0xE7;
	b[7]  = 0x2B;
	b[8]  = 5;
	b[9]  = 1;
	b[10] = 1;

	if (pDoc->inputs) {
			// OID continues with 3.3.1.9
		b[11] = 3;
		b[12] = 3;
		b[13] = 1;
		b[14] = 9;
	}
	else {
			// OID continues with 2.1.1.7
		b[11] = 2;
		b[12] = 1;
		b[13] = 1;
		b[14] = 7;
	}

	int j;
	while (true) {
		sscanf_s(s, "%d%n", &j, &i);
		if (i == 0 || j > 0x3FFF) return;
		if (j > 127) { b.push_back(0x80 | (j >> 7)); b.push_back(j & 0x7F); }
		else b.push_back(j);
		s = s.Mid(i);
		if (s.IsEmpty()) break;
		if (s[0] != '.') return;
		s = s.Mid(1);
	}

	b[3] = (uint8_t)(b.size() - 4);		// length of OID
	b.push_back(ASN1_TAG_INTEGER);
	b.push_back(1);
	b.push_back(3);
	sel_port.p.unit->TxNewMessage(b);
}

void CCrosspointView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	CScrollView::OnLButtonDblClk(nFlags, point);

		// find <y> such that of we are pointing to the middle of the line with 
		//		location <loc> we will have <y == loc.y>
	CPoint hit = point + GetDeviceScrollPosition();
	int y = hit.y - (CharHeight / 2);

	CCrosspointDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	INT_PTR n = locs.GetSize();
	if (n <= 0) return;	// if list of ports is invalid

	PortLocation loc;
	int i = 0;
	do {
		loc = locs.GetAt(i);
		if (loc.y >= y) break;
		i += 1;
	} while (i < n);

		// now <i> is the index to <loc> which is the first line below <y>, 
		//		unless <y> is below the last line in which case <loc> is the 
		//		last line and <i == n>
		// if pointing between two lines and nearer to the line above, select 
		//		the line above, else select <loc>
	PortLocation sel_port;
	if (i <= 0 || i >= n || (((sel_port = locs.GetAt(i-1)).y + 
											loc.y) / 2) < y) sel_port = loc;
	if (sel_port.p.unit == NULL) return;

	ByteString msg;
	CString new_name;
	bool set_name = false;
	CString location;
	if (sel_port.p.port < 0) {
			// double click on unit name
		msg.resize(46);
		msg[0]  = 0x40;	// "non-volatile Set" request
						// <TxNewMessage> supplies the serial number
		msg[2]  = 6;	// OID tag
		msg[3]  = 8;	// length of OID
		msg[4]  = 0x28;	// OID = 1.0.62379.1.1.1.n
		msg[5]  = 0x83;
		msg[6]  = 0xE7;
		msg[7]  = 0x2B;
		msg[8]  = 1;
		msg[9]  = 1;
		msg[10] = 1;
						// n goes at offset 11
		msg[12] = 4;	// OCTET STRING tag

		bool set_location = false;
		if (theApp.privilege == PRIV_MAINTENANCE) {
			CSetUnitPasswords qp;
			qp.caption = sel_port.p.unit->DisplayName();
			qp.name = qp.caption;
			location = sel_port.p.unit->GetStringObject("1.0.62379.1.1.1.2.0");
			qp.location = location;
			i = qp.DoModal();
			unless (i == IDOK) {
				theApp.controller_doc->UpdateDisplay();
				return;
			}

				// set new password(s) if required
				// note that each Set request needs a separate message but 
				//		there is no "window" size defined so several messages 
				//		can be sent without waiting for a reply
				// +++ however, we don't check the reply so to see whether the 
				//		change has been successful the user needs to look for 
				//		the reply message in the console window
			msg[11] = 17;
			msg[13] = 32;	// length

			uint64_t pw[4];
			if (qp.reset_opr == BST_CHECKED) {
					// set operator password
				StringToPassword(qp.operator_password, pw);
				BytesFrom64Bit(pw, msg.data() + 14);
				sel_port.p.unit->TxNewMessage(msg);
				if (sel_port.p.unit == NULL) return; // tx must have failed
			}
			if (qp.reset_svr == BST_CHECKED) {
					// set supervisor password
				msg[11] = 18;
				StringToPassword(qp.supervisor_password, pw);
				BytesFrom64Bit(pw, msg.data() + 14);
				sel_port.p.unit->TxNewMessage(msg);
				if (sel_port.p.unit == NULL) return; // tx must have failed
			}
			if (qp.reset_maint == BST_CHECKED) {
					// set maintenance password
				msg[11] = 19;
				StringToPassword(qp.maintenance_password, pw);
				BytesFrom64Bit(pw, msg.data() + 14);
				sel_port.p.unit->TxNewMessage(msg);
				if (sel_port.p.unit == NULL) return; // tx must have failed
			}

				// now collect the name and location in the same way as for the 
				//		other levels
			if (qp.name != qp.caption) {
				set_name = true;
				new_name = qp.name;
			}
			if (location != qp.location) {
				set_location = true;
				location = qp.location;
			}
		}
		else {
				// levels below maintenance
			CSetUnitName qn;
			qn.caption = sel_port.p.unit->DisplayName();
			qn.name = qn.caption;
			location = sel_port.p.unit->GetStringObject("1.0.62379.1.1.1.2.0");
			qn.location = location;
			i = qn.DoModal();
			unless (i == IDOK) {
				theApp.controller_doc->UpdateDisplay();
				return;
			}
			if (qn.name != qn.caption) {
				set_name = true;
				new_name = qn.name;
			}
			if (location != qn.location) {
				set_location = true;
				location = qn.location;
			}
		}

		if (set_name) {
				// 1.0.62379.1.1.1.1 unitName
			msg[11] = 1;
			i = new_name.GetLength();
			if (i < 128) {
				msg[13] = i;
				y = 14;
			}
			else if (i < 256) {
				msg[13] = 0x81;
				msg[14] = i;
				y = 15;
			}
			else if (i < 1200) {
				msg[13] = 0x82;
				msg[14] = i >> 8;
				msg[15] = i;
				y = 16;
			}
			else {
				theApp.controller_doc->UpdateDisplay();
				return;
			}
			msg.resize(y + i);
			memcpy(msg.data() + y, new_name.GetBuffer(), i);
			sel_port.p.unit->TxNewMessage(msg);
			if (sel_port.p.unit == NULL) return; // tx must have failed
		}

		if (set_location) {
				// 1.0.62379.1.1.1.2 unitLocation
			msg[11] = 2;
			i = location.GetLength();
			if (i < 128) {
				msg[13] = i;
				y = 14;
			}
			else if (i < 256) {
				msg[13] = 0x81;
				msg[14] = i;
				y = 15;
			}
			else if (i < 1200) {
				msg[13] = 0x82;
				msg[14] = i >> 8;
				msg[15] = i;
				y = 16;
			}
			else {
				theApp.controller_doc->UpdateDisplay();
				return;
			}
			msg.resize(y + i);
			memcpy(msg.data() + y, location.GetBuffer(), i);
			sel_port.p.unit->TxNewMessage(msg);
		}
		theApp.controller_doc->UpdateDisplay();
		return;
	}

		// here for a double click on a port
		// find its current name etc
	location.Format("%d", loc.p.port);	// port number on the unit
		// we assume the objects reported in the status broadcast always 
		//		include a name
	new_name = sel_port.p.unit->GetStringObject("1.0.62379.2.1.1.1.1.5." + 
														location); // aPortName
	bool video = new_name.IsEmpty();
	if (video) {
		new_name = sel_port.p.unit->GetStringObject("1.0.62379.3.1.1.1.1.5." + 
														location); // vPortName
		if (new_name.IsEmpty())  return;	// must be a SCP debug port
	}
	msg.resize(18);
	msg[0]  = 0x40;	// "non-volatile Set" request
					// <TxNewMessage> supplies the serial number
	msg[2]  = 6;	// OID tag
	msg[3]  = 11;	// length of OID
	msg[4]  = 0x28;	// OID = 1.0.62379.2.1.1.1.1.6.n for audio importance
	msg[5]  = 0x83;
	msg[6]  = 0xE7;
	msg[7]  = 0x2B;
	msg[8]  = video ? 3 : 2;
	msg[9]  = 1;
	msg[10] = 1;
	msg[11] = 1;
	msg[12] = 1;
	msg[13] = 6;
	msg[14] = loc.p.port;	// +++ NB assumes port number < 128
	msg[15] = 2;	// INTEGER tag
	msg[16] = 1;	// length

	if (pDoc->inputs) {
			// just set the name
		CSetPortName qn;
		qn.caption = sel_port.p.unit->DisplayName() + " port " + location;
		qn.name = new_name;
		i = qn.DoModal();
		if (i != IDOK || qn.name == new_name) {
			theApp.controller_doc->UpdateDisplay();
			return;
		}
		new_name = qn.name;
		set_name = true;
	}
	else {
			// also set importance
		CSetPortNameEtc qn;
		qn.caption = sel_port.p.unit->DisplayName() + " port " + location;
		qn.name = new_name;
		CString s = video ? "1.0.62379.3" : "1.0.62379.2";
		int importance = sel_port.p.unit->GetIntegerObject(s + ".1.1.1.1.6." + location);
		qn.importance = importance;
		i = qn.DoModal();
		if (i != IDOK) {
			theApp.controller_doc->UpdateDisplay();
			return;
		}
		if (qn.importance != importance) {
				// user has changed the importance
			if ((qn.importance >> 6) >= theApp.privilege) {
				s.Format("Setting importance %d needs at least privilege level %d",
										qn.importance, (qn.importance >> 6) + 1);
				AfxMessageBox(s);
				theApp.controller_doc->UpdateDisplay();
				return;
			}
			if (qn.importance < 128) msg[17] = qn.importance;
			else {
				msg.resize(19);
				msg[16] = 2;
				msg[17] = 0;
				msg[18] = qn.importance;
			}
			sel_port.p.unit->TxNewMessage(msg);
			if (sel_port.p.unit == NULL) return; // tx must have failed
		}
		if (qn.name != new_name) {
			new_name = qn.name;
			set_name = true;
		}
	}

	if (set_name) {
			// 1.0.62379.2.1.1.1.1.5 aPortName or 1.0.62379.3.1.1.1.1.5 vPortName
		msg[13] = 5;
		msg[15] = 4;	// OCTET STRING tag
		i = new_name.GetLength();
		if (i < 128) {
			y = 17;
			msg.resize(17 + i);
			msg[16] = i;
		}
		else if (i < 256) {
			y = 18;
			msg.resize(18 + i);
			msg[16] = 0x81;
			msg[17] = i;
		}
		else if (i < 1200) {
			y = 19;
			msg.resize(19 + i);
			msg[16] = 0x82;
			msg[17] = i >> 8;
			msg[18] = i;
		}
		else {
			theApp.controller_doc->UpdateDisplay();
			return;
		}
		memcpy(msg.data() + y, new_name.GetBuffer(), i);
		sel_port.p.unit->TxNewMessage(msg);
		if (sel_port.p.unit == NULL) return; // tx must have failed
	}
}
