#pragma once
#include "afxwin.h"
#include "resource.h"       // main symbols


// CQuery dialog

// Note that DoModal returns one of the following; you can't change what it returns
//
//	–1: could not create the dialog box
//	IDOK: OnOK (which is associated with the "OK" button) called
//	IDCANCEL: OnCancel (which is associated with the "CANCEL" button) called or the 
//				dialogue box window was closed
//	IDABORT: some kind of error not covered by any of the above
//
// I've set the dialogue box up so all the buttons set <result> and then do OnOK()

class CQuery : public CDialog
{
	DECLARE_DYNAMIC(CQuery)

public:
	CQuery(CWnd* pParent = NULL);   // standard constructor
	CQuery(CString msg);   // set text to <msg>
	virtual ~CQuery();

// Dialog Data
	enum { IDD = IDD_DIALOG1 };

		// message for display; there doesn't seem to be any way to set the text 
		//		in a "static" control at run time, but read-only edit controls 
		//		marked as "read only" and "no border" look the same so I've used 
		//		those; there are 4 of them (1 per line) because they don't seem 
		//		to understand newline characters
		// BTW it doesn't look as if you can do anything with static controls; if 
		//		there are several they are all identified as IDC_STATIC whereas 
		//		edit controls (for example) are IDC_EDIT1 etc so presumably you 
		//		can't use <CWnd::GetDlgItem> to get a <COleControl> object for it
	CString	intro;
	CString action[3];

		// <result> is bitwise encoded as follows; -ve if hasn't been set
		// I've set up all the buttons to write <result> and then call OnOK() 
		//		-- that seems clunky but seems to be how we have to do it 
		//		
#define QRES_YES 1	// d0 = 0 for no, 1 for yes
#define QRES_ALL 2	// d1 = 0 for just this unit, 1 for all units
	int result;	// -ve if nothing has been pressed

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedYes();
	afx_msg void OnBnClickedNo();
	afx_msg void OnBnClickedYesToAll();
	afx_msg void OnBnClickedNoToAll();
};
