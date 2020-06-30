//
// Created by akanchi on 2019-12-31.
//

#pragma once

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// l local scope

#include "simple_buffer.h"
#include "ts_packet.h"

class FLVMuxer {
public:
    FLVMuxer();

    virtual ~FLVMuxer();

public:
    int writeHeader(SimpleBuffer *pSb);

    int writeBody(EsFrame *pFrame, SimpleBuffer *pSb);

    int writeMetadata(SimpleBuffer *pSb, uint32_t lFileSize);

private:
    int writeAacTag(EsFrame *pFrame, SimpleBuffer *pSb);

    int writeAvcTag(EsFrame *pFrame, SimpleBuffer *pSb);

    void calcDuration(uint32_t lPts);

private:
    SimpleBuffer mPpsData;
    SimpleBuffer mSpsData;
    bool mHasSetStartPts;
    uint32_t mStartPts;
    uint32_t mDuration;
};

