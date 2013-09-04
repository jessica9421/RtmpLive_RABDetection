#ifndef _FLV_READER_H_
#define _FLV_READER_H_

#include <cstdio>

#define FLV_UI32(x) (int)(((x[0]) << 24) + ((x[1]) << 16) + ((x[2]) << 8) + (x[3]))
#define FLV_UI24(x) (int)(((x[0]) << 16) + ((x[1]) << 8) + (x[2]))
#define FLV_UI16(x) (int)(((x[0]) << 8) + (x[1]))
#define FLV_UI8(x) (int)((x))

#define FLV_AUDIODATA	8
#define FLV_VIDEODATA	9
#define FLV_SCRIPTDATAOBJECT	18

struct FlvFileHeader
{
    unsigned char signature[3];
    unsigned char version;
    unsigned char flags;
    unsigned char headersize[4];
};

struct FlvTag
{
    unsigned char type;
    unsigned char datasize[3];
    unsigned char timestamp[3];
    unsigned char timestamp_ex;
    unsigned char streamid[3];
};

class FlvReader
{
public:
    FlvReader(const char* filename);

    ~FlvReader();

    int ReadNextTagHeader(int* tagType, int* timestamp);

    void ReadNextTagData(char* outBuf);

private:
    FILE* fp_;
    int read_pos_;
    int filesize_;
    int reading_tag_data_size_;
};

#endif // _FLV_READER_H_
