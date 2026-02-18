// ControllerView.h : interface of the CControllerView class
//


#pragma once


class CControllerView : public CScrollView
{
protected: // create from serialization only
	CControllerView();
	DECLARE_DYNCREATE(CControllerView)

// Attributes
public:
	CControllerDoc* GetDocument() const;

// Operations
public:

// Overrides
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);

// Implementation
public:
	virtual ~CControllerView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	int Char5Width;				// width of 5 characters
	int CharHeight;				// height of 1 line
	CFont * text_font;			// for intialisation of the font

// Generated message map functions
protected:
	//{{AFX_MSG(C9tos2View)
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	virtual void OnInitialUpdate();
};

#ifndef _DEBUG  // debug version in ControllerView.cpp
inline CControllerDoc* CControllerView::GetDocument() const
   { return reinterpret_cast<CControllerDoc*>(m_pDocument); }
#endif

