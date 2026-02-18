// ControllerDoc.cpp : implementation of the CControllerDoc class
// Copyright (c) 2014-2015 Nine Tiles

#include "stdafx.h"
#include "Controller.h"
#include "CrosspointDoc.h"
#include "AnalyserDoc.h"
#include "PcodeChange.h"
#include "SHA3.h"
//#include ".\controllerdoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// NetPortList

NetPortList::NetPortList()
{
}

NetPortList::~NetPortList()
{
}

// set state to <st>
// if <st> is NET_PORT_STATE_LINK_UP and current state is neither _LINK_UP 
//		nor _WAITING, sets state to NET_PORT_STATE_WAITING and returns <true>
// if <st> is NET_PORT_STATE_LINK_UP and current state is _WAITING, returns 
//		<false> without changing state
// else sets state to <st> and returns <false>
bool NetPortList::NewState(int port, int st)
{
	if (st == NET_PORT_STATE_LINK_UP) {
		NetPortList::iterator p = find(port);
		if (p == end()) {
			insert(std::pair<int, int>(port, NET_PORT_STATE_WAITING));
			return true;
		}
		if (p->second == NET_PORT_STATE_LINK_UP || 
						p->second == NET_PORT_STATE_WAITING) return false;
		p->second = NET_PORT_STATE_WAITING;
		return true;
	}

//	(*this)[port] = st;
	operator[](port) = st;
	return false;
}


// return the state for <port> if it's in the map, NET_PORT_STATE_DISABLED else
// unlike operator[], does not create an entry if not found
inline int  NetPortList::State(int port) {
	NetPortList::iterator p = find(port);
	if (p == end()) return NET_PORT_STATE_DISABLED;
	return p->second;
}


// MgtSocket

FlexilinkSocket::FlexilinkSocket()
{
	call_ref = -1; // until written by CControllerApp::NewUnit() etc
	tx_flow = -1;
	state = MGT_ST_BEGIN;
	next_serial = 1;
	keep_alive_count = 0;
	mgt_msg.count = 0;
}

MgtSocket::MgtSocket()
{
	password_state = PW_NOT_USED; // until FindRoute response received
	upd_state_view = -1;
	upd_state = UPD_ST_NO_INFO;
	upd_area = flash.end();
	last_serial = -1;
	upd_reply = 0xA0;
	unit_id = 0;
	scp_server = NULL;
	last_console_serial = -1;
	next_seq = 0;
	user_update_flags = -1;
	mib_map.InitHashTable(499);	// initialise <mib>
	keep_alive_count = 0;
	traced = true;
}

FlexilinkSocket::~FlexilinkSocket()
{
	int i;
	unless (state == MGT_ST_CLOSED || call_ref < 0 || 
								theApp.link_socket->state != LINK_ST_ACTIVE) {
			// send a ClearDown message
			// don't ask for ack, nor store the message for display, because 
			//		<this> is about to cease to exist
		uint8_t m[21];	// accumulates the message
		m[0] = 9;		// ClearDown request
		m[1] = 3;
		m[2] = 0;
		m[3] = 0;
		m[4] = 0;
		m[5] = 0x18;
		m[6] = 0;
		m[7] = 13;

		i = 0;
		do {
			m[i + 8] = theApp.link_socket->our_ident[i];	// owner
			i += 1;
		} while (i < 8);

		m[16] = (uint8_t)(call_ref >> 24);
		m[17] = (uint8_t)(call_ref >> 16);
		m[18] = (uint8_t)(call_ref >> 8);
		m[19] = (uint8_t)call_ref;
		m[20] = 2;		// route ref
		theApp.link_socket->TxMessage(m, 21);
	}
}

MgtSocket::~MgtSocket()
{
	if (theApp.link_partner == this) theApp.link_partner = NULL;

	POSITION p = mib.GetHeadPosition();
	MibObject * obj;
	while (p != NULL) {
		obj = mib.GetNext(p);
		delete obj;
	}

	p = input_flows.GetStartPosition();
	CString s1;
	CString s2;
	while (p != NULL) {
		input_flows.GetNextAssoc(p, s1, s2);
		theApp.flow_senders.RemoveKey(s2);
	}

	theApp.unit_addrs.RemoveKey(unit_address);
	unless (call_ref < 0) theApp.units.SetAt(call_ref & 255, NULL);
	theApp.input_list->RemoveFromLocs(this);
	theApp.output_list->RemoveFromLocs(this);
	theApp.mib_changed = true;

	theApp.RemoveFromAnalysers(this);

	CControllerDoc * d = theApp.controller_doc;
	unless (d->UnitIs(this)) return;

		// here if this unit is being displayed; switch to the first in the list
		// <SetUnit> repaints the controller screen
	int n = (int)theApp.units.GetSize();
	int i = 1;
	while (i < n) {
		unless (theApp.units.GetAt(i) == NULL) {
			d->SetUnit(theApp.units.GetAt(i));
			return;
		}
		i++;
	}
	d->SetUnit(NULL);
}


// return the neighbour on port <p>, if any, else NULL
MgtSocket * MgtSocket::LinkPartner(int p) {
	CString oid;
	oid.Format("1.0.62379.5.1.1.1.1.1.6.%d", p); // nPortPAddrType
	unless (GetOidObject(oid) == "1.0.62379.5.2.2") return NULL;
	oid.SetAt(22, '7'); // nPortPartnerAddress
	ByteString p_addr = GetOctetStringObject(oid);
	if (p_addr.empty()) return NULL;

		// now <p_addr> is the link partner's address; check whether we already 
		//		have a management socket for it, and create if not
	MgtSocket * m;
	if (theApp.unit_addrs.Lookup(ByteArrayToHex(p_addr), (void *&)m)) return m;
	return theApp.NewUnit(p_addr);
}


// set <traced>, retry connection if appropriate, and trace any neighbours that 
//		haven't already been traced
void MgtSocket::Trace()
{
	traced = true;

		// see if should reconnect
	if (state >= MGT_ST_MIN_RETRY) SendConnReq();

		// examine neighbours
	NetPortList::iterator q = net_port_state.begin();
	MgtSocket * p;
	unless (q == net_port_state.end()) do {
		unless (q->second == NET_PORT_STATE_PT_PT) continue;
		p = LinkPartner(q->first);
		unless (p == NULL || p->traced) p->Trace();
	} until (++q == net_port_state.end());
}


void MgtSocket::OnIdle()
{
	MibObject * m;
	CString s;
	CString s2;
	int i, j, k;
	uint8_t byte;
	CStdioFile f;
	CFile fb;
	FlashMap::iterator p;
	FlashMap::iterator code_area[2][16];
	CByteArray image_buf;	// see comments where used
	CFileException err;		// for debug

	if (theApp.link_socket->state == LINK_ST_ACTIVE) switch (upd_state) {
case UPD_ST_BEGIN:
			// collect product code from unitIdentity
		m = GetObject("1.0.62379.1.1.1.4.0");
		if (m == NULL) {
			upd_state = UPD_ST_NO_INFO;
			break;
		}
		if (m->at(0) != 0 || m->at(1) != 0x90 || m->at(2) != 0xA8) {
				// not a Nine Tiles product
			upd_state = UPD_ST_NOT_RECOGD;
			break;
		}

			// collect <sw_ver> from unitFirmwareVersion
		s = GetObject("1.0.62379.1.1.1.8.0")->StringValue();
		sw_ver[0].Invalidate();
		sw_ver[1].Invalidate();
		if (s.IsEmpty() || s.Left(3) != "VM ") goto have_sw_ver;
		s = s.Mid(3);
		i = s.Find(';');
		if (i < 0) goto have_sw_ver;
		sw_ver[0].FromFilename(s.Left(i));
		if (s.Right(3) == "???") {
			upd_state = UPD_ST_FROM_ISE;
			break;
		}
		if (s.Mid(i, 8) == "; logic ") sw_ver[1].FromFilename(s.Mid(i + 8));
have_sw_ver:
			// read product file
		i = 3;
		do product_code[i] = m->at(i+5); until (--i < 0);
case UPD_ST_HAVE_SW_VER:
			// enter here after changing product code
		s.Format("product~%d-%d-%d-%d.9t3prd", 
						product_code[0], product_code[1], 
								product_code[2], product_code[3]);
		if (!f.Open(s, CFile::modeRead, &err)) {
			s.SetAt(s.GetLength() - 4, '4');
			if (!f.Open(s, CFile::modeRead, &err)) {
				upd_state = UPD_ST_NO_P_FILE;
				break;
			}
		}

			// here when we have opened the product file for reading
			// read the contents and look for "software" lines
			// +++ this code is currently tailored to the Nine Tiles 
			//		platform; we assume:
			//		(1) unitFirmwareVersion is formatted as 
			//				"VM " + v1 + "; logic " + v2
			//			where each v is a version number
			//		(2) the filenames in the product file end in 
			//				'~' + v + '.' + ext
			//			where <ext> is "9t3bin" for the VM code and 
			//			"bit" for the logic
			//		(3) the files are in our format, not the binary 
			//			image specified in 62379-1
			//		We just compare the <v> strings to see whether the 
			//		software is up-to-date.
			// +++ we need to tighten up 62379-1 such that the process 
			//		of discovering what's in the flash, what's actually 
			//		running, and what filename to read to get the 
			//		required version, is not product-specific; 
			//		extension on a 62379-1 filename doesn't need to be 
			//		specified in the standard but we need to be sure 
			//		it's not .9t3bin or .9t4bin or .bit
			// +++ I'm assuming the flash will be class 2 (firmware); 
			//		I'm not sure 62379-1 makes that explicit; I'm also 
			//		not sure it was a good idea to include the storage 
			//		class in the product file, e.g. the image might be 
			//		able to be loaded into either flash or RAM or disc
		vl_type[0] = -1;
		vl_type[1] = -1;
		while (f.ReadString(s)) {
				// NB (1) the documentation for CString::MakeLower() 
				//		says it returns an all-lowercase copy of <this>; 
				//		if you read it really carefully you find it 
				//		converts <this> as well
				//	(2) <sscanf_s> doesn't seem to count the assignment 
				//		for %n, so we have to find a different way of 
				//		checking whether the '=' is present
			s2 = s;
			s2.MakeLower();	// leaving the filename in <s> unchanged
			k = -1;
			if (sscanf_s(s2, " software %x %x = %n", 
								&i, &j, &k) == 2 && j == 2 && k > 0) {
					// have a line that specifies a filename for 
					//		something that goes in flash
				if (s2.Right(7) == ".9t4bin" || 
										s2.Right(7) == ".9t3bin") {
					vl_type[0] = i;
					vl_fn[0] = s.Mid(k);
				}
				else if (s2.Right(4) == ".bit") {
					vl_type[1] = i;
					vl_fn[1] = s.Mid(k);
				}
			}
					// see if we have a "last serial number"
					// +++ ought to update it if the boot logic is uploaded
			else sscanf_s(s2, " last_serial = %d", &last_serial);
		}
		f.Close();
		if (vl_type[0] < 0 || vl_type[1] < 0) {
			upd_state = UPD_ST_BAD_P_FILE;
			break;
		}

		j = vl_fn[0].ReverseFind('~') + 1;
		k = vl_fn[0].ReverseFind('.');
		if (j < 1 || k < j || !vl_ver[0].FromFilename(vl_fn[0].Mid(j, k-j))) {
			upd_state = UPD_ST_BAD_P_FILE;
			break;
		}

		j = vl_fn[1].ReverseFind('~') + 1;
		k = vl_fn[1].ReverseFind('.');
		if (j < 1 || k < j || !vl_ver[1].FromFilename(vl_fn[1].Mid(j, k-j))) {
			upd_state = UPD_ST_BAD_P_FILE;
			break;
		}

			// compare with the software that is running in the unit
			// we know the <vl_ver> values are valid, so if <sw_ver> isn't we'll 
			//		set <vl_update> true 
		vl_update[0] = (sw_ver[0] != vl_ver[0]);
		vl_update[1] = (sw_ver[1] != vl_ver[1]);
		unless (vl_update[0] || vl_update[1]) {
			upd_state = UPD_ST_NO_ACTION;	// both up-to-date
			break;
		}

		StartCollectFlashMap();
		break;


case UPD_ST_HAVE_MAP:
			// here when the flash map has been collected; we've checked 
			//		previously that at least one of the software components 
			//		needs updating and its filename has the version number 
			//		in the expected place
			// +++ note that we only read the flash if the software the unit says it 
			//		is running is different from what the product file says; for 
			//		instance if the flash has been updated but the unit not reset, 
			//		and we revert the product file to the previous version, it won't 
			//		notice that the flash is different; maybe there should be a user 
			//		action to say "check what is in the flash anyway"
			// initialise the maps to the iterator equivalent of NULL
		p = flash.begin();
		if (p == flash.end()) {
				// map is empty: assume this is because we don't have a high 
				//		enough privilege level to read it
			upd_state = UPD_ST_NOT_MAINT;
			goto break_out_of_switch;
		}

		i = 16;
		do {
			code_area[0][--i] = flash.end();
			code_area[1][i] = flash.end();
		} while (i > 0);

		do if (p->second.status == AREA_STATUS_VALID) {
			if (p->second.data_type == vl_type[0]) i = 0;
			else if (p->second.data_type == vl_type[1]) i = 1;
			else continue;

				// now <i> is the index for the relevant area type
				// check the serial number
			j = p->second.serial;	// in range 0 to 255
			if (j > 15 || code_area[i][j] != flash.end()) {
					// illegal serial number, or more than one with the same 
					//		serial number
				upd_state = UPD_ST_BAD_FLASH;
				goto break_out_of_switch;
			}
			code_area[i][j] = p;
		} until (++p == flash.end());

			// now we have maps of the serial numbers of all the "normal" code 
			//		areas in the flash
			// check the serial numbers in each; we set <vl_serial> to the 
			//		"latest" serial numbers, using the same algorithm as in the 
			//		VM loader code
		i = 0;
		do {
			if (code_area[i][0] == flash.end()) {
					// serial number 0 not present; search backwards from 15
				j = 15;
				while (true) {
					if (code_area[i][j] != flash.end()) {
							// <j> is a "latest" serial number
						flash_ver[i] = code_area[i][j]->second.vn;
							// switch the "requires update" indication to depending 
							//		on the flash version rather than the one that's 
							//		running
						vl_update[i] = (flash_ver[i] != vl_ver[i]);
						unless (vl_update[i]) break; // if up-to-date

							// else check there is a second unused serial number 
							//		above it so it won't become illegal if we 
							//		upload another image
						if (j < 15 || code_area[i][1] == flash.end()) break;
						vl_update[i] = false;
						upd_state = UPD_ST_MAKE_SPACE;
						goto break_out_of_switch;
					}
					if (--j <= 0) {
							// here if none at all used
							// we set <j> to 1 so that the software will have 
							//		serial number 2, avoiding the bug in the 
							//		loader code in logic beta 22 and earlier 
							//		which fails to find the VM code if there's 
							//		just one and it's serial number 1
						j = 1;
						break;
					}
				}
			}
			else {
					// serial number 0 is present; search forwards from 1
				j = 1;
				while (true) {
					if (code_area[i][j] == flash.end()) {
							// <j-1> is the "latest" serial number
						j -= 1;
						flash_ver[i] = code_area[i][j]->second.vn;
							// switch the "requires update" indication to depending 
							//		on the flash version rather than the one that's 
							//		running
						vl_update[i] = (flash_ver[i] != vl_ver[i]);
						unless (vl_update[i]) break; // if up-to-date

							// else check there is a second unused serial number 
							//		above it so it won't become illegal if we 
							//		upload another image
						if (j < 14 && code_area[i][j+2] == flash.end()) break;
						vl_update[i] = false;
						upd_state = UPD_ST_MAKE_SPACE;
						goto break_out_of_switch;
					}
					if (++j > 15) {
							// all 16 serial numbers are in use
						vl_update[i] = false;
						upd_state = UPD_ST_MAKE_SPACE;
						goto break_out_of_switch;
					}
				}
			}
			vl_serial[i] = j;
		} while (++i < 2);

			// now <vl_update> shows what (if anything) needs to be uploaded, and 
			//		<vl_serial> the "latest" serial numbers
		unless (vl_update[0] || vl_update[1]) {
				// software in flash is up-to-date; check whether anything 
				//		needs to be erased
			upd_state = UPD_ST_TIDY_UP;
			break;
		}

			// here to generate the image and start uploading
		unless (OkToUpdate()) break;

			// if both need updating, we do the logic first, then collect the map 
			//		again before doing the VM code
			// +++ this assumes the new map will indeed show it as correct
		if (vl_update[1]) {
				// upload logic
				// +++ we checked the extension earlier, so can assume this is a 
				//		Xilinx .bit file and not the format specified in 62379-1; 
				//		we also assume it was generated for SPI wifth 4 and 
				//		includes the MultiBoot stub (the latter selected by the 
				//		"place MultiBoot settings into bit stream" option); the 
				//		loader code implements the functionality of the stub, so 
				//		we locate the data by the second sync word
			if (!fb.Open(vl_fn[1], CFile::modeRead)) {
				upd_state = UPD_ST_NO_LOGIC;
				break;
			}

			k = (int)fb.GetLength();
			if (k < 4096) {
				upd_state = UPD_ST_BAD_LOGIC;
				break;
			}
				// read the file into a CByteArray, then transfer it to <image>
				// +++ ought to read it directly into <image>, but the version of 
				//		Visual Studio I'm using doesn't implement <std::vector::data>
				// +++ however, to aid portability to an environment that does 
				//		implement it I copy the whole file into <image> rather than 
				//		looking for the start of the data in the CByteArray and only 
				//		copying what we need
			image_buf.SetSize(k);
			i = fb.Read(image_buf.GetData(), k);
			fb.Close();
			if (i != k) {
				upd_state = UPD_ST_BAD_LOGIC;
				break;
			}
			image.resize(k);
			i = 0;
			do { image[i] = image_buf[i]; i += 1; } while (i < k);

				// look for the second sync word
			i = 0;	// offset in data
			j = 0;	// number of sync word bytes we've seen so far
			do {
				byte = image[i++];
				switch (j & 3) {
		case 0:		if (byte == 0xAA) j += 1;
					break;

		case 1:		if (byte == 0x99) j += 1; else j &= ~3;
					break;

		case 2:		if (byte == 0x55) j += 1; else j &= ~3;
					break;

		case 3:		if (byte == 0x66) j += 1; else j &= ~3;
				}
			} until (j >= 8 || i >= 4096);

			if (j < 8) {
				upd_state = UPD_ST_BAD_LOGIC;
				image.clear();
				break;
			}

				// now we know the length that will be required
				// <k> is total length of image, <i> is offset to byte after 
				//		second sync word
			i -= 20; // offset to where atart of image will be
			image.erase(image.begin(), image.begin() + i); // remove unwanted bytes

				// overwrite the first four bytes with the version number and the 
				//		next four with the product code; the FPGA image in flash 
				//		will begin with the area header, version number, product 
				//		code, 8 bytes of FF, and the sync word; we need to ensure 
				//		that the version number and product code can't include a 
				//		sync word, but that just needs all the components other 
				//		than the beta number to be less than 85
			image[0] = vl_ver[1][3];	// beta number
			image[1] = vl_ver[1][0];
			image[2] = vl_ver[1][1];
			image[3] = vl_ver[1][2];

			i = 3;
			j = vl_fn[1].ReverseFind('c');
			/*if (j >= 4 && sscanf_s(vl_fn[1].Mid(j-4), "logic-%d-%d-%d-%d", 
					&p_code_local[0], &p_code_local[1], 
						&p_code_local[2], &p_code_local[3]) == 4) 
							do image[i+4] = p_code_local[i]; while (--i >= 0);
			else*/ do image[i+4] = product_code[i]; while (--i >= 0);

			if (vl_type[1] == 4) {
					// replace the FFs with the 64-bit identifier
				image[8] = 0;
				image[9] = 0x90;
				image[10] = 0xA8;
				image[11] = 0x99;
				image[12] = 0;
				image[13] = 0;
				image[14] = 0;
				image[15] = last_serial + 1;
			}
			StartUpload(1);
			break;
		}

			// here if there's something to upload but it's not the logic, so 
			//		must be the VM code
			// +++ as with the logic, we've checked the extension and 
			//		assume it's the format the compiler outputs; I think 
			//		it does actually conform to the 62379-1 format, but 
			//		we ought to check the header
		if (!fb.Open(vl_fn[0], CFile::modeRead)) {
			upd_state = UPD_ST_NO_VM_FILE;
			break;
		}

		k = (int)fb.GetLength();
			// read the file into a CByteArray, then transfer it to <image>
			// +++ see remarks for logic file above
		image_buf.SetSize(k);
		i = fb.Read(image_buf.GetData(), k);
		fb.Close();
		if (i != k) {
			upd_state = UPD_ST_BAD_VM_FILE;
			break;
		}
		image.resize(k);
		i = 0;
		while (i < k) { image[i] = image_buf[i]; i += 1; }

		i = 0;
		while (i < k && image[i] != 0x0D) i += 1;
		j = i + 1;
		while (j < k && image[j] != 0x0D) j += 1;
		if (j >= k) {
				// haven't found the two carriage returns
			upd_state = UPD_ST_BAD_VM_FILE;
			break;
		}

			// here with <i> pointing to the first carriage return and <j> to 
			//		the second
			// +++ ought to read the length and check it corresponds to the 
			//		length of the data; also ought to check the checksum
		image.erase(image.begin(), image.begin() + j + 1); // remove unwanted bytes

		StartUpload(0);
		break;


case UPD_ST_TIDY_UP:
case UPD_ST_MAKE_SPACE:
			// mark "invalid" any software that isn't one of the three latest
			// +++ originally we kept four but in unit 13 that doesn't leave 
			//		enough space to load another one, and I think 3 is plenty
		p = flash.begin();
		while (p != flash.end()) {
			if (p->second.data_type == vl_type[0]) i = 0;
			else if (p->second.data_type == vl_type[1]) i = 1;
			else { p++; continue; }

				// here if the type matches software <i>: mark it to be deleted 
				//		if it's more than 2 older than the latest one
			if (((vl_serial[i] - p->second.serial) & 15) > 2)
										p->second.status = AREA_STATUS_INVALID;
			p++;
		}

			// now look for the first area that is marked "invalid", either 
			//		because we've just changed its status or because that 
			//		was the status reported by the managed unit
		SendNextErase();	// sets appropriate state
		break;
	}
break_out_of_switch:

	if (upd_state != upd_state_view) {
			// screen may need to be repainted
		UpdateDisplay();
		upd_state_view = upd_state;
	}
}


// start collecting the flash map
void MgtSocket::StartCollectFlashMap()
{
	flash.clear();

	unless (theApp.privilege == PRIV_MAINTENANCE) {
		upd_state = UPD_ST_NOT_MAINT;
		return;
	}

	unless (theApp.link_partner == this) {
			// need a password
		if (password_state == PW_NOT_SET) GetPassword();
		unless (password_state == PW_VALID) {
			upd_state = UPD_ST_NOT_MAINT;
			return;
		}
	}

	uint8_t b[14];
	b[0]  = 0x1F;
	b[2]  = 6;		// OID tag
	b[3]  = 7;		// length of OID
	b[4]  = 0x28;	// OID = 1.0.62379.1.1.5
	b[5]  = 0x83;
	b[6]  = 0xE7;
	b[7]  = 0x2B;
	b[8]  = 1;
	b[9]  = 1;
	b[10] = 5;
	TxNewMessage(b, 11, true, true);
	upd_state = UPD_ST_COLLECT_MAP;
}


// ask the user to input the unit's password
// no change to password state if Cancel; CANCELLED if "OK" with box unticked
void MgtSocket::GetPassword()
{
	CSupplyPassword qp;
	qp.unit = DisplayName();
	password_state = PW_REQUESTED;

		// set the caption and the current password
	switch (theApp.privilege) {
case PRIV_OPERATOR:
		qp.caption = "Unit's password for operator privilege level";
		break;
case PRIV_SUPERVISOR:
		qp.caption = "Unit's password for supervisor privilege level";
		break;
case PRIV_MAINTENANCE:
		qp.caption = "Unit's password for maintenance privilege level";
		break;
default:qp.caption.Format("Unit's password for privilege level %d", 
													theApp.privilege);
	}

	if (password_state == PW_VALID) qp.pw = PasswordToString(password_string);

	int i = qp.DoModal();
	if (i != IDOK) return;
	StringToPassword(qp.pw, password_string);
	password_state = qp.include_password == BST_CHECKED ? PW_VALID : PW_CANCELLED;
}


// write <n> 64-bit words big-endianly (i.e. in network byte order) to 
//		(n*8)-byte buffer <b>
// assumes <n > 0>
// note that the parameters are in the same order as for <memcpy>
void FlexilinkSocket::CopyLongwordsToNetwork(uint8_t * b, uint64_t * w, int n)
{
	int j;
	do {
		j = 64;
		do {
			j -= 8;
			*b++ = (uint8_t)((*w) >> j);
		} while (j > 0);
		w += 1;
		n -= 1;
	} while (n > 0);
}


// ask whether to update if haven't already asked
// returns whether should update; sets state UPD_ST_NOT_MAINT or 
//			UPD_ST_USER_REFUSED if not
// +++ ideally the ControllerDoc window should switch to <this> before asking 
//			the question, but that requires waiting until the repainting has 
//			been done before displaying the dialogue box; note that SetUnit 
//			updates the title immediately but the repainting is delayed (and 
//			even if it called UpdateAllViews that only invalidates them, so 
//			I think the repainting would still be delayed)
bool MgtSocket::OkToUpdate() {
	if (theApp.privilege < PRIV_MAINTENANCE) {
		upd_state = UPD_ST_NOT_MAINT;
		return false;
	}

	if (user_update_flags < 0) {
		if (theApp.update_flags < 0) {
			CQuery q("Update flash in " + unit_name + " as follows?");
			int i = 0;
			CString s;
			bool boot_version = false;
				// +++ ought to check the boot code will go to sector 0
			if (vl_update[0]) {
				if (vl_type[0] == 5) {
					q.action[0] = "upload boot VM software " + 
														vl_ver[0].ToFilename();
					boot_version = true;
				}
				else {
					s = flash_ver[0].ToFilename();
					if (s.IsEmpty()) s = "[unknown]";
					q.action[0] = "upload VM software " + 
							vl_ver[0].ToFilename() + " (to supersede " + s + ')';
				}
				i = 1;
			}
			if (vl_update[1]) {
				if (vl_type[1] == 4) {
					q.action[i] = "upload boot FPGA logic " + 
														vl_ver[1].ToFilename();
					boot_version = true;
				}
				else {
					s = flash_ver[1].ToFilename();
					if (s.IsEmpty()) s = "[unknown]";
					q.action[i] = "upload FPGA logic " + 
							vl_ver[1].ToFilename() + " (to supersede " + s + ')';
				}
				i += 1;
			}
			if (boot_version) q.action[i].Format("unit number %d", last_serial + 1);
			else q.action[i] = "tidy up";
			i = q.DoModal();
			if (i == IDOK) {
				user_update_flags = q.result & QRES_YES;
				if (q.result & QRES_ALL) theApp.update_flags = user_update_flags;
			}

			theApp.controller_doc->SetUnit(this); // see note above
		}
		else user_update_flags = theApp.update_flags;
	}

	if (user_update_flags > 0) return true;

	upd_state = UPD_ST_USER_REFUSED;
	return false;
}


// look for a free area in flash into which <image> can be uploaded; if 
//		found, kick-start the process of uploading by sending the request 
//		to change its state, else just set state UPD_ST_TIDY_UP (so we'll 
//		look for something to erase) and remove the image
// <i> is the index into the <vl_> arrays
// caller is assumed to have checked that neither of the two serial 
//		numbers following <vl_serial[i]> is in use
// increments <vl_serial[i]> so it corresponds to the area into which the 
//		new image is being uploaded
void MgtSocket::StartUpload(int i) {
	int len = (int)image.size();
	if (len <= 0) {
			// no image; must have failed to load
		if (upd_state < UPD_ST_MIN_FILE_ERR || 
					upd_state > UPD_ST_MAX_FILE_ERR) upd_state = UPD_ST_NO_S_FILE;
		return;
	}

	upd_area = flash.begin();
	until (upd_area == flash.end()) {
		if (upd_area->second.status == AREA_STATUS_EMPTY && 
									upd_area->second.length >= len) {
				// <upd_area> points to the area to be used
			upd_area->second.length = len;
			upd_area->second.data_type = vl_type[i];
			vl_serial[i] = (vl_serial[i] + 1) & 15;
			upd_area->second.serial = vl_serial[i];
			upd_area->second.vn = vl_ver[i];

				// set the area to "writing"
				// OID = 1.0.62379.1.1.5.1.1.4.a (a = area)
			uint8_t b[24];
			b[0]  = 0x30;	// "Set" request
			b[2]  = ASN1_TAG_OID;
			b[4]  = 0x28;
			b[5]  = 0x83;
			b[6]  = 0xE7;
			b[7]  = 0x2B;
			b[8]  = 1;
			b[9]  = 1;
			b[10] = 5;
			b[11] = 1;
			b[12] = 1;
			b[13] = 4;	// column number for swaStatus
			uint8_t * p = b + 14;
			AddIndex(p, upd_area->first);
				// <p> points to where the tag byte for the value will go
			b[3] = (uint8_t)((p - b) - 4);	// assumed < 128
			*p++ = ASN1_TAG_INTEGER;
			*p++ = 1;
			*p++ = AREA_STATUS_WRITING;
				// now <b> points to the first byte of the message and 
				//		<p> to the byte after last
			TxNewMessage(b, (int)(p - b), true, true);
			upd_state = UPD_ST_UPLOADING;
			upd_offset = -4;
			return;
		}

			// here to try next area
		upd_area++;
	}

		// here if no free areas big enough
		// +++ need to signal we weren't able to load the s/w so (a) user 
		//		isn't told needs reboot to run new s/w (b) "tidy up" code 
		//		can make sure it leaves enough room (including maybe 
		//		asking user whether it's OK to leave fewer than normal)
	upd_state = UPD_ST_TIDY_UP;
	image.clear();
}


//////////////////////// CControllerDoc

IMPLEMENT_DYNCREATE(CControllerDoc, CDocument)

BEGIN_MESSAGE_MAP(CControllerDoc, CDocument)
END_MESSAGE_MAP()


// CControllerDoc construction/destruction

CControllerDoc::CControllerDoc()
{
	unit = NULL;
	display_select = MIB_DISPLAY;
}

CControllerDoc::~CControllerDoc()
{
}


BOOL CControllerDoc::OnNewDocument()
{
	unless (CDocument::OnNewDocument()) return FALSE;

	unless (theApp.controller_doc == NULL && 
			theApp.link_socket == NULL) return FALSE; // only support one

	theApp.controller_doc = this;

// at this point earlier versions read in one or more "configuration" files

		// create the link to the Flexilink network
	LinkSocket * skt = new LinkSocket();

		// +++ NOTE: calling skt->Bind(0) after Create fails with WSAEINVAL 
		//		("invalid argument was supplied") in the assembly-code 
		//		part, but Create seems to call Bind(0) anyway; also, doing 
		//		Connect at this point seems to stop the broadcast being sent
	int err; // to hold result of GetLastError() for debug
	unless (skt && skt->Create(0, SOCK_DGRAM, FD_READ | FD_CLOSE)) {
		err = GetLastError(); // for debug
		delete skt;
		return FALSE;
	}
	theApp.link_socket = skt;

		// Request a timer for retries and checking for keepalives
		// +++ TEMP: double the timeout to allow for programming flash in 
		//		the unit to which we're directly connected (not sure why it 
		//		takes longer, may be because the VM is occupied forwarding
		//		replies from other units)
//	if (theApp.m_pMainWnd->SetTimer(IDT_500MSEC, 500, NULL) == 0) 
	if (theApp.m_pMainWnd->SetTimer(IDT_500MSEC, 1000, NULL) == 0) 
		AfxMessageBox("No timer available; unacknowledged messages "
				"will not be repeated", MB_OK | MB_ICONEXCLAMATION);

		// now send the Link Request
	return skt->Init();
}


// increment <display_select> and update display
void CControllerDoc::SelectNextScp()
{
	bool wrapped;
	if (display_select < 0) {
		display_select = -1;
		wrapped = true;
	}
	else wrapped = false;

	unless (theApp.scp.empty()) do {
		display_select += 1;
		if ((unsigned int)display_select >= theApp.scp.size()) {
			if (wrapped) {
					// have tried them all
				display_select = CONSOLE_DISPLAY;
				break;
			}
			display_select = 0;
			wrapped = true;
		}
	} while (theApp.scp.at(display_select) == NULL);

	UpdateAllViews(NULL);
}


// set <unit> to <u> and update display; copes with <this> being NULL
void CControllerDoc::SetUnit(MgtSocket * u)
{
	if (this == NULL) return;
	unit = u;
	UpdateTitle();
	BringToFront();
	UpdateAllViews(NULL);
}


// give the focus to the Controller window
void CControllerDoc::BringToFront()
{
	POSITION p = GetFirstViewPosition();
	unless (p) return;
	CView * v = GetNextView(p);
	if (v) v->SetActiveWindow();
}


// return whether <u> is currently selected for display
bool CControllerDoc::Selected(class FlexilinkSocket * u)
{
	if (this == NULL) return false;

	if (display_select < 0) {
			// displaying the MIB or console for <unit>
		MgtSocket * um = dynamic_cast<MgtSocket *>(u);
		return (um && um == unit);
	}

		// else displaying a SCP interface
	AnalyserDoc * ua = dynamic_cast<AnalyserDoc *>(u);
	return (ua && ua == theApp.scp.at(display_select));
}


// can't put this in the class declaration because that comes before 
//		declaration of <MgtSocket>
// +++ Release version fails to link in VS2015 if "inline" is present
/*inline*/ void CControllerDoc::UpdateTitle() {
	SetTitle(unit == NULL ? "" : unit->unit_name);
}

// CControllerDoc serialization

void CControllerDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}


// CControllerDoc diagnostics

#ifdef _DEBUG
void CControllerDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CControllerDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

