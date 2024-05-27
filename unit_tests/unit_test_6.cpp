//
// Created by Anders Cedronius on 2020-07-01.
//


//This unit test is testing the PTS TDS and PCR values after mux and demux

//PTS only
//PTS + DTS
//PTS + DTS + OOB (meaning separate packets) PCR
//PTS + DTS + PCR in the video pid (PES packet)

#include "unit_test_6.h"

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
//PCR PID
#define PCR_PID 300

//Test Vector size
#define TEST_VECTOR_SIZE 100

#define NUM_FRAMES_LOOP 2000

void UnitTest6::dmxOutputPcr(uint64_t lPcr) {
    if ((mDts + mPts + mPcr) != lPcr) {
        std::cout << "PCR mismatch." << std::endl;
        mUnitTestStatus = false;
    }
}

void UnitTest6::dmxOutputPtsDts(const EsFrame &esFrame) {
    if (mPts != esFrame.mPts) {
        std::cout << "PTS mismatch." << std::endl;
        mUnitTestStatus = false;
    }

    if (mDts != esFrame.mDts) {
        std::cout << "DTS mismatch." << std::endl;
        mUnitTestStatus = false;
    }

    uint8_t referenceVector = 0;
    //verify expected vector
    for (int lI = 0 ; lI < TEST_VECTOR_SIZE ; lI++) {
        if (esFrame.mData->data()[lI] != referenceVector++ ) {
            std::cout << "Content missmatch." << std::endl;
            mUnitTestStatus = false;
        }
    }

    mFrameInTransit = false;
}

void UnitTest6::dmxOutputPts(const EsFrame &esFrame){

    if (mPts != esFrame.mPts) {
        std::cout << "PTS missmatch." << std::endl;
    }

    uint8_t referenceVector = 0;
    //verify expected vector
    for (int lI = 0 ; lI < TEST_VECTOR_SIZE ; lI++) {
        if (esFrame.mData->data()[lI] != referenceVector++ ) {
            std::cout << "Content missmatch." << std::endl;
            mUnitTestStatus = false;
        }
    }

    mFrameInTransit = false;

}

void UnitTest6::muxOutput(SimpleBuffer &rTsOutBuffer) {
    //Double to fail at non integer data
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

bool UnitTest6::runTest() {

    mDemuxer.esOutCallback = std::bind(&UnitTest6::dmxOutputPts, this, std::placeholders::_1);

    uint8_t testVector[TEST_VECTOR_SIZE];
    std::vector<std::shared_ptr<PMTElementInfo>> gEsStreamInfo;
    gEsStreamInfo.push_back(std::shared_ptr<PMTElementInfo>(new PMTElementInfo(TYPE_VIDEO, VIDEO_PID)));
    mpMuxer = new MpegTsMuxer(gEsStreamInfo, PMT_PID, PCR_PID, MpegTsMuxer::MuxType::h222Type);
    mpMuxer->tsOutCallback = std::bind(&UnitTest6::muxOutput, this, std::placeholders::_1);

    //Make Vector
    for (int x = 0; x < TEST_VECTOR_SIZE; x++) {
        testVector[x] = x;
    }

    mPts = 0;
    mDts = 0;
    mPcr = 0;

    //Send frames only containing PTS
    for (int x = 1; x < NUM_FRAMES_LOOP; x++) {
        EsFrame lEsFrame;
        lEsFrame.mData = std::make_shared<SimpleBuffer>();
        lEsFrame.mData->append((const uint8_t *)&testVector[0], TEST_VECTOR_SIZE);
        lEsFrame.mPts = mPts;
        lEsFrame.mDts = mPts;
        lEsFrame.mPcr = 0;
        lEsFrame.mStreamType = TYPE_VIDEO;
        lEsFrame.mStreamId = 224;
        lEsFrame.mPid = VIDEO_PID;
        lEsFrame.mExpectedPesPacketLength = 0;
        lEsFrame.mRandomAccess = 1;
        lEsFrame.mCompleted = true;
        mFrameInTransit = true;
        mpMuxer->encode(lEsFrame);
        if (mFrameInTransit) {
            std::cout << "Frame " << unsigned(x) << " not muxed/demuxed corectly" << std::endl;
            mUnitTestStatus = false;
        }
        mPts += 1920;
    }

    //Flip the callback to analyze DTS also
    mDemuxer.esOutCallback = std::bind(&UnitTest6::dmxOutputPtsDts, this, std::placeholders::_1);

    //Send frames containing PTS / DTS
    for (int x = 1; x < NUM_FRAMES_LOOP; x++) {
        EsFrame lEsFrame;
        lEsFrame.mData = std::make_shared<SimpleBuffer>();
        lEsFrame.mData->append((const uint8_t *)&testVector[0], TEST_VECTOR_SIZE);
        lEsFrame.mPts = mPts;
        lEsFrame.mDts = mDts;
        lEsFrame.mPcr = 0;
        lEsFrame.mStreamType = TYPE_VIDEO;
        lEsFrame.mStreamId = 224;
        lEsFrame.mPid = VIDEO_PID;
        lEsFrame.mExpectedPesPacketLength = 0;
        lEsFrame.mRandomAccess = 1;
        lEsFrame.mCompleted = true;
        mFrameInTransit = true;
        mpMuxer->encode(lEsFrame);
        if (mFrameInTransit) {
            std::cout << "Frame " << unsigned(x) << " not muxed/demuxed corectly" << std::endl;
            mUnitTestStatus = false;
        }
        mPts += 1111;
        mDts += 2920;
    }


    mDemuxer.pcrOutCallback = std::bind(&UnitTest6::dmxOutputPcr, this, std::placeholders::_1);

    //Send frames containing PTS / DTS then also send out of band PCR
    for (int x = 1; x < NUM_FRAMES_LOOP; x++) {
        EsFrame lEsFrame;
        lEsFrame.mData = std::make_shared<SimpleBuffer>();
        lEsFrame.mData->append((const uint8_t *)&testVector[0], TEST_VECTOR_SIZE);
        lEsFrame.mPts = mPts;
        lEsFrame.mDts = mDts;
        lEsFrame.mPcr = 0;
        lEsFrame.mStreamType = TYPE_VIDEO;
        lEsFrame.mStreamId = 224;
        lEsFrame.mPid = VIDEO_PID;
        lEsFrame.mExpectedPesPacketLength = 0;
        lEsFrame.mRandomAccess = 1;
        lEsFrame.mCompleted = true;
        mFrameInTransit = true;
        mpMuxer->encode(lEsFrame);
        mpMuxer->createPcr(mPts+mDts+mPcr);
        if (mFrameInTransit) {
            std::cout << "Frame " << unsigned(x) << " not muxed/demuxed corectly" << std::endl;
            mUnitTestStatus = false;
        }
        mPts += 3920;
        mDts += 1923;
        mPcr += 1;
    }

    //Here we switch the PCR to be embedded into the data..

    //So we delete the old muxer and create a new one using the video pid as PCR pid
    //That means that the PCR will be taken from the Elementary stream data.
    delete mpMuxer;
    mpMuxer = new MpegTsMuxer(gEsStreamInfo, PMT_PID, VIDEO_PID, MpegTsMuxer::MuxType::h222Type);
    mpMuxer->tsOutCallback = std::bind(&UnitTest6::muxOutput, this, std::placeholders::_1);


    //Send frames containing PTS / DTS then also send in band PCR
    for (int x = 1; x < NUM_FRAMES_LOOP; x++) {
        EsFrame lEsFrame;
        lEsFrame.mData = std::make_shared<SimpleBuffer>();
        lEsFrame.mData->append((const uint8_t *)&testVector[0], TEST_VECTOR_SIZE);
        lEsFrame.mPts = mPts;
        lEsFrame.mDts = mDts;
        lEsFrame.mPcr = mPts+mDts+mPcr;
        lEsFrame.mStreamType = TYPE_VIDEO;
        lEsFrame.mStreamId = 224;
        lEsFrame.mPid = VIDEO_PID;
        lEsFrame.mExpectedPesPacketLength = 0;
        lEsFrame.mRandomAccess = 1;
        lEsFrame.mCompleted = true;
        mFrameInTransit = true;
        mpMuxer->encode(lEsFrame);
        if (mFrameInTransit) {
            std::cout << "Frame " << unsigned(x) << " not muxed/demuxed corectly" << std::endl;
            mUnitTestStatus = false;
        }
        mPts += 1234;
        mDts += 4321;
        mPcr += 1;
    }

    delete mpMuxer;
    return mUnitTestStatus; //True = OK
}
