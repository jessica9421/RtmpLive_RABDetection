#ifndef _VIDEO_ENCODER_THREAD_H_
#define _VIDEO_ENCODER_THREAD_H_

#include <string>

#include "base/SimpleThread.h"

class DSCaptureListener;
class X264Encoder;
class DSVideoGraph;
class VideoEncoderThread : public base::SimpleThread
{
public:
    VideoEncoderThread(DSVideoGraph* dsVideoGraph, int bitrate);

    ~VideoEncoderThread();

    virtual void Run();

    void SetListener(DSCaptureListener* listener) { listener_ = listener; }

    void SetOutputFilename(const std::string& filename);

    void SavePicture() { is_save_picture_ = true; }

private:
    bool RGB2YUV420(LPBYTE RgbBuf,UINT nWidth,UINT nHeight,LPBYTE yuvBuf,unsigned long *len);

    bool RGB2YUV420_r(LPBYTE RgbBuf,UINT nWidth,UINT nHeight,LPBYTE yuvBuf,unsigned long *len);

    void SaveRgb2Bmp(char* rgbbuf, unsigned int width, unsigned int height);

private:
    DSVideoGraph* ds_video_graph_;
    X264Encoder* x264_encoder_;
    std::string filename_264_;
    bool is_save_picture_;
    DSCaptureListener* listener_;
};

#endif // _VIDEO_ENCODER_THREAD_H_
