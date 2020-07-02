#include "simple_buffer.h"

#include <assert.h>
//#include <algorithm>
#include <iterator>

SimpleBuffer::SimpleBuffer()
    : mPos(0)
{
}

SimpleBuffer::SimpleBuffer(int32_t size, int8_t value)
    : mPos(0)
{
    mData = std::vector<uint8_t>(size, value);
}

SimpleBuffer::~SimpleBuffer()
{
}

void SimpleBuffer::write_1byte(int8_t val)
{
    mData.push_back(val);
}

void SimpleBuffer::write_2bytes(int16_t val)
{
    char *p = (char *)&val;

    for (int i = 1; i >= 0; --i) {
        mData.push_back(p[i]);
    }
}

void SimpleBuffer::write_3bytes(int32_t val)
{
    char *p = (char *)&val;

    for (int i = 2; i >= 0; --i) {
        mData.push_back(p[i]);
    }
}

void SimpleBuffer::write_4bytes(int32_t val)
{
    char *p = (char *)&val;

    for (int i = 3; i >= 0; --i) {
        mData.push_back(p[i]);
    }
}

void SimpleBuffer::write_8bytes(int64_t val)
{
    char *p = (char *)&val;

    for (int i = 7; i >= 0; --i) {
        mData.push_back(p[i]);
    }
}

void SimpleBuffer::write_string(std::string val)
{
    std::copy(val.begin(), val.end(), std::back_inserter(mData));
}

void SimpleBuffer::append(const uint8_t* bytes, int size)
{
    if (!bytes || size <= 0)
        return;

    mData.insert(mData.end(), bytes, bytes + size);
}

int8_t SimpleBuffer::read_1byte()
{
    assert(require(1));

    int8_t val = mData.at(0 + mPos);
    mPos++;

    return val;
}

int16_t SimpleBuffer::read_2bytes()
{
    assert(require(2));

    int16_t val = 0;
    char *p = (char *)&val;

    for (int i = 1; i >= 0; --i) {
        p[i] = mData.at(0 + mPos);
        mPos++;
    }

    return val;
}

int32_t SimpleBuffer::read_3bytes()
{
    assert(require(3));

    int32_t val = 0;
    char *p = (char *)&val;

    for (int i = 2; i >= 0; --i) {
        p[i] = mData.at(0 + mPos);
        mPos++;
    }

    return val;
}

int32_t SimpleBuffer::read_4bytes()
{
    assert(require(4));

    int32_t val = 0;
    char *p = (char *)&val;

    for (int i = 3; i >= 0; --i) {
        p[i] = mData.at(0 + mPos);
        mPos++;
    }

    return val;
}

int64_t SimpleBuffer::read_8bytes()
{
    assert(require(8));

    int64_t val = 0;
    char *p = (char *)&val;

    for (int i = 7; i >= 0; --i) {
        p[i] = mData.at(0 + mPos);
        mPos++;
    }

    return val;
}

std::string SimpleBuffer::read_string(int len)
{
    assert(require(len));



    std::string val(*(char*)&mData[0] + mPos, len);
    mPos += len;

    return val;
}

void SimpleBuffer::skip(int size)
{
    mPos += size;
}

bool SimpleBuffer::require(int required_size)
{
    assert(required_size >= 0);

    return required_size <= mData.size() - mPos;
}

bool SimpleBuffer::empty()
{
    return mPos >= mData.size();
}

int SimpleBuffer::size()
{
    return mData.size();
}

int SimpleBuffer::pos()
{
    return mPos;
}

uint8_t* SimpleBuffer::data()
{
    return (size() == 0) ? nullptr : &mData[0];
}

void SimpleBuffer::clear()
{
    mPos = 0;
    mData.clear();
}

void SimpleBuffer::set_data(int pos, const uint8_t* data, int len)
{
    if (!data)
        return;

    if (pos + len > size()) {
        return;
    }

    for (int i = 0; i < len; i++) {
        mData[pos + i] = data[i];
    }
}

std::string SimpleBuffer::to_string()
{
    return std::string(mData.begin(), mData.end());
}
