//
// Created by Anders Cedronius on 2020-07-01.
//


//This unit test is

//1. Sending a vector of bytes from 1 byte to TEST_VECTOR_SIZE (4000) bytes
//The vector is a linear counting uint8_t that is multiplexed and demuxed
//The result is compared against the expected length and vector linearity

//2. Verify the PTS and DTS values (They are set to the packet size and packet number)

//3. Verify the muxer is spiting out a modulo 188 size packets

//4. Every tenth frame is set to mRandomAccess. Verify the correctness after muxing demuxing

//This unit test is sending each TS to the demuxer meaning 188 bytes at a time


#include "unit_test_1.h"

//AAC (ADTS) audio
#define TYPE_AUDIO 0x0f
//H.264 video
#define TYPE_VIDEO 0x1b
//Audio PID
#define AUDIO_PID 257
//Video PID
#define VIDEO_PID 256
//PMT PID
#define PMT_PID 100

//Test Vector size
#define TEST_VECTOR_SIZE 4000

void UnitTest1::dmxOutput(EsFrame *pEs){


    if ((frameCounter%10)?0:1 != pEs->mRandomAccess) {
        std::cout << "mRandomAccess indication error: " << unsigned(pEs->mRandomAccess) << " Frame -> " << unsigned(frameCounter) << std::endl;
        mUnitTestStatus = false;
    }

    //verify expected size
    if (pEs->mData->size() != frameCounter ) {
        std::cout << "Frame size missmatch" << unsigned(pEs->mData->size()) << std::endl;
        mUnitTestStatus = false;
    }

    //Make sure PTS and DTS are set as expected
    if (pEs->mPts != frameCounter ||  pEs->mDts != frameCounter) {
        std::cout << "PTS - DTS" << std::endl;
        mUnitTestStatus = false;
    }

    uint8_t referenceVector = 0;
    //verify expected vector
    for (int lI = 0 ; lI < frameCounter ; lI++) {
        if (pEs->mData->data()[lI] != referenceVector++ ) {
            std::cout << "Content missmatch for packet size -> " << unsigned(frameCounter) << " at byte: " << unsigned(lI);
            std::cout << " Expected " << unsigned(referenceVector -1 ) << " got " << (uint8_t)pEs->mData->data()[lI] + 0 << std::endl;
            mUnitTestStatus = false;
        }
    }

    frameCounter++;

}

void UnitTest1::muxOutput(SimpleBuffer &rTsOutBuffer) {
    //Double to fail at non integer data and be able to visualize in the print-out
    double packets = (double) rTsOutBuffer.size() / 188.0;
    if (packets != (int) packets) {
        std::cout << "Payload not X * 188 " << std::endl;
        mUnitTestStatus = false;
    }

    uint8_t* lpData = rTsOutBuffer.data();


    for (int lI = 0 ; lI < packets ; lI++) {
        SimpleBuffer lIn;
        lIn.append(lpData+(lI*188), 188);
        mDemuxer.decode(lIn);
    }
}

bool UnitTest1::runTest() {

    mDemuxer.esOutCallback = std::bind(&UnitTest1::dmxOutput, this, std::placeholders::_1);

    uint8_t testVector[TEST_VECTOR_SIZE];
    std::map<uint8_t, int> gStreamPidMap;
    gStreamPidMap[TYPE_VIDEO] = VIDEO_PID;
    MpegTsMuxer lMuxer(gStreamPidMap, PMT_PID, VIDEO_PID);
    lMuxer.tsOutCallback = std::bind(&UnitTest1::muxOutput, this, std::placeholders::_1);

    //Make Vector
    for (int x = 0; x < TEST_VECTOR_SIZE; x++) {
        testVector[x] = x;
    }

    //Run trough all sizes
    for (int x = 1; x < TEST_VECTOR_SIZE+1; x++) {
        EsFrame lEsFrame;
        lEsFrame.mData = std::make_shared<SimpleBuffer>();
        lEsFrame.mData->append((const uint8_t *)&testVector[0], x);
        lEsFrame.mPts = x;
        lEsFrame.mDts = x;
        lEsFrame.mPcr = 0;
        lEsFrame.mStreamType = TYPE_VIDEO;
        lEsFrame.mStreamId = 224;
        lEsFrame.mPid = VIDEO_PID;
        lEsFrame.mExpectedPesPacketLength = 0;
        lEsFrame.mRandomAccess = (x%10)?0:1;
        lEsFrame.mCompleted = true;

        lMuxer.encode(lEsFrame);
    }
    return mUnitTestStatus; //True = OK
}