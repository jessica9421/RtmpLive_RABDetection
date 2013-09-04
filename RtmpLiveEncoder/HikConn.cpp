// HiKConn.cpp : implementation file
//

#include "stdafx.h"
#include "HiKConn.h"
#include <cxcore.h>
#include <cv.h>
#include <highgui.h>
#include "Thread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern "C" WINBASEAPI HWND WINAPI GetConsoleWindow ();
LONG lPort; //全局的播放库port号
LONG m_iPort;
 

void CALLBACK DecCBFun(long nPort,char * pBuf,long nSize, FRAME_INFO * pFrameInfo, long nReserved1,long /*nReserved2*/) //YV12(YUV的一种)-->IplImage
{
	if(pFrameInfo->dwFrameNum>0)
	{
		int index = nReserved1 ;
		HANDLE h[2] = {g_hEmptySemaphore[index],g_hMutex[index]};
		if(WaitForMultipleObjects(2,h,TRUE,0)==WAIT_TIMEOUT)
			return;
		
		//TRACE("%d %d", pFrameInfo->nWidth, pFrameInfo->nHeight);
		char * imgData = new char[640*480*1.5];
		memcpy(imgData, pBuf, 640*480*1.5);
		imagesData[index][in[index]] = imgData;
		
		in[index] =(in[index]+1)%SIZE_OF_BUFFER;
		ReleaseMutex(g_hMutex[index]);
		ReleaseSemaphore(g_hFullSemaphore[index],1,NULL);
		//delete[] imgData;

	}
}
void CALLBACK g_RealDataCallBack_V30(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer,DWORD dwBufSize,void* dwUser)
{
	HWND hWnd= GetConsoleWindow();
	int* index = (int*)dwUser;
	switch (dwDataType)
	{
	case NET_DVR_SYSHEAD: //系统头

		if (!PlayM4_GetPort(&lPort))  //获取播放库未使用的通道号
		{
			break;
		}
		m_iPort = lPort; //第一次回调的是系统头，将获取的播放库port号赋值给全局port，下次回调数据时即使用此port号播放
		if (dwBufSize > 0)
		{
			if (!PlayM4_SetStreamOpenMode(lPort, STREAME_REALTIME))  //设置实时流播放模式
			{
				break;
			}

			if (!PlayM4_OpenStream(lPort, pBuffer, dwBufSize, 1024*1024)) //打开流接口
			{
				break;
			}
			PlayM4_SetDecCallBack(lPort,DecCBFun);
			if (!PlayM4_Play(lPort, hWnd)) //播放开始
			{
				break;
			}
		}
	case NET_DVR_STREAMDATA:   //码流数据
		if (dwBufSize > 0 && lPort != -1)
		{
		
			if (!PlayM4_InputData(lPort, pBuffer, dwBufSize))
			{
				break;
			}
		}
	}

}

HiKConn::HiKConn(HWND hwnd,HWND pichwnd,int index,CString DeviceIp, CString m_csUser, CString m_csPWD, UINT m_nDevPort):hwnd(hwnd),pichwnd(pichwnd),index(index),DeviceIp(DeviceIp),m_csUser(m_csUser),m_csPWD(m_csPWD),m_nDevPort(m_nDevPort)
{
}

HiKConn::~HiKConn(void)
{
}

BOOL HiKConn::StartHikCamera(){
	if(!DoLogin())
		return FALSE;
	DoGetDeviceResoureCfg();  //获取设备资源信息	
	return TRUE;
}

BOOL HiKConn::DoLogin()
{
	//转换声明
	USES_CONVERSION;
	
	NET_DVR_DEVICEINFO_V30 DeviceInfoTmp;
	memset(&DeviceInfoTmp,0,sizeof(NET_DVR_DEVICEINFO_V30));
		
	m_lDecID = NET_DVR_Login_V30(T2A(DeviceIp),m_nDevPort,T2A(m_csUser), T2A(m_csPWD), &DeviceInfoTmp);
	if(m_lDecID == -1)
	{
		return FALSE;
	}
    m_struDeviceInfo.lLoginID = m_lDecID;
	m_struDeviceInfo.iDeviceChanNum = DeviceInfoTmp.byChanNum;
    m_struDeviceInfo.iIPChanNum = DeviceInfoTmp.byIPChanNum;
    m_struDeviceInfo.iStartChan  = DeviceInfoTmp.byStartChan;

	return TRUE;
}

/*************************************************
函数名:    	DoGetDeviceResoureCfg
函数描述:	获取设备的通道资源
输入参数:   
输出参数:   			
返回值:		
**************************************************/
void HiKConn::DoGetDeviceResoureCfg()
{
	NET_DVR_IPPARACFG IpAccessCfg;
	memset(&IpAccessCfg,0,sizeof(IpAccessCfg));	
	DWORD  dwReturned;

	m_struDeviceInfo.bIPRet = NET_DVR_GetDVRConfig(m_struDeviceInfo.lLoginID,NET_DVR_GET_IPPARACFG,0,&IpAccessCfg,sizeof(NET_DVR_IPPARACFG),&dwReturned);

	int i;
    if(!m_struDeviceInfo.bIPRet)   //不支持ip接入,9000以下设备不支持禁用模拟通道
	{
		for(i=0; i<MAX_ANALOG_CHANNUM; i++)
		{
			if (i < m_struDeviceInfo.iDeviceChanNum)
			{
				sprintf(m_struDeviceInfo.struChanInfo[i].chChanName,"camera%d",i+m_struDeviceInfo.iStartChan);
				m_struDeviceInfo.struChanInfo[i].iChanIndex=i+m_struDeviceInfo.iStartChan;  //通道号
				m_struDeviceInfo.struChanInfo[i].bEnable = TRUE;
				
			}
			else
			{
				m_struDeviceInfo.struChanInfo[i].iChanIndex = -1;
				m_struDeviceInfo.struChanInfo[i].bEnable = FALSE;
				sprintf(m_struDeviceInfo.struChanInfo[i].chChanName, "");	
			}
		}
	}
	else        //支持IP接入，9000设备
	{
		for(i=0; i<MAX_ANALOG_CHANNUM; i++)  //模拟通道
		{
			if (i < m_struDeviceInfo.iDeviceChanNum)
			{
				sprintf(m_struDeviceInfo.struChanInfo[i].chChanName,"camera%d",i+m_struDeviceInfo.iStartChan);
				m_struDeviceInfo.struChanInfo[i].iChanIndex=i+m_struDeviceInfo.iStartChan;
				if(IpAccessCfg.byAnalogChanEnable[i])
				{
					m_struDeviceInfo.struChanInfo[i].bEnable = TRUE;
				}
				else
				{
					m_struDeviceInfo.struChanInfo[i].bEnable = FALSE;
				}
				
			}
			else//clear the state of other channel
			{
				m_struDeviceInfo.struChanInfo[i].iChanIndex = -1;
				m_struDeviceInfo.struChanInfo[i].bEnable = FALSE;
				sprintf(m_struDeviceInfo.struChanInfo[i].chChanName, "");	
			}		
		}

		//数字通道
		for(i=0; i<MAX_IP_CHANNEL; i++)
		{
			if(IpAccessCfg.struIPChanInfo[i].byEnable)  //ip通道在线
			{
				m_struDeviceInfo.struChanInfo[i+MAX_ANALOG_CHANNUM].bEnable = TRUE;
                m_struDeviceInfo.struChanInfo[i+MAX_ANALOG_CHANNUM].iChanIndex = IpAccessCfg.struIPChanInfo[i].byChannel;
				sprintf(m_struDeviceInfo.struChanInfo[i+MAX_ANALOG_CHANNUM].chChanName,"IP Camera %d",IpAccessCfg.struIPChanInfo[i].byChannel);

			}
			else
			{
               m_struDeviceInfo.struChanInfo[i+MAX_ANALOG_CHANNUM].bEnable = FALSE;
			    m_struDeviceInfo.struChanInfo[i+MAX_ANALOG_CHANNUM].iChanIndex = -1;
			}
		}		
	}

}

/*************************************************
函数名:    	StartPlay
函数描述:	开始一路播放
输入参数:   ChanIndex-通道号
输出参数:   			
返回值:		
**************************************************/
BOOL HiKConn::StartPlay(int iChanIndex)
{
	int m_iCurChanIndex = 0;
	NET_DVR_CLIENTINFO ClientInfo;
	ClientInfo.hPlayWnd     = pichwnd;
	ClientInfo.lChannel     = m_struDeviceInfo.struChanInfo[m_iCurChanIndex].iChanIndex;
	ClientInfo.lLinkMode    = 0;
    ClientInfo.sMultiCastIP = NULL;
	m_lPlayHandle = NET_DVR_RealPlay_V30(m_struDeviceInfo.lLoginID,&ClientInfo,g_RealDataCallBack_V30,NULL,FALSE);
	TRACE("*********%d",m_lPlayHandle);
	if(-1 == m_lPlayHandle)
	{
		return FALSE;
	}
}

/*************************************************
函数名:    	StopPlay
函数描述:	停止播放
输入参数:   
输出参数:   			
返回值:		
**************************************************/
void HiKConn::StopPlay()
{
	if(m_lPlayHandle != -1)
	{
		NET_DVR_StopRealPlay(m_lPlayHandle);
	}
}


