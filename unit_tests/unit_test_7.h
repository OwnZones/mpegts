//
// Created by Anders Cedronius on 2020-07-30.
//

#ifndef MPEGTS_UNIT_TEST_7_H
#define MPEGTS_UNIT_TEST_7_H

#include <fstream>
#include <random>
#include "mpegts_demuxer.h"

class UnitTest7 {
public:
    bool runTest();
private:
    void dmxOutput(const EsFrame *pEs);
    void dmxOutputPcr(uint64_t lPcr);

    MpegTsDemuxer mDemuxer;

    int mFrameCounter = 0;
    bool mUnitTestStatus = false;
    bool mFrameInTransit = false;

};

#endif //MPEGTS_UNIT_TEST_7_H
