// CrosspointDoc.cpp : implementation file
//

#include "stdafx.h"
#include "Controller.h"
#include "CrosspointView.h"


// CCrosspointDoc

IMPLEMENT_DYNCREATE(CCrosspointDoc, CDocument)

CCrosspointDoc::CCrosspointDoc()
{
	inputs = true;
	sel_port.unit = NULL;
}

BOOL CCrosspointDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument()) return FALSE;

	if (theApp.input_list == NULL) {
		theApp.input_list = this;
		SetTitle("Sources");
		return TRUE;
	}

	if (theApp.output_list != NULL) return FALSE;

	inputs = false;
	theApp.output_list = this;
	SetTitle("Destinations");
	return TRUE;
}

CCrosspointDoc::~CCrosspointDoc()
{
	if (theApp.input_list == this) theApp.input_list = NULL;
	if (theApp.output_list == this) theApp.output_list = NULL;
}


// replace any pointers to <u> in <CCrosspointView::locs> with NULL
// safe if <this> is NULL
void CCrosspointDoc::RemoveFromLocs(MgtSocket * u)
{
	if (this == NULL) return;
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL) {
		CCrosspointView * pView = 
					dynamic_cast<CCrosspointView *>(GetNextView(pos));
		if (pView) pView->RemoveFromLocs(u);
	}
}


BEGIN_MESSAGE_MAP(CCrosspointDoc, CDocument)
END_MESSAGE_MAP()


// CCrosspointDoc diagnostics

#ifdef _DEBUG
void CCrosspointDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CCrosspointDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// CCrosspointDoc serialization

void CCrosspointDoc::Serialize(CArchive& ar)
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


// CCrosspointDoc commands
