#include "stdafx.h"
#include "FlvReader.h"

FlvReader::FlvReader(const char* filename)
{
    fp_ = fopen(filename, "rb");

    fseek(fp_, 0, SEEK_END);
    filesize_ = ftell(fp_);
    fseek(fp_, 0, SEEK_SET);

    // 读取flv文件头
    FlvFileHeader flvheader;
    fread(&flvheader, sizeof(FlvFileHeader), 1, fp_);

    // 跳过flv头剩余的大小
    unsigned int headersize = FLV_UI32(flvheader.headersize);
    fseek(fp_, headersize-sizeof(FlvFileHeader), SEEK_CUR);

    // 跳过第一个PreviousTagSize
    fseek(fp_, 4, SEEK_CUR);

    read_pos_ = headersize + 4;
}

FlvReader::~FlvReader()
{
    fclose(fp_);
}

int FlvReader::ReadNextTagHeader(int* tagType, int* timestamp)
{
    if (read_pos_ + sizeof(FlvTag) > filesize_)
    {
        return -1;
    }

    FlvTag flvtag;
    fread(&flvtag, sizeof(FlvTag), 1, fp_);
    read_pos_ += sizeof(FlvTag);

    *timestamp = (flvtag.timestamp_ex << 24) + 
        (flvtag.timestamp[0] << 16) + 
        (flvtag.timestamp[1] << 8) + 
        (flvtag.timestamp[2]);
    *tagType = flvtag.type;
    reading_tag_data_size_ = FLV_UI24(flvtag.datasize);

    return reading_tag_data_size_;
}

void FlvReader::ReadNextTagData(char* outBuf)
{
    fread(outBuf, reading_tag_data_size_, 1, fp_);
    read_pos_ += reading_tag_data_size_;

    fseek(fp_, 4, SEEK_CUR);
    read_pos_ += 4;
}
