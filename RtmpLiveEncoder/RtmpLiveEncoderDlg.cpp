
// RtmpStreamingDlg.cpp : implementation file
//
#include "stdafx.h"
#include "RtmpLiveEncoderApp.h"
#include "RtmpLiveEncoderDlg.h"
#include "afxdialogex.h"
#include "FlvReader.h"
#include "LibRtmp.h"
#include "dshow/DSCapture.h"
#include <cxcore.h>
#include <cv.h>
#include <highgui.h>
#include "Thread.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// RtmpStreamingDlg dialog
RtmpLiveEncoderDlg::RtmpLiveEncoderDlg(CWnd* pParent /*=NULL*/)
	: CDialog(RtmpLiveEncoderDlg::IDD, pParent),m_bIsPlaying_1(FALSE),m_bIsPlaying_2(FALSE),m_bIsPlaying_3(FALSE),m_lPlayHandle(-1),m_bIsLogin(FALSE)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    WORD version;  
    WSADATA wsaData;  
    version = MAKEWORD(1, 1);
    WSAStartup(version, &wsaData);

    rtmp_live_encoder_1 = NULL;
	rtmp_live_encoder_2 = NULL;
	rtmp_live_encoder_3 = NULL;
}

RtmpLiveEncoderDlg::~RtmpLiveEncoderDlg()
{
    WSACleanup();

    CoUninitialize();
}

void RtmpLiveEncoderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CB_VIDEO, cb_select_video_);
    DDX_Control(pDX, IDC_CB_AUDIO, cb_select_audio_);
    DDX_Control(pDX, IDC_BTN_START, btn_start_);
    DDX_Control(pDX, IDC_BTN_STOP, btn_stop_);
    DDX_Control(pDX, IDC_EDIT_URL, edit_url_);
	DDX_Control(pDX, IDC_EDIT_URL2, edit_url_2_);
	DDX_Control(pDX, IDC_EDIT_URL3, edit_url_3_);
    DDX_Control(pDX, IDC_CHECK_LOG, check_log_);
    DDX_Control(pDX, IDC_STATIC_TITLE, edit_title_);
    DDX_Control(pDX, IDC_CB_WH, cb_wh_);
    DDX_Control(pDX, IDC_EDIT_BITRATE, edit_bitrate_);
    DDX_Control(pDX, IDC_EDIT_FPS, edit_fps_);
}

BEGIN_MESSAGE_MAP(RtmpLiveEncoderDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BTN_START, &RtmpLiveEncoderDlg::OnBnClickedBtnStart)
    ON_BN_CLICKED(IDC_BTN_STOP, &RtmpLiveEncoderDlg::OnBnClickedBtnStop)
    ON_CBN_SELCHANGE(IDC_CB_VIDEO, &RtmpLiveEncoderDlg::OnCbnSelchangeCbVideo)
	ON_BN_CLICKED(ID_LOGIN, &RtmpLiveEncoderDlg::OnBnClickedLogin)
	ON_BN_CLICKED(IDC_BUTTON_PLAY, &RtmpLiveEncoderDlg::OnBnClickedButtonPlay)
	ON_BN_CLICKED(ID_LOGIN2, &RtmpLiveEncoderDlg::OnBnClickedLogin2)
	ON_BN_CLICKED(IDC_BUTTON_PLAY2, &RtmpLiveEncoderDlg::OnBnClickedButtonPlay2)
	ON_BN_CLICKED(ID_LOGIN3, &RtmpLiveEncoderDlg::OnBnClickedLogin3)
	ON_BN_CLICKED(IDC_BUTTON_PLAY3, &RtmpLiveEncoderDlg::OnBnClickedButtonPlay3)
END_MESSAGE_MAP()


// RtmpStreamingDlg message handlers

BOOL RtmpLiveEncoderDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

    // 字体设置
    LOGFONT logFont;
    memset(&logFont,0,sizeof(LOGFONT));
    logFont.lfHeight = 20;
    logFont.lfWeight = FW_BOLD; 
    logFont.lfCharSet = GB2312_CHARSET;
    //strcpy(logFont.lfFaceName, _T("宋体"));
    label_font_.CreateFontIndirect(&logFont);

    SetWindowText(_T("RtmpLiveEncoder v1.1"));
    edit_title_.SetWindowText(_T("RTMP直播采集演示程序"));
    edit_title_.SetFont(&label_font_);

    // 音视频设备
    std::map<CString, CString> a_devices, v_devices;
    DSCapture::ListAudioCapDevices(a_devices);
    DSCapture::ListVideoCapDevices(v_devices);

    for (std::map<CString, CString>::iterator it = a_devices.begin();
        it != a_devices.end(); ++it)
    {
        cb_select_audio_.AddString(it->second);
        audio_device_index_.push_back(it->first);
    }
    cb_select_audio_.SetCurSel(0);

    for (std::map<CString, CString>::iterator it = v_devices.begin();
        it != v_devices.end(); ++it)
    {
        cb_select_video_.AddString(it->second);
        video_device_index_.push_back(it->first);
    }
    cb_select_video_.SetCurSel(0);
    OnCbnSelchangeCbVideo();

    // rtmp url
    edit_url_.SetWindowText(_T("rtmp://10.103.242.58/oflaDemo/rty"));
	edit_url_2_.SetWindowText(_T("rtmp://10.103.242.58/oflaDemo/test2"));
	edit_url_3_.SetWindowText(_T("rtmp://10.103.242.58/oflaDemo/test3"));
    edit_bitrate_.SetWindowText(_T("150"));
    edit_fps_.SetWindowText(_T("10"));
    check_log_.SetCheck(TRUE);

    btn_start_.EnableWindow(TRUE);
    btn_stop_.EnableWindow(FALSE);

	HWND hwnd = GetSafeHwnd();
	HWND pichwnd_1 = GetDlgItem(IDC_VIDEO_PRIVIEW)->m_hWnd;
	HWND pichwnd_2 = GetDlgItem(IDC_VIDEO_PRIVIEW2)->m_hWnd;
	HWND pichwnd_3 = GetDlgItem(IDC_VIDEO_PRIVIEW3)->m_hWnd;
	HiK_Live_1 = new HiKConn(hwnd,pichwnd_1,0,_T("10.103.241.170"),_T("admin"),_T("12345"),8000);
	HiK_Live_2 = new HiKConn(hwnd,pichwnd_2,1,_T("10.103.241.171"),_T("admin"),_T("12345"),8000);
	HiK_Live_3 = new HiKConn(hwnd,pichwnd_3,2,_T("10.103.241.172"),_T("admin"),_T("12345"),8000);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void RtmpLiveEncoderDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR RtmpLiveEncoderDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

static std::string CStringToString(const CString& mfcStr)
{
    CT2CA pszConvertedAnsiString(mfcStr);
    return (pszConvertedAnsiString);
}

void RtmpLiveEncoderDlg::OnBnClickedBtnStart()
{
#if 0
    {
        SYSTEMTIME time;
        GetLocalTime(&time);
        if (time.wMonth != 6)
        {
            exit(1);
        }
        if (time.wDay >= 24)
        {
            MessageBox(_T(""), _T("过期"));
            exit(1);
        }
    }
#endif
    CString audio_device_id, video_device_id;
    if (cb_select_audio_.GetCurSel() >= 0)
        audio_device_id = audio_device_index_[cb_select_audio_.GetCurSel()];

    int width = 0, height = 0, bitrate = 0, fps = 0;
    if (cb_select_video_.GetCurSel() >= 0)
    {
        video_device_id = video_device_index_[cb_select_video_.GetCurSel()];

        int video_i = cb_wh_.GetCurSel();
        width = width_index_[video_i];
        height = height_index_[video_i];
        CString tmp;
        edit_bitrate_.GetWindowText(tmp);
        bitrate = _ttoi(tmp);
        edit_fps_.GetWindowText(tmp);
        fps = _ttoi(tmp);
    }

    CWnd *pWnd1 = GetDlgItem(IDC_VIDEO_PRIVIEW),*pWnd2 = GetDlgItem(IDC_VIDEO_PRIVIEW2),*pWnd3 = GetDlgItem(IDC_VIDEO_PRIVIEW3);
    CRect rc1,rc2,rc3;
    pWnd1->GetClientRect(&rc1);
	pWnd2->GetClientRect(&rc2);
	pWnd3->GetClientRect(&rc3);

    CString rtmp_url;
    edit_url_.GetWindowText(rtmp_url);
    rtmp_live_encoder_1 = new RtmpLiveEncoder(audio_device_id, video_device_id, 
        width, height, bitrate, fps,
        (OAHWND)pWnd1->GetSafeHwnd(), rc1.right, rc1.bottom, CStringToString(rtmp_url),
        !!check_log_.GetCheck());

    rtmp_live_encoder_1->StartLive();

	CString rtmp_url_2;
    edit_url_2_.GetWindowText(rtmp_url_2);
	rtmp_live_encoder_2 = new RtmpLiveEncoder(audio_device_id, video_device_id, 
        width, height, bitrate, fps,
        (OAHWND)pWnd2->GetSafeHwnd(), rc2.right, rc2.bottom, CStringToString(rtmp_url_2),
        !!check_log_.GetCheck());

    rtmp_live_encoder_2->StartLive();

	CString rtmp_url_3;
    edit_url_3_.GetWindowText(rtmp_url_3);
	rtmp_live_encoder_3 = new RtmpLiveEncoder(audio_device_id, video_device_id, 
        width, height, bitrate, fps,
        (OAHWND)pWnd3->GetSafeHwnd(), rc3.right, rc3.bottom, CStringToString(rtmp_url_3),
        !!check_log_.GetCheck());

    rtmp_live_encoder_3->StartLive();

    btn_start_.EnableWindow(FALSE);
    btn_stop_.EnableWindow(TRUE);
}


void RtmpLiveEncoderDlg::OnBnClickedBtnStop()
{
    if (rtmp_live_encoder_1)
    {
        rtmp_live_encoder_1->StopLive();
        delete rtmp_live_encoder_1;
        rtmp_live_encoder_1 = NULL;
    }

	if (rtmp_live_encoder_2)
    {
        rtmp_live_encoder_2->StopLive();
        delete rtmp_live_encoder_2;
        rtmp_live_encoder_2 = NULL;
    }

	if (rtmp_live_encoder_3)
    {
        rtmp_live_encoder_3->StopLive();
        delete rtmp_live_encoder_3;
        rtmp_live_encoder_3 = NULL;
    }

    btn_start_.EnableWindow(TRUE);
    btn_stop_.EnableWindow(FALSE);
}


void RtmpLiveEncoderDlg::OnCbnSelchangeCbVideo()
{
    if (cb_select_video_.GetCurSel() >= 0)
    {
        int video_i = cb_select_video_.GetCurSel();
        CString device_id = video_device_index_[video_i];
        width_index_.clear();
        height_index_.clear();
        cb_wh_.ResetContent();

        int set_sel = 0;
        DSCapture::ListVideoCapDeviceWH(device_id, width_index_, height_index_);
        for (int i = 0; i < width_index_.size(); ++i)
        {
            CString tmp;
            tmp.Format(_T("%d X %d"), width_index_[i], height_index_[i]);
            cb_wh_.AddString(tmp);
            if (width_index_[i] == 320)
                set_sel = i;
        }
        cb_wh_.SetCurSel(set_sel);
    }
}


void RtmpLiveEncoderDlg::OnBnClickedLogin()
{
	if(!m_bIsLogin)
	{
		if(!(HiK_Live_1) -> DoLogin())
			return;
		HiK_Live_1 -> DoGetDeviceResoureCfg();  //获取设备资源信息	
		m_bIsLogin = TRUE;
	}
}

void RtmpLiveEncoderDlg::OnBnClickedButtonPlay()
{
	int m_iCurChanIndex = 2;
	if(m_iCurChanIndex == -1)
	{
		MessageBox(_T("选择一个通道"));
		return;
	}

	if(!(HiK_Live_1) -> StartHikCamera())
	{
		MessageBox(_T("连接错误"));
		return ;
	}

	if(!m_bIsPlaying_1)
	{
        HiK_Live_1 -> StartPlay(m_iCurChanIndex);
		m_bIsPlaying_1 = TRUE;
		GetDlgItem(IDC_BUTTON_PLAY)->SetWindowText(_T("stop1"));
	}
	else
	{
		HiK_Live_1 -> StopPlay();
		m_bIsPlaying_1 = FALSE;
		GetDlgItem(IDC_BUTTON_PLAY)->SetWindowText(_T("play1"));
		GetDlgItem(IDC_VIDEO_PRIVIEW)->Invalidate();
	}
	//初始化存储队列以及semaphore等
	int index = 0;
	init(index);
}

void RtmpLiveEncoderDlg::OnBnClickedLogin2()
{
	if(!m_bIsLogin)
	{
		if(!(HiK_Live_2) -> DoLogin())
			return;
		HiK_Live_2 -> DoGetDeviceResoureCfg();  //获取设备资源信息	
		m_bIsLogin = TRUE;
	}
}


void RtmpLiveEncoderDlg::OnBnClickedButtonPlay2()
{
	int m_iCurChanIndex = 2;
	if(m_iCurChanIndex == -1)
	{
		MessageBox(_T("选择一个通道"));
		return;
	}

	if(!(HiK_Live_2) -> StartHikCamera())
	{
		MessageBox(_T("连接错误"));
		return ;
	}

	if(!m_bIsPlaying_2)
	{
        HiK_Live_2 -> StartPlay(m_iCurChanIndex);
		m_bIsPlaying_2 = TRUE;
		GetDlgItem(IDC_BUTTON_PLAY2)->SetWindowText(_T("stop2"));
	}
	else
	{
		HiK_Live_2 -> StopPlay();
		m_bIsPlaying_2 = FALSE;
		GetDlgItem(IDC_BUTTON_PLAY2)->SetWindowText(_T("play2"));
		GetDlgItem(IDC_VIDEO_PRIVIEW2)->Invalidate();
	}
	//初始化存储队列以及semaphore等
	int index = 1;
	init(index);
}


void RtmpLiveEncoderDlg::OnBnClickedLogin3()
{
	if(!m_bIsLogin)
	{
		if(!(HiK_Live_3) -> DoLogin())
			return;
		HiK_Live_3 -> DoGetDeviceResoureCfg();  //获取设备资源信息	
		m_bIsLogin = TRUE;
	}
}


void RtmpLiveEncoderDlg::OnBnClickedButtonPlay3()
{
	int m_iCurChanIndex = 2;
	if(m_iCurChanIndex == -1)
	{
		MessageBox(_T("选择一个通道"));
		return;
	}

	if(!(HiK_Live_3) -> StartHikCamera())
	{
		MessageBox(_T("连接错误"));
		return ;
	}

	if(!m_bIsPlaying_3)
	{
         HiK_Live_3 -> StartPlay(m_iCurChanIndex);
		 m_bIsPlaying_3 = TRUE;
		 GetDlgItem(IDC_BUTTON_PLAY3)->SetWindowText(_T("stop3"));
	}
	else
	{
		HiK_Live_3 -> StopPlay();
		m_bIsPlaying_3 = FALSE;
		GetDlgItem(IDC_BUTTON_PLAY3)->SetWindowText(_T("play3"));
		GetDlgItem(IDC_VIDEO_PRIVIEW3)->Invalidate();
	}
	//初始化存储队列以及semaphore等
	int index = 2;
	init(index);
}

