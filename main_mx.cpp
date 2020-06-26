//
// Created by Anders Cedronius on 2020-06-24.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <functional>
#include "kissnet/kissnet.hpp"
#include "mpegts_muxer.h"

//AAC audio
#define TYPE_AUDIO 0x0f
//H.264 video
#define TYPE_VIDEO 0x1b

//Audio PID
#define AUDIO_PID 257
//Video PID
#define VIDEO_PID 256
//PMT PID
#define PMT_PID 100

#define SECOND_SLEEP 1000 * 1000
#define LOOP_FRAME_COUNT_VIDEO 1200
#define LOOP_FRAME_COUNT_AUDIO 1125

//Network part
#define TARGET_INTERFACE "127.0.0.1"
#define TARGET_PORT 8100
kissnet::udp_socket mpegTsOut(kissnet::endpoint(TARGET_INTERFACE, TARGET_PORT));
//Network part end

int64_t gTimeReference;
uint64_t gVideoFrameCounter = 0;
uint64_t gAudioFrameCounter = 0;

uint64_t gFirstPts = 0;
uint64_t gPtsLoopDuration = 0;

//Create the multiplexer
MpegTsMuxer gMuxer;
std::map<uint8_t, int> gStreamPidMap;
std::mutex gEncMtx;

void muxOutput(TsFrame &rFrame){
    std::lock_guard<std::mutex> lock(gEncMtx);
    //Create your TS-Buffer
    SimpleBuffer lTsOutBuffer;
    //Multiplex your data
    gMuxer.encode(&rFrame, gStreamPidMap, PMT_PID, &lTsOutBuffer);

    //Double to fail at non integer data and be able to visualize in the print-out
    double packets = (double)lTsOutBuffer.size() / 188.0;
    //std::cout << "Sending -> " << packets << " MPEG-TS packets" << std::endl;
    char* lpData = lTsOutBuffer.data();
    for (int lI = 0 ; lI < packets ; lI++) {
        mpegTsOut.send((const std::byte *)lpData+(lI*188), 188);
    }
    lTsOutBuffer.clear(); //Not needed
}

void fakeAudioEncoder() {

    double lAdtsTimeDistance = (1024.0/48000.0) * 1000000.0;

    double lNextTriger = (double)gTimeReference + lAdtsTimeDistance; //Meaning every 21.33333..ms from baseTime we send a adts frame
    uint64_t lFrameCounter = 0;
    char *adtsFrame;
    std::string lThisPTS;
    uint64_t lPts = gFirstPts-1920;
    uint64_t lCurrentOffset = 0;
    TsFrame lTsFrame;

    uint64_t lLoopCounter = UINT64_MAX;
    while (lLoopCounter) {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        int64_t lTimeNow = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        if (lTimeNow >= lNextTriger) {
            lNextTriger += (1024.0/48000.0) * 1000000.0;
            gAudioFrameCounter++;

            std::string aFilename;
            std::ostringstream aFilenameBld;
            aFilenameBld << "../bars_dmx/af" << lFrameCounter+1 << ".adts";
            aFilename = aFilenameBld.str();
            //std::cout << " Filename: " << aFilename  << std::endl;

            std::ifstream adtsFile (aFilename, std::ios::in|std::ios::binary|std::ios::ate);
            if (adtsFile.is_open())
            {
                size_t lFileSize= adtsFile.tellg();
                adtsFrame = new char [lFileSize];
                adtsFile.seekg (0, std::ios::beg);
                adtsFile.read (adtsFrame, lFileSize);
                adtsFile.close();

                lPts +=  90000/(48000/1024);
                uint64_t lRecalcPts = lPts + lCurrentOffset;

                lTsFrame.mData = std::make_shared<SimpleBuffer>();
                lTsFrame.mData->append((const char *)adtsFrame, lFileSize);
                lTsFrame.mPts = lRecalcPts;
                lTsFrame.mDts = lRecalcPts;
                lTsFrame.mPcr = 0;
                lTsFrame.mStreamType = TYPE_AUDIO;
                lTsFrame.mStreamId = 192;
                lTsFrame.mPid = AUDIO_PID;
                lTsFrame.mExpectedPesPacketLength = 0;
                lTsFrame.mCompleted = true;

                muxOutput(lTsFrame);

                delete[] adtsFrame;
            }
            else std::cout << "Unable to open file" << std::endl;


            if ((lFrameCounter+1) == LOOP_FRAME_COUNT_AUDIO) {
                lCurrentOffset += gPtsLoopDuration;
                lPts = gFirstPts-1920;
            }

            lFrameCounter = (lFrameCounter + 1) % LOOP_FRAME_COUNT_AUDIO;

        }
    }


}

void fakeVideoEncoder() {
    int64_t lNextTriger = gTimeReference + (20*1000); //Meaning every 20ms from baseTime we send a picture
    uint64_t lFrameCounter = 0;
    char *videoNal;
    std::string lThisPTS;
    std::string lThisDTS;
    uint64_t lPts = 0;
    uint64_t lDts = 0;
    uint64_t lCurrentOffset = 0;
    TsFrame lTsFrame;

    uint64_t lLoopCounter = UINT64_MAX;

    while (lLoopCounter) {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        int64_t lTimeNow = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        if (lTimeNow >= lNextTriger) {
            lNextTriger += (20*1000);
            gVideoFrameCounter++;

            std::string vFilename;
            std::ostringstream vFilenameBld;
            vFilenameBld << "../bars_dmx/vf" << lFrameCounter+1 << ".h264";
            vFilename = vFilenameBld.str();
            //std::cout << " Filename: " << vFilename  << std::endl;

            std::string ptsDtsFilename;
            std::ostringstream ptsDtsFilenameBld;
            ptsDtsFilenameBld << "../bars_dmx/ptsdts" << lFrameCounter+1 << ".txt";
            ptsDtsFilename = ptsDtsFilenameBld.str();

            std::ifstream thisPtsFile(ptsDtsFilename);
            std::getline(thisPtsFile, lThisPTS);
            std::getline(thisPtsFile, lThisDTS);
            thisPtsFile.close();
            std::stringstream lPtsSS(lThisPTS);
            lPtsSS >> lPts;
            std::stringstream lDtsSS(lThisDTS);
            lDtsSS >> lDts;

            std::ifstream nalFile (vFilename, std::ios::in|std::ios::binary|std::ios::ate);
            if (nalFile.is_open())
            {
                size_t lFileSize= nalFile.tellg();
                videoNal = new char [lFileSize];
                nalFile.seekg (0, std::ios::beg);
                nalFile.read (videoNal, lFileSize);
                nalFile.close();

                uint64_t lRecalcPts = lPts + lCurrentOffset;
                uint64_t lRecalcDts = lDts + lCurrentOffset;

                lTsFrame.mData = std::make_shared<SimpleBuffer>();
                lTsFrame.mData->append((const char *)videoNal, lFileSize);
                lTsFrame.mPts = lRecalcPts;
                lTsFrame.mDts = lRecalcDts;
                lTsFrame.mPcr = 0;
                lTsFrame.mStreamType = TYPE_VIDEO;
                lTsFrame.mStreamId = 224;
                lTsFrame.mPid = VIDEO_PID;
                lTsFrame.mExpectedPesPacketLength = 0;
                lTsFrame.mCompleted = true;

                muxOutput(lTsFrame);

                delete[] videoNal;
            }
            else std::cout << "Unable to open file" << std::endl;


            if ((lFrameCounter+1) == LOOP_FRAME_COUNT_VIDEO) {
                lCurrentOffset += gPtsLoopDuration;
            }

            lFrameCounter = (lFrameCounter + 1) % LOOP_FRAME_COUNT_VIDEO;

        }
    }
}

int main(int argc, char *argv[]) {
    std::cout << "TS - muxlib test " << std::endl;

    gStreamPidMap[TYPE_AUDIO] = AUDIO_PID;
    gStreamPidMap[TYPE_VIDEO] = VIDEO_PID;

    std::string lStartPTS;
    std::string lEndPTS;
    std::ifstream startPtsFile("../bars_dmx/ptsdts1.txt");
    std::getline(startPtsFile, lStartPTS);
    startPtsFile.close();
    std::ifstream endPtsFile("../bars_dmx/ptsdts1201.txt");
    std::getline(endPtsFile, lEndPTS);
    endPtsFile.close();
    std::stringstream frst(lStartPTS);
    frst >> gFirstPts;
    uint64_t lLastPts = 0;
    std::stringstream lst(lEndPTS);
    lst >> lLastPts;
    gPtsLoopDuration = lLastPts - gFirstPts;

    uint64_t lVideoFrameCounter = 0;

    gTimeReference = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

    std::thread(std::bind(&fakeAudioEncoder)).detach();
    std::thread(std::bind(&fakeVideoEncoder)).detach();

    uint64_t lLoopCounter = UINT64_MAX;
    while (lLoopCounter) {
        gTimeReference += SECOND_SLEEP;
        int64_t lTimeNow = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        int64_t lTimeCompensation = gTimeReference - lTimeNow;
        if (lTimeCompensation < 0) {
            std::cout << "Stats printing overloaded" << std::endl;
            gTimeReference = lTimeNow;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(lTimeCompensation));
        }

        std::cout << "Video FPS:" << unsigned(gVideoFrameCounter - lVideoFrameCounter) << std::endl;
        lVideoFrameCounter = gVideoFrameCounter;
    }

    //Will never execute
    return EXIT_SUCCESS;
}
