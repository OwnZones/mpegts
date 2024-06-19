// Copyright (c) 2024, Edgeware AB. All rights reserved.

#pragma once

#include <fstream>
#include <random>
#include "mpegts_demuxer.h"

using namespace mpegts;

class UnitTest9 {
public:
    bool runTest();
private:
    void dmxOutput(const EsFrame &esFrame);

    MpegTsDemuxer mDemuxer;

    int mFrameCounter = 0;
    bool mUnitTestStatus = false;
};
