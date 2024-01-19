//
// Created by Anders Cedronius on 2020-07-30.
//


//This unit test is testing the demuxers ability to demux non ts size inputs

#include "unit_test_7.h"

void UnitTest7::dmxOutputPcr(uint64_t lPcr) {
}

void UnitTest7::dmxOutput(const EsFrame &esFrame){
    mFrameCounter ++;
    mUnitTestStatus = true;
}

bool UnitTest7::runTest() {

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> lRandGenerator(1,300); // distribution in range [1, 300]

    mDemuxer.esOutCallback = std::bind(&UnitTest7::dmxOutput, this, std::placeholders::_1);
    mDemuxer.pcrOutCallback = std::bind(&UnitTest7::dmxOutputPcr, this, std::placeholders::_1);

    std::ifstream lFile("bars.ts", std::ios::binary | std::ios::in);
    uint8_t lPacket[300] = {0};
    SimpleBuffer lIn;

    while (!lFile.eof()) {
        uint32_t lRandSize = lRandGenerator(rng);
        lFile.read((char*)&lPacket[0], lRandSize);
        uint32_t lBytesRead = lFile.gcount();
        if (lBytesRead) {
            lIn.append(lPacket, lBytesRead);
            mDemuxer.decode(lIn);
        }
    }
    lFile.close();

    if (mFrameCounter != 1337) {
        mUnitTestStatus = false;
    }

    return mUnitTestStatus; //True = OK
}
