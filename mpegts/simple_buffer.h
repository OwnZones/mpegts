#ifndef __SIMPLE_BUFFER_H__
#define __SIMPLE_BUFFER_H__

#include <vector>
#include <string>
#include <stdint.h>
#include <iostream>

namespace mpegts {

// only support little endian
class SimpleBuffer
{
public:
    SimpleBuffer();
    SimpleBuffer(int32_t size, int8_t value);
    virtual ~SimpleBuffer();

public:
    void write1Byte(uint8_t val);
    void write2Bytes(uint16_t val);
    void write3Bytes(uint32_t val);
    void write4Bytes(uint32_t val);
    void write8Bytes(uint64_t val);
    void append(const uint8_t* bytes, size_t size);
    void prepend(const uint8_t* bytes, size_t size);

    uint8_t read1Byte();
    uint16_t read2Bytes();
    uint32_t read3Bytes();
    uint32_t read4Bytes();
    uint64_t read8Bytes();
    std::string readString(size_t len);

    void skip(size_t size);
    [[nodiscard]] bool require(size_t required_size) const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t pos() const;
    uint8_t* data();
    uint8_t* currentData();
    [[nodiscard]] size_t dataLeft() const;
    void clear();
    void setData(size_t pos, const uint8_t* data, size_t len);

private:
    std::vector<uint8_t> mData;
    size_t mPos = 0;
};

}
#endif /* __SIMPLE_BUFFER_H__ */
