//
// Created by Anders Cedronius on 2020-07-01.
//

#ifndef MPEGTS_UNIT_TEST_2_H
#define MPEGTS_UNIT_TEST_2_H

#include <iostream>
#include "mpegts_demuxer.h"
#include "mpegts_muxer.h"

using namespace mpegts;

class UnitTest2 {
public:
    bool runTest();
private:
    void muxOutput(SimpleBuffer &rTsOutBuffer);
    void dmxOutput(const EsFrame& esFrame);

    MpegTsDemuxer mDemuxer;
    int mFrameCounter = 1;
    bool mFrameInTransit = false;
    bool mUnitTestStatus = true;
};

#endif //MPEGTS_UNIT_TEST_2_H
