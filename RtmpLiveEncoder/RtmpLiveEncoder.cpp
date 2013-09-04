#include "stdafx.h"
#include "RtmpLiveEncoder.h"

#include <vector>

#include "dshow/DSVideoGraph.h"
#include "AmfByteStream.h"
#include "BitWritter.h"
#include "LibRtmp.h"

RtmpLiveEncoder::RtmpLiveEncoder(const CString& audioDeviceID, const CString& videoDeviceID,
    int width, int height, int bitrate, int fps,
    OAHWND owner, int previewWidth, int previewHeight,
    const std::string& rtmpUrl, bool isNeedLog)
    : wait_event_(false, false)
{
    ds_capture_ = new DSCapture();

    source_samrate_ = 44100;
    source_channel_ = 1;

    DSAudioFormat audio_fmt;
    audio_fmt.samples_per_sec = source_samrate_;
    audio_fmt.channels = source_channel_;
    audio_fmt.bits_per_sample = 16;

    DSVideoFormat video_fmt;
    video_fmt.width = width;
    video_fmt.height = height;
    video_fmt.bitrate = bitrate;
    video_fmt.fps = fps;

    ds_capture_->Create(audioDeviceID, videoDeviceID, audio_fmt, video_fmt, _T(""), _T(""));
    ds_capture_->AdjustVideoWindow(owner, previewWidth, previewHeight);
    ds_capture_->SetListener(this);

    width_ = ds_capture_->GetVideoGraph()->Width();
    height_ = ds_capture_->GetVideoGraph()->Height();

    sps_ = NULL;
    sps_size_ = 0;
    pps_ = NULL;
    pps_size_ = 0;

    audio_mem_buf_size_ = 0;
    audio_mem_buf_ = NULL;
    video_mem_buf_size_ = width*height*3;
    video_mem_buf_ = new char[video_mem_buf_size_];

    rtmp_url_ = rtmpUrl;
    librtmp_ = new LibRtmp(isNeedLog, true);

    flv_writter_ = new FlvWritter();
    flv_writter_->Open("rty.flv");
    has_flv_writter_header_ = false;

    is_enable_audio_ = audioDeviceID.IsEmpty() ? false : true;
    is_enable_video_ = videoDeviceID.IsEmpty() ? false : true;
}

RtmpLiveEncoder::~RtmpLiveEncoder()
{
    if (ds_capture_)
        delete ds_capture_;

    if (librtmp_)
        delete librtmp_;

    if (sps_)
        free(sps_);
    if (pps_)
        free(pps_);

    if (audio_mem_buf_)
        delete[] audio_mem_buf_;
    if (video_mem_buf_)
        delete[] video_mem_buf_;
}

void RtmpLiveEncoder::StartLive()
{
    base::SimpleThread::Start();
}

void RtmpLiveEncoder::StopLive()
{
    if (SimpleThread::IsStop()) return;

    SimpleThread::Stop();
    wait_event_.Signal();
    SimpleThread::Join();
}

void RtmpLiveEncoder::Run()
{
    // 连接rtmp server，完成握手等协议
    librtmp_->Open(rtmp_url_.c_str());

    // 发送metadata包
    //SendMetadataPacket();

    // 开始捕获音视频
    if (is_enable_audio_) ds_capture_->StartAudio();
    if (is_enable_video_) ds_capture_->StartVideo();

    if (false == is_enable_video_)
    {
        SendAACSequenceHeaderPacket();
        gVideoBegin = true;

        time_begin_ = ::GetTickCount();
        last_timestamp_ = time_begin_;

        if (flv_writter_)
        {
            flv_writter_->WriteAACSequenceHeaderTag(source_samrate_, source_channel_);
            has_flv_writter_header_ = true;
        }
    }

    while (true)
    {
        wait_event_.Wait();

        if (SimpleThread::IsStop()) break;

        // 从队列中取出音频或视频数据
        RtmpDataBuffer rtmp_data;
        {
            base::AutoLock alock(queue_lock_);

            if (false == process_buf_queue_.empty())
            {
                rtmp_data = process_buf_queue_.front();
                process_buf_queue_.pop_front();
            }
        }

        if (rtmp_data.data != NULL)
        {
            if (rtmp_data.type == FLV_TAG_TYPE_AUDIO)
            {
                SendAudioDataPacket(rtmp_data.data, rtmp_data.timestamp);
            }
            else
            {
                SendVideoDataPacket(rtmp_data.data, rtmp_data.timestamp, rtmp_data.is_keyframe);
            }
        }
    }

    if (is_enable_video_) ds_capture_->StopVideo();
    if (is_enable_audio_) ds_capture_->StopAudio();

    librtmp_->Close();

    if (flv_writter_)
    {
        flv_writter_->Close();
        delete flv_writter_;
        flv_writter_ = NULL;
        has_flv_writter_header_ = false;
    }
}

void RtmpLiveEncoder::OnCaptureAudioBuffer(base::DataBuffer* dataBuf, unsigned int timestamp)
{
    if (SimpleThread::IsStop() || false == is_enable_audio_) return;

    {
        base::AutoLock alock(queue_lock_);
        process_buf_queue_.push_back(RtmpDataBuffer(FLV_TAG_TYPE_AUDIO, 
            dataBuf->Clone(), GetTimestamp(), false));
    }
    wait_event_.Signal();

    delete dataBuf;
}

void RtmpLiveEncoder::OnCaptureVideoBuffer(base::DataBuffer* dataBuf, unsigned int timestamp, bool isKeyframe)
{
    if (SimpleThread::IsStop() || false == is_enable_video_) return;

    {
        base::AutoLock alock(queue_lock_);
        process_buf_queue_.push_back(RtmpDataBuffer(FLV_TAG_TYPE_VIDEO,
            dataBuf->Clone(), GetTimestamp(), isKeyframe));
    }
    wait_event_.Signal();

    delete dataBuf;
}

void RtmpLiveEncoder::SendVideoDataPacket(base::DataBuffer* dataBuf, unsigned int timestamp, bool isKeyframe)
{
    int need_buf_size = dataBuf->BufLen() + 5;
    if (need_buf_size > video_mem_buf_size_)
    {
        if (video_mem_buf_)
            delete[] video_mem_buf_;
        video_mem_buf_ = new char[need_buf_size];
        video_mem_buf_size_ = need_buf_size;
    }

    char* buf = video_mem_buf_;
    char* pbuf = buf;

    unsigned char flag = 0;
    if (isKeyframe)
        flag = 0x17;
    else
        flag = 0x27;

    pbuf = UI08ToBytes(pbuf, flag);

    pbuf = UI08ToBytes(pbuf, 1);    // avc packet type (0, nalu)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    memcpy(pbuf, dataBuf->Buf(), dataBuf->BufLen());
    pbuf += dataBuf->BufLen();

    librtmp_->Send(buf, (int)(pbuf - buf), FLV_TAG_TYPE_VIDEO, timestamp);

    if (flv_writter_ && has_flv_writter_header_)
    {
        flv_writter_->WriteVideoTag(buf, (int)(pbuf - buf), timestamp);
    }

    delete dataBuf;
}

void RtmpLiveEncoder::SendAudioDataPacket(base::DataBuffer* dataBuf, unsigned int timestamp)
{
    int need_buf_size = dataBuf->BufLen() + 5;
    if (need_buf_size > audio_mem_buf_size_)
    {
        if (audio_mem_buf_)
            delete[] audio_mem_buf_;
        audio_mem_buf_ = new char[need_buf_size];
        audio_mem_buf_size_ = need_buf_size;
    }

    char* buf = audio_mem_buf_;
    char* pbuf = buf;

    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);

    pbuf = UI08ToBytes(pbuf, 1);    // aac packet type (1, raw)

    memcpy(pbuf, dataBuf->Buf(), dataBuf->BufLen());
    pbuf += dataBuf->BufLen();

    librtmp_->Send(buf, (int)(pbuf - buf), FLV_TAG_TYPE_AUDIO, timestamp);

    if (flv_writter_ && has_flv_writter_header_)
    {
        flv_writter_->WriteAudioTag(buf, (int)(pbuf - buf), timestamp);
    }

    delete dataBuf;
}

void RtmpLiveEncoder::PostBuffer(base::DataBuffer* dataBuf)
{

}

void RtmpLiveEncoder::SendMetadataPacket()
{
    char metadata_buf[4096];
    char* pbuf = WriteMetadata(metadata_buf);

    librtmp_->Send(metadata_buf, (int)(pbuf - metadata_buf), FLV_TAG_TYPE_META, 0);
}

char* RtmpLiveEncoder::WriteMetadata(char* buf)
{
    char* pbuf = buf;

    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "@setDataFrame");

    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "onMetaData");

//     pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_MIXEDARRAY);
//     pbuf = UI32ToBytes(pbuf, 2);
    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_OBJECT);

    pbuf = AmfStringToBytes(pbuf, "width");
    pbuf = AmfDoubleToBytes(pbuf, width_);

    pbuf = AmfStringToBytes(pbuf, "height");
    pbuf = AmfDoubleToBytes(pbuf, height_);

    pbuf = AmfStringToBytes(pbuf, "framerate");
    pbuf = AmfDoubleToBytes(pbuf, 10);

    pbuf = AmfStringToBytes(pbuf, "videocodecid");
    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_STRING);
    pbuf = AmfStringToBytes(pbuf, "avc1");

    // 0x00 0x00 0x09
    pbuf = AmfStringToBytes(pbuf, "");
    pbuf = UI08ToBytes(pbuf, AMF_DATA_TYPE_OBJECT_END);
    
    return pbuf;
}

void RtmpLiveEncoder::SendAVCSequenceHeaderPacket()
{
    char avc_seq_buf[4096];
    char* pbuf = WriteAVCSequenceHeader(avc_seq_buf);

    librtmp_->Send(avc_seq_buf, (int)(pbuf - avc_seq_buf), FLV_TAG_TYPE_VIDEO, 0);
}

char* RtmpLiveEncoder::WriteAVCSequenceHeader(char* buf)
{
    char* pbuf = buf;

    unsigned char flag = 0;
    flag = (1 << 4) |   // frametype "1  == keyframe"
        7;              // codecid   "7  == AVC"

    pbuf = UI08ToBytes(pbuf, flag);

    pbuf = UI08ToBytes(pbuf, 0);    // avc packet type (0, header)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    // AVCDecoderConfigurationRecord

    pbuf = UI08ToBytes(pbuf, 1);            // configurationVersion
    pbuf = UI08ToBytes(pbuf, sps_[1]);      // AVCProfileIndication
    pbuf = UI08ToBytes(pbuf, sps_[2]);      // profile_compatibility
    pbuf = UI08ToBytes(pbuf, sps_[3]);      // AVCLevelIndication
    pbuf = UI08ToBytes(pbuf, 0xff);         // 6 bits reserved (111111) + 2 bits nal size length - 1
    pbuf = UI08ToBytes(pbuf, 0xe1);         // 3 bits reserved (111) + 5 bits number of sps (00001)
    pbuf = UI16ToBytes(pbuf, sps_size_);    // sps
    memcpy(pbuf, sps_, sps_size_);
    pbuf += sps_size_;
    pbuf = UI08ToBytes(pbuf, 1);            // number of pps
    pbuf = UI16ToBytes(pbuf, pps_size_);    // pps
    memcpy(pbuf, pps_, pps_size_);
    pbuf += pps_size_;

    return pbuf;
}

void RtmpLiveEncoder::SendAACSequenceHeaderPacket()
{
    char aac_seq_buf[4096];
    char* pbuf = WriteAACSequenceHeader(aac_seq_buf);

    librtmp_->Send(aac_seq_buf, (int)(pbuf - aac_seq_buf), FLV_TAG_TYPE_AUDIO, 0);
}

char* RtmpLiveEncoder::WriteAACSequenceHeader(char* buf)
{
    char* pbuf = buf;

    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);

    pbuf = UI08ToBytes(pbuf, 0);    // aac packet type (0, header)

    // AudioSpecificConfig

    int sample_rate_index;
    if (source_samrate_ == 48000)
        sample_rate_index = 0x03;
    else if (source_samrate_ == 44100)
        sample_rate_index = 0x04;
    else if (source_samrate_ == 32000)
        sample_rate_index = 0x05;
    else if (source_samrate_ == 24000)
        sample_rate_index = 0x06;
    else if (source_samrate_ == 22050)
        sample_rate_index = 0x07;
    else if (source_samrate_ == 16000)
        sample_rate_index = 0x08;
    else if (source_samrate_ == 12000)
        sample_rate_index = 0x09;
    else if (source_samrate_ == 11025)
        sample_rate_index = 0x0a;
    else if (source_samrate_ == 8000)
        sample_rate_index = 0x0b;

    PutBitContext pb;
    init_put_bits(&pb, pbuf, 1024);
    put_bits(&pb, 5, 2);    //object type - AAC-LC
    put_bits(&pb, 4, sample_rate_index); // sample rate index
    put_bits(&pb, 4, source_channel_);    // channel configuration
    //GASpecificConfig
    put_bits(&pb, 1, 0);    // frame length - 1024 samples
    put_bits(&pb, 1, 0);    // does not depend on core coder
    put_bits(&pb, 1, 0);    // is not extension

    flush_put_bits(&pb);

    pbuf += 2;

    return pbuf;
}

// 当收到sps和pps信息时，发送AVC和AAC的sequence header
void RtmpLiveEncoder::OnSPSAndPPS(char* spsBuf, int spsSize, char* ppsBuf, int ppsSize)
{
    sps_ = (char*)malloc(spsSize);
    memcpy(sps_, spsBuf, spsSize);
    sps_size_ = spsSize;

    pps_ = (char*)malloc(ppsSize);
    memcpy(pps_, ppsBuf, ppsSize);
    pps_size_ = ppsSize;

    if (is_enable_video_)
        SendAVCSequenceHeaderPacket();
    if (is_enable_audio_)
        SendAACSequenceHeaderPacket();

    time_begin_ = ::GetTickCount();
    last_timestamp_ = time_begin_;

    gVideoBegin = true;

    if (flv_writter_)
    {
        if (is_enable_audio_)
            flv_writter_->WriteAACSequenceHeaderTag(source_samrate_, source_channel_);
        if (is_enable_video_)
            flv_writter_->WriteAVCSequenceHeaderTag(sps_, sps_size_, pps_, pps_size_);

        has_flv_writter_header_ = true;
    }
}

unsigned int RtmpLiveEncoder::GetTimestamp()
{
    unsigned int timestamp;
    __int64 now = ::GetTickCount();

    //if (now < last_timestamp_)
    if (now < time_begin_)
    {
        timestamp = 0;
        last_timestamp_ = now;
        time_begin_ = now;
    }
    else
    {
        timestamp = now - time_begin_;
        //timestamp = now - last_timestamp_;
    }
    return timestamp;
}
