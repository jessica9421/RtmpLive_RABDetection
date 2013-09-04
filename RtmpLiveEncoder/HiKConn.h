

#include "HKConnect/GeneralDef.h"
#include "cv.h"

/////////////////////////////////////////////////////////////////////////////

void CALLBACK g_RealDataCallBack_V30(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer,DWORD dwBufSize,void* dwUser);

class HiKConn
{
// Construction
public:
	HiKConn(HWND hwnd,HWND pichwnd,int index, CString DeviceIp, CString m_csUser, CString m_csPWD, UINT m_nDevPort);
	~HiKConn(void);
public:
	LOCAL_DEVICE_INFO m_struDeviceInfo;
	LONG m_lPlayHandle;
	LONG m_lDecID;
	CString DeviceIp;
	CString m_csUser;
	CString m_csPWD;
	UINT m_nDevPort;
	HWND hwnd;
	HWND pichwnd;
	int index;
public:
	BOOL StartHikCamera();
	BOOL DoLogin();
	void DoGetDeviceResoureCfg();
	BOOL StartPlay(int iChanIndex);
	void StopPlay();	
};



