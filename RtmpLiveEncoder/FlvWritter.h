#ifndef _FLV_WRITTER_H_
#define _FLV_WRITTER_H_

#include <stdio.h>

class FlvWritter
{
public:
    FlvWritter();

    ~FlvWritter();

    void Open(const char* filename);

    void Close();

    void WriteAACSequenceHeaderTag(int sampleRate, int channel);

    void WriteAVCSequenceHeaderTag(
        const char* spsBuf, int spsSize,
        const char* ppsBuf, int ppsSize);

    void WriteAACDataTag(const char* dataBuf, 
        int dataBufLen, int timestamp);

    void WriteAVCDataTag(const char* dataBuf, 
        int dataBufLen, int timestamp, int isKeyframe);

    void WriteAudioTag(char* buf, 
        int bufLen, int timestamp);

    void WriteVideoTag(char* buf, 
        int bufLen, int timestamp);

private:
    FILE* file_handle_;
    __int64 time_begin_;
};

#endif // _FLV_WRITTER_H_
