//
// Created by Anders Cedronius on 2020-07-01.
//

#ifndef MPEGTS_UNIT_TEST_5_H
#define MPEGTS_UNIT_TEST_5_H

#include <iostream>
#include "mpegts_demuxer.h"
#include "mpegts_muxer.h"

using namespace mpegts;

class UnitTest5 {
public:
    bool runTest();

    void dmxOutput(const EsFrame &esFrame);
    bool mFrameInTransit = false;
    bool mUnitTestStatus = true;
    int mPacketLength = 0;

};

#endif //MPEGTS_UNIT_TEST_5_H
