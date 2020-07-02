//
// Created by Anders Cedronius on 2020-07-01.
//

#ifndef MPEGTS_UNIT_TEST_3_H
#define MPEGTS_UNIT_TEST_3_H

#include <iostream>
#include "mpegts_demuxer.h"
#include "mpegts_muxer.h"

class UnitTest3 {
public:
    bool runTest();
private:
    void muxOutput(SimpleBuffer &rTsOutBuffer);
    void dmxOutput(EsFrame *pEs);

    MpegTsDemuxer mDemuxer;
    int mFrameCounter = 1;
    bool mUnitTestStatus = true;
};

#endif //MPEGTS_UNIT_TEST_3_H
