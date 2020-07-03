//
// Created by Anders Cedronius on 2020-07-01.
//

#ifndef MPEGTS_UNIT_TEST_6_H
#define MPEGTS_UNIT_TEST_6_H

#include <iostream>
#include "mpegts_demuxer.h"
#include "mpegts_muxer.h"

class UnitTest6 {
public:
    bool runTest();
private:
    void muxOutput(SimpleBuffer &rTsOutBuffer);
    void dmxOutputPts(EsFrame *pEs);
    void dmxOutputPtsDts(EsFrame *pEs);
    void dmxOutputPcr(uint64_t lPcr);

    MpegTsDemuxer mDemuxer;
    MpegTsMuxer *mpMuxer = nullptr;
    int mFrameCounter = 1;
    bool mUnitTestStatus = true;
    bool mFrameInTransit = false;
    uint64_t mPts = 0;
    uint64_t mDts = 0;
    uint64_t mPcr = 0;
};

#endif //MPEGTS_UNIT_TEST_6_H
