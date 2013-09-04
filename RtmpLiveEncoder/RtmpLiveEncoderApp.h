
// RtmpStreaming.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// RtmpStreamingApp:
// See RtmpStreaming.cpp for the implementation of this class
//

class RtmpLiveEncoderApp : public CWinApp
{
public:
	RtmpLiveEncoderApp();

// Overrides
public:
	virtual BOOL InitInstance();
// Implementation

	DECLARE_MESSAGE_MAP()
};

extern RtmpLiveEncoderApp theApp;