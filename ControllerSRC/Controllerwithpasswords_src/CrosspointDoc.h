#pragma once


struct PortIdent {
	class MgtSocket * unit;
	int port; };


// CCrosspointDoc document

class CCrosspointDoc : public CDocument
{
	DECLARE_DYNCREATE(CCrosspointDoc)

public:
	CCrosspointDoc();
	virtual ~CCrosspointDoc();
	virtual void Serialize(CArchive& ar);   // overridden for document i/o
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	bool inputs;			// else is list of outputs
	PortIdent sel_port;		// selected port; <unit == NULL> if none
	void RemoveFromLocs(MgtSocket * u);	// call if <u> being deleted

protected:
	virtual BOOL OnNewDocument();

	DECLARE_MESSAGE_MAP()
};
