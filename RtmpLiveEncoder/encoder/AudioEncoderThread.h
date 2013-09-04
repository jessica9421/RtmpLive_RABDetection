#ifndef _AUDIO_ENCODER_THREAD_H_
#define _AUDIO_ENCODER_THREAD_H_

#include <string>

#include "base/SimpleThread.h"

class DSCaptureListener;
class FAACEncoder;
class DSAudioGraph;
class AudioEncoderThread : public base::SimpleThread
{
public:
    AudioEncoderThread(DSAudioGraph* dsAudioGraph);

    ~AudioEncoderThread();

    virtual void Run();

    void SetListener(DSCaptureListener* listener) { listener_ = listener; }

    void SetOutputFilename(const std::string& filename);

private:
    DSAudioGraph* ds_audio_graph_;
    FAACEncoder* faac_encoder_;
    std::string filename_aac_;
    DSCaptureListener* listener_;
};


#endif // _AUDIO_ENCODER_THREAD_H_
