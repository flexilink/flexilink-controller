// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#endif

// Define which versions of Windows will be supported; see WINVER in help index
// Refer to MSDN for the latest info on corresponding values for different platforms.

#ifndef WINVER
#define WINVER 0x0501		// Allow use of features not supported before XP
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WINXP	// smallest value supported by VS 2015
#endif						

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410	// Allow use of features not supported before 98
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0400	// Allow use of features not supported before IE 5.0
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

// make sure <rand_s> is included; needs to precede the first invocation of <stdlib.h> 
//		(which here is in <afx.h> which is in turn invoked by <afxwin.h>)
#define _CRT_RAND_S

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes
//#include <afxcview.h>		// may need this for some of the controls
#include <afxtempl.h>		// for the templates
#include <afxctl.h>			// for <COleControl> class

#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxsock.h>		// MFC socket extensions
