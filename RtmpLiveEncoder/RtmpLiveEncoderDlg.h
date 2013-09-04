#pragma once

#include <vector>
#include "RtmpLiveEncoderApp.h"
#include "HKConnect/GeneralDef.h"
#include "RtmpLiveEncoder.h"
#include "HiKConn.h"
#include "Encoder/VideoEncoderThread.h"

class RtmpLiveEncoderDlg : public CDialog
{
public:
	RtmpLiveEncoderDlg(CWnd* pParent = NULL);

    ~RtmpLiveEncoderDlg();

public:
	LOCAL_DEVICE_INFO m_struDeviceInfo;
	BOOL m_bIsPlaying_1;
	BOOL m_bIsPlaying_2;
	BOOL m_bIsPlaying_3;
	BOOL m_bIsLogin;
	BOOL DoLogin();
	LONG m_lPlayHandle;
	LONG m_lDecID;
	NET_DVR_START_PIC_VIEW_INFO m_struPicPreviewInfo;
	void StopPlay();
	void StartPlay(int iChanIndex);

	void DoGetDeviceResoureCfg();
	enum { IDD = IDD_RTMPSTREAMING_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

private:
    CComboBox cb_select_video_;
    CComboBox cb_select_audio_;
    CButton btn_start_;
    CButton btn_stop_;
    CEdit edit_url_;
	CEdit edit_url_2_;
	CEdit edit_url_3_;
    CButton check_log_;

    CFont label_font_;
    CEdit edit_title_;
    CComboBox cb_wh_;
    CEdit edit_bitrate_;
    CEdit edit_fps_;

private:
    std::vector<CString> audio_device_index_;
    std::vector<CString> video_device_index_;

    std::vector<int> width_index_;
    std::vector<int> height_index_;

    RtmpLiveEncoder* rtmp_live_encoder_1;
	RtmpLiveEncoder* rtmp_live_encoder_2;
	RtmpLiveEncoder* rtmp_live_encoder_3;
	HiKConn* HiK_Live_1;
	HiKConn* HiK_Live_2;
	HiKConn* HiK_Live_3;
public:
    afx_msg void OnBnClickedBtnStart();
    afx_msg void OnBnClickedBtnStop();
    afx_msg void OnCbnSelchangeCbVideo();
	afx_msg void OnBnClickedLogin();
	afx_msg void OnBnClickedButtonPlay();
	afx_msg void OnBnClickedButtonDetect();
	afx_msg void OnBnClickedLogin2();
	afx_msg void OnBnClickedButtonPlay2();
	afx_msg void OnBnClickedLogin3();
	afx_msg void OnBnClickedButtonPlay3();
};
