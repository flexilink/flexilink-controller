// Classes for the "change product code" dialogue box at maintenance 
//		level and to set the password at the other levels (except 
//		listener, where none of the "right click on unit name" 
//		operations are available

#pragma once

// ------- change product code --------

class CPcodeChange : public CDialog
{
	DECLARE_DYNAMIC(CPcodeChange)

public:
	CPcodeChange(CWnd* pParent = NULL);   // standard constructor
	virtual ~CPcodeChange();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG2 };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int reset_update_state; // values BST_CHECKED, BST_UNCHECKED
	CString pcode;			// product code
	int include_password;	// values BST_CHECKED, BST_UNCHECKED
	int change_password;	// values BST_CHECKED, BST_UNCHECKED
	CString pw;				// password
	int configure_tunnel;	// values BST_CHECKED, BST_UNCHECKED
	int remote_unit;		// unit number
	int remote_port;		// port number (1-4,10-13)
	int local_port;			// port number (1-4,10-13)
};


// ------- input password --------

class CSupplyPassword : public CDialog
{
	DECLARE_DYNAMIC(CSupplyPassword)

public:
	CSupplyPassword(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSupplyPassword();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG3 };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString unit;		// unit's name
	CString caption;	// privilege level
	int include_password;	// values BST_CHECKED, BST_UNCHECKED
	CString pw;			// password to be used in messages
};


// copy password between CString and uint64_t[4]
CString PasswordToString(uint64_t * pw);
void StringToPassword(CString s, uint64_t * pw);

// copy password (or random string) between message and uint64_t[n]
void BytesFrom64Bit(uint64_t * pw, uint8_t * m, int n = 4);
void BytesTo64Bit(uint8_t * m, uint64_t * pw, int n);


// ------- set new name, location, and passwords for unit --------

class CSetUnitPasswords : public CDialog
{
	DECLARE_DYNAMIC(CSetUnitPasswords)

public:
	CSetUnitPasswords(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSetUnitPasswords();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG4 };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString caption;
	int reset_opr;
	CString operator_password;
	int reset_svr;
	CString supervisor_password;
	int reset_maint;
	CString maintenance_password;
	CString name;
	CString location;
};


// ------- set new name and location for unit --------

class CSetUnitName : public CDialog
{
	DECLARE_DYNAMIC(CSetUnitName)

public:
	CSetUnitName(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSetUnitName();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG5 };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString caption;
	CString name;
	CString location;
};



// ------- set new name etc for port --------

class CSetPortNameEtc : public CDialog
{
	DECLARE_DYNAMIC(CSetPortNameEtc)

public:
	CSetPortNameEtc(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSetPortNameEtc();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG6 };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString caption;
	CString name;
	int importance;
};


class CSetPortName : public CDialog
{
	DECLARE_DYNAMIC(CSetPortName)

public:
	CSetPortName(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSetPortName();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG7 };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString caption;
	CString name;
};

