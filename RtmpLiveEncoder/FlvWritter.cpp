#include "stdafx.h"
#include "FlvWritter.h"

#include <string.h>
#include <stdlib.h>

#include "AmfByteStream.h"
#include "BitWritter.h"

typedef struct 
{
    unsigned char type;
    unsigned char datasize[3];
    unsigned char timestamp[3];
    unsigned char timestamp_ex;
    unsigned char streamid[3];
} FlvTag;

const unsigned char kFlvFileHeaderData[] = "FLV\x1\x5\0\0\0\x9\0\0\0\0";

FlvWritter::FlvWritter()
{
    file_handle_ = NULL;
    time_begin_ = -1;
}

FlvWritter::~FlvWritter()
{
    Close();
}

void FlvWritter::Open(const char* filename)
{
    file_handle_ = fopen(filename, "wb");

    // flv ÎÄ¼þÍ·
    fwrite(kFlvFileHeaderData, 13, 1, file_handle_);
}

void FlvWritter::Close()
{
    if (file_handle_)
    {
        fclose(file_handle_);
        file_handle_ = NULL;
    }
}

void FlvWritter::WriteAACSequenceHeaderTag(
    int sampleRate, int channel)
{
    char aac_seq_buf[4096];
    char* pbuf = aac_seq_buf;
    PutBitContext pb;
    int sample_rate_index = 0x04;

    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);

    pbuf = UI08ToBytes(pbuf, 0);    // aac packet type (0, header)

    // AudioSpecificConfig

    if (sampleRate == 48000)        sample_rate_index = 0x03;
    else if (sampleRate == 44100)   sample_rate_index = 0x04;
    else if (sampleRate == 32000)   sample_rate_index = 0x05;
    else if (sampleRate == 24000)   sample_rate_index = 0x06;
    else if (sampleRate == 22050)   sample_rate_index = 0x07;
    else if (sampleRate == 16000)   sample_rate_index = 0x08;
    else if (sampleRate == 12000)   sample_rate_index = 0x09;
    else if (sampleRate == 11025)   sample_rate_index = 0x0a;
    else if (sampleRate == 8000)    sample_rate_index = 0x0b;

    init_put_bits(&pb, pbuf, 1024);
    put_bits(&pb, 5, 2);    //object type - AAC-LC
    put_bits(&pb, 4, sample_rate_index); // sample rate index
    put_bits(&pb, 4, channel);    // channel configuration
    //GASpecificConfig
    put_bits(&pb, 1, 0);    // frame length - 1024 samples
    put_bits(&pb, 1, 0);    // does not depend on core coder
    put_bits(&pb, 1, 0);    // is not extension

    flush_put_bits(&pb);

    pbuf += 2;

    WriteAudioTag(aac_seq_buf, (int)(pbuf - aac_seq_buf), 0);
}

void FlvWritter::WriteAVCSequenceHeaderTag(
    const char* spsBuf, int spsSize,
    const char* ppsBuf, int ppsSize)
{
    char avc_seq_buf[4096];
    char* pbuf = avc_seq_buf;

    unsigned char flag = 0;
    flag = (1 << 4) |   // frametype "1  == keyframe"
        7;              // codecid   "7  == AVC"

    pbuf = UI08ToBytes(pbuf, flag);

    pbuf = UI08ToBytes(pbuf, 0);    // avc packet type (0, header)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    // AVCDecoderConfigurationRecord

    pbuf = UI08ToBytes(pbuf, 1);            // configurationVersion
    pbuf = UI08ToBytes(pbuf, spsBuf[1]);      // AVCProfileIndication
    pbuf = UI08ToBytes(pbuf, spsBuf[2]);      // profile_compatibility
    pbuf = UI08ToBytes(pbuf, spsBuf[3]);      // AVCLevelIndication
    pbuf = UI08ToBytes(pbuf, 0xff);         // 6 bits reserved (111111) + 2 bits nal size length - 1
    pbuf = UI08ToBytes(pbuf, 0xe1);         // 3 bits reserved (111) + 5 bits number of sps (00001)
    pbuf = UI16ToBytes(pbuf, (unsigned short)spsSize);    // sps
    memcpy(pbuf, spsBuf, spsSize);
    pbuf += spsSize;
    pbuf = UI08ToBytes(pbuf, 1);            // number of pps
    pbuf = UI16ToBytes(pbuf, (unsigned short)ppsSize);    // pps
    memcpy(pbuf, ppsBuf, ppsSize);
    pbuf += ppsSize;

    WriteVideoTag(avc_seq_buf, (int)(pbuf - avc_seq_buf), 0);
}

void FlvWritter::WriteAACDataTag(const char* dataBuf, 
    int dataBufLen, int timestamp)
{
    char* buf = (char*)malloc(dataBufLen+5);
    char* pbuf = buf;

    unsigned char flag = 0;
    flag = (10 << 4) |  // soundformat "10 == AAC"
        (3 << 2) |      // soundrate   "3  == 44-kHZ"
        (1 << 1) |      // soundsize   "1  == 16bit"
        1;              // soundtype   "1  == Stereo"

    pbuf = UI08ToBytes(pbuf, flag);

    pbuf = UI08ToBytes(pbuf, 1);    // aac packet type (1, raw)

    memcpy(pbuf, dataBuf, dataBufLen);
    pbuf += dataBufLen;

    if (time_begin_ == -1) time_begin_ = timestamp;

    if (timestamp < time_begin_)
    {
        time_begin_ = 0;
    }
    WriteAudioTag(buf, (int)(pbuf - buf), timestamp-time_begin_);

    free(buf);
}

void FlvWritter::WriteAVCDataTag(const char* dataBuf, 
    int dataBufLen, int timestamp, int isKeyframe)
{
    char* buf = (char*)malloc(dataBufLen+5);
    char* pbuf = buf;

    unsigned char flag = 0;
    if (isKeyframe)
        flag = 0x17;
    else
        flag = 0x27;

    pbuf = UI08ToBytes(pbuf, flag);

    pbuf = UI08ToBytes(pbuf, 1);    // avc packet type (0, nalu)
    pbuf = UI24ToBytes(pbuf, 0);    // composition time

    memcpy(pbuf, dataBuf, dataBufLen);
    pbuf += dataBufLen;

    if (time_begin_ == -1) time_begin_ = timestamp;

    if (timestamp < time_begin_)
    {
        time_begin_ = 0;
    }

    WriteVideoTag(buf, (int)(pbuf - buf), timestamp-time_begin_);

    free(buf);
}

void FlvWritter::WriteAudioTag(char* buf, 
    int bufLen, int timestamp)
{
    char prev_size[4];

    FlvTag flvtag;
    memset(&flvtag, 0, sizeof(FlvTag));

    flvtag.type = FLV_TAG_TYPE_AUDIO;
    UI24ToBytes((char*)flvtag.datasize, bufLen);
    flvtag.timestamp_ex = (timestamp >> 24) & 0xff;
    flvtag.timestamp[0] = (timestamp >> 16) & 0xff;
    flvtag.timestamp[1] = (timestamp >> 8) & 0xff;
    flvtag.timestamp[2] = (timestamp) & 0xff;

    fwrite(&flvtag, sizeof(FlvTag), 1, file_handle_);
    fwrite(buf, 1, bufLen, file_handle_);

    UI32ToBytes(prev_size, bufLen+sizeof(FlvTag));
    fwrite(prev_size, 4, 1, file_handle_);
}

void FlvWritter::WriteVideoTag(char* buf, 
    int bufLen, int timestamp)
{
    char prev_size[4];

    FlvTag flvtag;
    memset(&flvtag, 0, sizeof(FlvTag));

    flvtag.type = FLV_TAG_TYPE_VIDEO;
    UI24ToBytes((char*)flvtag.datasize, bufLen);
    flvtag.timestamp_ex = (timestamp >> 24) & 0xff;
    flvtag.timestamp[0] = (timestamp >> 16) & 0xff;
    flvtag.timestamp[1] = (timestamp >> 8) & 0xff;
    flvtag.timestamp[2] = (timestamp) & 0xff;

    fwrite(&flvtag, sizeof(FlvTag), 1, file_handle_);
    fwrite(buf, 1, bufLen, file_handle_);

    UI32ToBytes(prev_size, bufLen+sizeof(FlvTag));
    fwrite(prev_size, 4, 1, file_handle_);
}
