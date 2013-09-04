#ifndef _RTMP_LIVE_ENCODER_H_
#define _RTMP_LIVE_ENCODER_H_

#include <deque>
#include <string>

#include "base/DataBuffer.h"
#include "base/Lock.h"
#include "base/SimpleThread.h"
#include "base/WaitEvent.h"

#include "dshow/DSCapture.h"
#include "FlvWritter.h"

struct RtmpDataBuffer
{
    int type;
    base::DataBuffer* data;
    unsigned int timestamp;
    bool is_keyframe;

    RtmpDataBuffer(int type_i, base::DataBuffer* data_i, unsigned int ts_i, bool iskey_i)
    {
        type = type_i;
        data = data_i;
        timestamp = ts_i;
        is_keyframe = iskey_i;
    }

    RtmpDataBuffer()
    {
        type = -1;
        data = NULL;
        timestamp = 0;
        is_keyframe = false;
    }
};

class LibRtmp;
class RtmpLiveEncoder
    : public base::SimpleThread
    , public DSCaptureListener
{
public:
    RtmpLiveEncoder(const CString& audioDeviceID, const CString& videoDeviceID,
        int width, int height, int bitrate, int fps,
        OAHWND owner, int previewWidth, int previewHeight,
        const std::string& rtmpUrl, bool isNeedLog);

    ~RtmpLiveEncoder();

    void StartLive();

    void StopLive();

    virtual void Run();

    virtual void OnCaptureAudioBuffer(base::DataBuffer* dataBuf, unsigned int timestamp);

    virtual void OnCaptureVideoBuffer(base::DataBuffer* dataBuf, unsigned int timestamp, bool isKeyframe);

    void PostBuffer(base::DataBuffer* dataBuf);

    virtual void OnSPSAndPPS(char* spsBuf, int spsSize, char* ppsBuf, int ppsSize);

private:
    void SendVideoDataPacket(base::DataBuffer* dataBuf, unsigned int timestamp, bool isKeyframe);

    void SendAudioDataPacket(base::DataBuffer* dataBuf, unsigned int timestamp);

    void SendMetadataPacket();

    void SendAVCSequenceHeaderPacket();

    void SendAACSequenceHeaderPacket();

    char* WriteMetadata(char* buf);

    char* WriteAVCSequenceHeader(char* buf);

    char* WriteAACSequenceHeader(char* buf);

    unsigned int GetTimestamp();

private:
    DSCapture* ds_capture_;
    LibRtmp* librtmp_;
    std::string rtmp_url_;

    FlvWritter* flv_writter_;
    bool has_flv_writter_header_;

    int source_samrate_;
    int source_channel_;

    // metadata
    int width_;
    int height_;

    char* sps_;        // sequence parameter set
    int sps_size_;
    char* pps_;        // picture parameter set
    int pps_size_;

    char* audio_mem_buf_;
    int audio_mem_buf_size_;
    char* video_mem_buf_;
    int video_mem_buf_size_;

    __int64 time_begin_;
    __int64 last_timestamp_;

    bool is_enable_audio_;
    bool is_enable_video_;

    std::deque<RtmpDataBuffer> process_buf_queue_;
    base::Lock queue_lock_;
    base::WaitableEvent wait_event_;
};

#endif // _RTMP_LIVE_ENCODER_H_
