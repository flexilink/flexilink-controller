// PcodeChange.cpp : implementation file for CPcodeChange 
//		and CSupplyPassword

#include "stdafx.h"
#include "Controller.h"
#include "PcodeChange.h"
#include "afxdialogex.h"


// Potentially useful calls:
//		CWnd* GetDlgItem(int nID) const; to get a control?
//		COleControl::SetEnabled
// COleControl * w = dynamic_cast<COleControl *>(qp.GetDlgItem(IDC_CHECK2));
// unless (w == NULL) w->SetEnabled(b);

// ------- change product code --------

IMPLEMENT_DYNAMIC(CPcodeChange, CDialog)

CPcodeChange::CPcodeChange(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_DIALOG2, pParent)
	, pcode(_T(""))
{
	reset_update_state = BST_UNCHECKED;
	include_password = BST_CHECKED;
	change_password = BST_UNCHECKED;
	configure_tunnel = BST_UNCHECKED;
	remote_unit = 0;
	remote_port = 1;
	local_port = 1;
}

CPcodeChange::~CPcodeChange()
{
}

void CPcodeChange::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK1, reset_update_state);
	DDX_Text(pDX, IDC_EDIT1, pcode);
//	DDV_MaxChars(pDX, pcode, 15); +++ longer when error message added
	DDX_Check(pDX, IDC_CHECK3, include_password);
	DDX_Check(pDX, IDC_CHECK4, change_password);
	DDX_Text(pDX, IDC_EDIT2, pw);
	DDV_MaxChars(pDX, pw, 32);
	DDX_Check(pDX, IDC_CHECK2, configure_tunnel);
	DDX_Text(pDX, IDC_EDIT4, remote_unit);
	DDV_MinMaxInt(pDX, remote_unit, 0, 35);
	DDX_Text(pDX, IDC_EDIT9, remote_port);
	DDV_MinMaxInt(pDX, remote_port, 1, 13);
	DDX_Text(pDX, IDC_EDIT8, local_port);
	DDV_MinMaxInt(pDX, local_port, 1, 13);
}


BEGIN_MESSAGE_MAP(CPcodeChange, CDialog)
END_MESSAGE_MAP()


// CPcodeChange message handlers


// ------- set password --------

IMPLEMENT_DYNAMIC(CSupplyPassword, CDialog)

CSupplyPassword::CSupplyPassword(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_DIALOG3, pParent)
	, caption(_T(""))
{
	include_password = BST_CHECKED;
}

CSupplyPassword::~CSupplyPassword()
{
}

void CSupplyPassword::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT3, unit);
	DDX_Text(pDX, IDC_EDIT1, caption);
	DDX_Text(pDX, IDC_EDIT2, pw);
	DDV_MaxChars(pDX, pw, 32);
	DDX_Check(pDX, IDC_CHECK3, include_password);
}


BEGIN_MESSAGE_MAP(CSupplyPassword, CDialog)
END_MESSAGE_MAP()


// CSupplyPassword message handlers


// ------- set new name, location, and passwords for unit --------

IMPLEMENT_DYNAMIC(CSetUnitPasswords, CDialog)

CSetUnitPasswords::CSetUnitPasswords(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_DIALOG4, pParent)
	, caption(_T(""))
{
	reset_opr = BST_UNCHECKED;
	reset_svr = BST_UNCHECKED;
	reset_maint = BST_UNCHECKED;
}

CSetUnitPasswords::~CSetUnitPasswords()
{
}

void CSetUnitPasswords::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, caption);
	DDX_Check(pDX, IDC_CHECK1, reset_opr);
	DDX_Text(pDX, IDC_EDIT4, operator_password);
	DDV_MaxChars(pDX, operator_password, 32);
	DDX_Check(pDX, IDC_CHECK2, reset_svr);
	DDX_Text(pDX, IDC_EDIT2, supervisor_password);
	DDV_MaxChars(pDX, supervisor_password, 32);
	DDX_Check(pDX, IDC_CHECK3, reset_maint);
	DDX_Text(pDX, IDC_EDIT3, maintenance_password);
	DDV_MaxChars(pDX, maintenance_password, 32);
	DDX_Text(pDX, IDC_EDIT6, name);
	DDX_Text(pDX, IDC_EDIT7, location);
}


BEGIN_MESSAGE_MAP(CSetUnitPasswords, CDialog)
END_MESSAGE_MAP()


// CSetUnitPasswords message handlers


// ------- set new name, location, and passwords for unit --------

IMPLEMENT_DYNAMIC(CSetUnitName, CDialog)

CSetUnitName::CSetUnitName(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_DIALOG5, pParent)
	, caption(_T(""))
{
}

CSetUnitName::~CSetUnitName()
{
}

void CSetUnitName::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, caption);
	DDX_Text(pDX, IDC_EDIT6, name);
	DDX_Text(pDX, IDC_EDIT7, location);
}


BEGIN_MESSAGE_MAP(CSetUnitName, CDialog)
END_MESSAGE_MAP()


// CSetUnitName message handlers


// ------- set new name etc for port --------

IMPLEMENT_DYNAMIC(CSetPortNameEtc, CDialog)

CSetPortNameEtc::CSetPortNameEtc(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_DIALOG6, pParent)
	, caption(_T(""))
{
	importance = 64;
}

CSetPortNameEtc::~CSetPortNameEtc()
{
}

void CSetPortNameEtc::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, caption);
	DDX_Text(pDX, IDC_EDIT6, name);
	DDX_Text(pDX, IDC_EDIT7, importance);
	DDV_MinMaxInt(pDX, importance, 0, 255);
}


BEGIN_MESSAGE_MAP(CSetPortNameEtc, CDialog)
END_MESSAGE_MAP()


// CSetPortNameEtc message handlers


// ------- set new name for port --------

IMPLEMENT_DYNAMIC(CSetPortName, CDialog)

CSetPortName::CSetPortName(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_DIALOG7, pParent)
	, caption(_T(""))
{
}

CSetPortName::~CSetPortName()
{
}

void CSetPortName::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, caption);
	DDX_Text(pDX, IDC_EDIT6, name);
}


BEGIN_MESSAGE_MAP(CSetPortName, CDialog)
END_MESSAGE_MAP()


// CSetPortName message handlers


// ------- translate password format --------

// assumes single-byte characters
#ifdef  UNICODE
			// CString is 16-bit characters
			// need to implement conversion between UTF-16 and UTF-8 (see 
			//		<extras> module in compiler)
		ASSERT(false);
#endif

// copy from four 64-bit words to CString
CString PasswordToString(uint64_t * pw)
{
	CString s;
	int n;
	uint8_t b;
	int i = 4;
	do {
		n = 64;
		do {
			n -= 8;
			b = (uint8_t)((*pw) >> n);
			if (b == 0) return s;
			s += b;
		} while (n > 0);
		pw += 1;
		i -= 1;
	} while (i > 0);
	return s;
}

// copy from CString to four 64-bit words
void StringToPassword(CString s, uint64_t * pw)
{
	uint64_t k;
	int y = s.GetLength();
	int i = 0; // counts characters
	while (i < y) {
		k = 0; // accumulates the word
		do {
			k = k << 8;
			if (i < y) k |= s[i];
			i += 1;
		} while ((i & 7) != 0);
		*pw++ = k;
		if (i >= 32) return;
	}
	do {
		*pw++ = 0;
		i += 8;
	} while (i < 32);
 }


// copy from n (default 4) 64-bit words to 8n bytes in network byte order
void BytesFrom64Bit(uint64_t * pw, uint8_t * m, int n) {
	int j;
	do {
		j = 64;
		do {
			j -= 8;
			*m++ = (uint8_t)((*pw) >> j);
		} while (j > 0);
		pw += 1;
		n -= 1;
	} while (n > 0);
}

// copy from 8n bytes in network byte order to n 64-bit words
void BytesTo64Bit(uint8_t * m, uint64_t * pw, int n) {
	uint64_t w;
	int i = -8; // counts bytes
	n -= 1;
	n *= 8;
	do {
		w = 0; // accumulates the word
		do {
			w = (w << 8) | *m++;
			i += 1;
		} while ((i & 7) != 0);
		pw[i >> 3] = w;
	} while (i < n);
}