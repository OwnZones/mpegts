//
// Created by Anders Cedronius on 2020-07-01.
//

#ifndef MPEGTS_UNIT_TEST_1_H
#define MPEGTS_UNIT_TEST_1_H

#include <iostream>
#include "mpegts_demuxer.h"
#include "mpegts_muxer.h"

class UnitTest1 {
public:
    bool runTest();
private:
    void muxOutput(SimpleBuffer &rTsOutBuffer);
    void dmxOutput(const EsFrame& esFrame);

    MpegTsDemuxer mDemuxer;
    int mFrameCounter = 1;
    bool mUnitTestStatus = true;
    bool mFrameInTransit = false;
};

#endif //MPEGTS_UNIT_TEST_1_H
