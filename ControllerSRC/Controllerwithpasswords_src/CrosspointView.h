#pragma once
#include "CrosspointDoc.h"


struct PortLocation {
	PortIdent p;	// <p.port> is -ve for the header line with the unit's name
	int y; };		// y value before line describing port is output


// CCrosspointView view

class CCrosspointView : public CScrollView
{
	DECLARE_DYNCREATE(CCrosspointView)

protected:
	CCrosspointView();           // protected constructor used by dynamic creation
	virtual ~CCrosspointView();

public:
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	CCrosspointDoc* GetDocument() const;

	int Char5Width;				// width of 5 characters
	int CharHeight;				// height of 1 line
	CFont * text_font;			// for initialisation of the font

	CArray<PortLocation, PortLocation> locs;	// entry for each port
	PortLocation sel_port;		// unit last clicked on; <unit == NULL> if none
	void RemoveFromLocs(MgtSocket * u);	// call if <u> being deleted

protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
};

#ifndef _DEBUG  // debug version in ControllerView.cpp
inline CCrosspointDoc* CCrosspointView::GetDocument() const
   { return reinterpret_cast<CCrosspointDoc*>(m_pDocument); }
#endif


