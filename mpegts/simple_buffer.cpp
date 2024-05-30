#include "simple_buffer.h"

#include <cstring>
#include <iterator>

#include <assert.h>

namespace mpegts {

SimpleBuffer::SimpleBuffer() = default;

SimpleBuffer::SimpleBuffer(int32_t size, int8_t value)
{
    mData = std::vector<uint8_t>(size, value);
}

SimpleBuffer::~SimpleBuffer() = default;

void SimpleBuffer::write1Byte(uint8_t val)
{
    mData.push_back(val);
}

void SimpleBuffer::write2Bytes(uint16_t val)
{
    uint8_t *p = reinterpret_cast<uint8_t*>(&val);

    for (int i = 1; i >= 0; --i) {
        mData.push_back(p[i]);
    }
}

void SimpleBuffer::write3Bytes(uint32_t val)
{
    uint8_t *p = reinterpret_cast<uint8_t*>(&val);

    for (int i = 2; i >= 0; --i) {
        mData.push_back(p[i]);
    }
}

void SimpleBuffer::write4Bytes(uint32_t val)
{
    uint8_t *p = reinterpret_cast<uint8_t*>(&val);

    for (int i = 3; i >= 0; --i) {
        mData.push_back(p[i]);
    }
}

void SimpleBuffer::write8Bytes(uint64_t val)
{
    uint8_t *p = reinterpret_cast<uint8_t*>(&val);

    for (int i = 7; i >= 0; --i) {
        mData.push_back(p[i]);
    }
}

void SimpleBuffer::append(const uint8_t* bytes, size_t size)
{
    if (!bytes || size <= 0) {
#ifdef DEBUG
        std::cout << " append error " << std::endl;
#endif
        return;
    }

    mData.insert(mData.end(), bytes, bytes + size);
}

void SimpleBuffer::prepend(const uint8_t* bytes, size_t size)
{
    if (!bytes || size <= 0) {
#ifdef DEBUG
        std::cout << " prepend error " << std::endl;
#endif
        return;
    }

    mData.insert(mData.begin(), bytes, bytes + size);
}

uint8_t SimpleBuffer::read1Byte()
{
    assert(require(1));

    uint8_t val = mData.at(0 + mPos);
    mPos++;

    return val;
}

uint16_t SimpleBuffer::read2Bytes()
{
    assert(require(2));

    uint16_t val = 0;
    uint8_t *p = reinterpret_cast<uint8_t*>(&val);

    for (int i = 1; i >= 0; --i) {
        p[i] = mData.at(0 + mPos);
        mPos++;
    }

    return val;
}

uint32_t SimpleBuffer::read3Bytes()
{
    assert(require(3));

    uint32_t val = 0;
    uint8_t *p = reinterpret_cast<uint8_t*>(&val);

    for (int i = 2; i >= 0; --i) {
        p[i] = mData.at(0 + mPos);
        mPos++;
    }

    return val;
}

uint32_t SimpleBuffer::read4Bytes()
{
    assert(require(4));

    int32_t val = 0;
    uint8_t *p = reinterpret_cast<uint8_t*>(&val);

    for (int i = 3; i >= 0; --i) {
        p[i] = mData.at(0 + mPos);
        mPos++;
    }

    return val;
}

uint64_t SimpleBuffer::read8Bytes()
{
    assert(require(8));

    uint64_t val = 0;
    uint8_t *p = reinterpret_cast<uint8_t*>(&val);

    for (int i = 7; i >= 0; --i) {
        p[i] = mData.at(0 + mPos);
        mPos++;
    }

    return val;
}

std::string SimpleBuffer::readString(size_t len)
{
    assert(require(len));

    std::string val(reinterpret_cast<char*>(&mData.at(mPos)), len);
    mPos += len;

    return val;
}

void SimpleBuffer::skip(size_t size)
{
    mPos += size;
}

bool SimpleBuffer::require(size_t required_size)
{
    assert(required_size >= 0);

    return required_size <= mData.size() - mPos;
}

bool SimpleBuffer::empty()
{
    return mPos >= mData.size();
}

size_t SimpleBuffer::size()
{
    return mData.size();
}

size_t SimpleBuffer::pos() const
{
    return mPos;
}

uint8_t* SimpleBuffer::data()
{
    return mData.empty() ? nullptr : &mData[0];
}

uint8_t* SimpleBuffer::currentData()
{
    return (mData.empty() || mPos >= mData.size()) ? nullptr : &mData[mPos];
}

size_t SimpleBuffer::dataLeft()
{
    return mData.size() - mPos;
}

void SimpleBuffer::clear()
{
    mPos = 0;
    mData.clear();
}

void SimpleBuffer::setData(size_t pos, const uint8_t* data, size_t len)
{
    if (!data) {
#ifdef DEBUG
        std::cout << " setData error data ptr == nullpts " << std::endl;
#endif
        return;
    }

    if (pos + len > size()) {
#ifdef DEBUG
        std::cout << " setData error data out of bounds " << std::endl;
#endif
        return;
    }

    std::memcpy(currentData(), data, len);
}

size_t SimpleBuffer::bytesLeft() {
    return mData.size() - mPos;
}

}
