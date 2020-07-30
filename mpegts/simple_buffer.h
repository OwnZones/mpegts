#ifndef __SIMPLE_BUFFER_H__
#define __SIMPLE_BUFFER_H__

#include <vector>
#include <string>
#include <stdint.h>
#include <iostream>

// only support little endian
class SimpleBuffer
{
public:
    SimpleBuffer();
    SimpleBuffer(int32_t size, int8_t value);
    virtual ~SimpleBuffer();

public:
    void write1Byte(int8_t val);
    void write2Bytes(int16_t val);
    void write3Bytes(int32_t val);
    void write4Bytes(int32_t val);
    void write8Bytes(int64_t val);
    void append(const uint8_t* bytes, int size);
    void prepend(const uint8_t* bytes, int size);

    int8_t read1Byte();
    int16_t read2Bytes();
    int32_t read3Bytes();
    int32_t read4Bytes();
    int64_t read8Bytes();
    std::string readString(int len);

    void skip(int size);
    bool require(int required_size);
    bool empty();
    int size();
    int pos();
    uint8_t* data();
    void clear();
    void setData(int pos, const uint8_t* data, int len);

private:
    std::vector<uint8_t> mData;
    int mPos;
};

#endif /* __SIMPLE_BUFFER_H__ */
