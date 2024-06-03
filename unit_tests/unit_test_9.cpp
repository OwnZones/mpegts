// Copyright (c) 2024, Edgeware AB. All rights reserved.

//This unit test is testing the demuxers ability to demux a broken ts stream and maintain ts packet sync

#include "unit_test_9.h"

void UnitTest9::dmxOutput(const EsFrame &esFrame){
    mFrameCounter++;
    mUnitTestStatus = true;
}

bool UnitTest9::runTest() {
    mDemuxer.esOutCallback = std::bind(&UnitTest9::dmxOutput, this, std::placeholders::_1);

    std::ifstream lFile("bars-github.ts", std::ios::binary | std::ios::in);
    if (!lFile.is_open()) {
        std::cout << "Failed to open bars-github.ts - download it using: wget https://github.com/andersc/assets/raw/master/bars.ts -O bars-github.ts" << std::endl;
        mUnitTestStatus = false;
        return mUnitTestStatus;
    }

    uint8_t lPacket[400] = {0};
    SimpleBuffer lIn;

    const std::vector<size_t> lPacketChunkSize = {150, 200, 250, 300, 188, 276, 400};
    size_t lPacketChunkSizeIndex = 0;
    size_t lPacketDestroyIndex = 0;

    while (!lFile.eof()) {
        uint32_t lPacketSize = lPacketChunkSize[lPacketChunkSizeIndex++ % lPacketChunkSize.size()];

        lFile.read((char*)&lPacket[0], lPacketSize);
        uint32_t lBytesRead = lFile.gcount();

        if (lBytesRead) {
            if (lPacketDestroyIndex++ % 5 == 0) {
                // Simulate that every fifth packet is corrupt by removing the last 33 bytes (from the size we append below)
                lBytesRead -= 33;
            }
            lIn.append(lPacket, lBytesRead);
            // decode call will clear the buffer
            mDemuxer.decode(lIn);
        }
    }
    lFile.close();

    if (mFrameCounter != 1115) {
        std::cout << "Frame counter is not as expected: " << mFrameCounter << std::endl;
        mUnitTestStatus = false;
    }

    return mUnitTestStatus; //True = OK
}
