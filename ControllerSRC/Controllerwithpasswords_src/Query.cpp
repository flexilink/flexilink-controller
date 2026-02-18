// Query.cpp : implementation file
//

#include "stdafx.h"
#include "Controller.h"
#include "Query.h"


// CQuery dialog

IMPLEMENT_DYNAMIC(CQuery, CDialog)

CQuery::CQuery(CWnd* pParent /*=NULL*/)
	: CDialog(CQuery::IDD, pParent)
{
	//{{AFX_DATA_INIT(CEditProductDlg)
	intro = _T("");
	//}}AFX_DATA_INIT
	result = -1;
}

CQuery::CQuery(CString msg)
	: CDialog(CQuery::IDD, NULL)
{
	intro = msg;
	result = -1;
}

CQuery::~CQuery()
{
}

void CQuery::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CQuery)
	DDX_Text(pDX, IDC_EDIT1, intro);
	DDX_Text(pDX, IDC_EDIT3, action[0]);
	DDX_Text(pDX, IDC_EDIT4, action[1]);
	DDX_Text(pDX, IDC_EDIT5, action[2]);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CQuery, CDialog)
	ON_BN_CLICKED(IDYES, &CQuery::OnBnClickedYes)
	ON_BN_CLICKED(IDNO, &CQuery::OnBnClickedNo)
	ON_BN_CLICKED(IDOK, &CQuery::OnBnClickedYesToAll)
	ON_BN_CLICKED(IDIGNORE, &CQuery::OnBnClickedNoToAll)
END_MESSAGE_MAP()


// CQuery message handlers

void CQuery::OnBnClickedYes()
{
	result = QRES_YES;
	OnOK();
}

void CQuery::OnBnClickedNo()
{
	result = 0;
	OnOK();
}

void CQuery::OnBnClickedYesToAll()
{
	result = QRES_YES | QRES_ALL;
	OnOK();
}

void CQuery::OnBnClickedNoToAll()
{
	result = QRES_ALL;
	OnOK();
}
