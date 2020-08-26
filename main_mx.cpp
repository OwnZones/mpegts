//
// Created by Anders Cedronius on 2020-06-24.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <functional>
#include "kissnet/kissnet.hpp"
#include "mpegts_muxer.h"

//If defined save TS data to file
#define SAVE_TO_FILE
#ifdef SAVE_TO_FILE
#define SAVED_TS_NUM_SECONDS 25
std::ofstream oMpegTs("../output.ts", std::ofstream::binary | std::ofstream::out);
int savedOutputSeconds = {0};
bool savingFrames = {true};
bool closeOnce = {false};
#endif

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
MpegTsMuxer *gpMuxer;

//Debug
int fRCnt = {0};

uint8_t* findNal(uint8_t *start, uint8_t *end)
{
    uint8_t *p = start;
    while (p <= end-3 && (p[0] || p[1] || p[2]!=1)) ++p;
    if (p > end-3) return nullptr;
    /* Include 8 bits leading zero */
    //if (p>start && *(p-1)==0)
    //    return (p-1);
    return p;
}

void muxOutput(SimpleBuffer &rTsOutBuffer, uint8_t lTag, bool lRandom){

    //Double to fail at non integer data and be able to visualize in the print-out
    double packets = (double)rTsOutBuffer.size() / 188.0;
    //std::cout << "Sending -> " << packets << " MPEG-TS packets" << std::endl;
    uint8_t* lpData = rTsOutBuffer.data();
    for (int lI = 0 ; lI < packets ; lI++) {
        mpegTsOut.send((const std::byte *)lpData+(lI*188), 188);
    #ifdef SAVE_TO_FILE
        if (savingFrames) {
            oMpegTs.write((char*)lpData + (lI * 188), 188);
        } else {
            if (!closeOnce) {
                oMpegTs.close();
                std::cout << "Stopped saving to file." << std::endl;
                closeOnce = true;
            }
        }
    #endif
    }
}

void fakeAudioEncoder() {
    double lAdtsTimeDistance = (1024.0/48000.0) * 1000000.0;
    double lNextTriger = (double)gTimeReference + lAdtsTimeDistance; //Meaning every 21.33333..ms from baseTime we send a adts frame
    uint64_t lFrameCounter = 0;
    uint8_t* adtsFrame;
    std::string lThisPTS;
    uint64_t lPts = gFirstPts-1920;
    uint64_t lCurrentOffset = 0;
    EsFrame lEsFrame;

    //The loop counter is there to silence my compilers infinitive loop warning.
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
                adtsFrame = new uint8_t [lFileSize];
                adtsFile.seekg (0, std::ios::beg);
                adtsFile.read ((char*)&adtsFrame[0], lFileSize);
                adtsFile.close();

                lPts +=  90000.0/(48000.0/1024.0);
                uint64_t lRecalcPts = lPts + lCurrentOffset;

                lEsFrame.mData = std::make_shared<SimpleBuffer>();
                lEsFrame.mData->append(adtsFrame, lFileSize);
                lEsFrame.mPts = lRecalcPts;
                lEsFrame.mDts = lRecalcPts;
                lEsFrame.mPcr = 0;
                lEsFrame.mStreamType = TYPE_AUDIO;
                lEsFrame.mStreamId = 192;
                lEsFrame.mPid = AUDIO_PID;
                lEsFrame.mExpectedPesPacketLength = 0;
                lEsFrame.mRandomAccess = 0x01;
                lEsFrame.mCompleted = true;

                gpMuxer->encode(lEsFrame);

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
    uint8_t* videoNal;
    std::string lThisPTS;
    std::string lThisDTS;
    uint64_t lPts = 0;
    uint64_t lDts = 0;
    uint64_t lCurrentOffset = 0;
    EsFrame lEsFrame;

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
                videoNal = new uint8_t [lFileSize];
                nalFile.seekg (0, std::ios::beg);
                nalFile.read ((char*)&videoNal[0], lFileSize);
                nalFile.close();

                uint8_t lIdrFound = 0x00;
                uint8_t *lpNalStart = (uint8_t *) videoNal;
                while (true) {
                    lpNalStart = findNal(lpNalStart, (uint8_t *) videoNal + lFileSize);
                    if (lpNalStart == nullptr) break;
                    uint8_t startcode = lpNalStart[3] & 0x1f;
                    if (startcode == 0x05) {
                        lIdrFound = 0x01;
                    }
                    lpNalStart++;
                }

                uint64_t lRecalcPts = lPts + lCurrentOffset;
                uint64_t lRecalcDts = lDts + lCurrentOffset;

                lEsFrame.mData = std::make_shared<SimpleBuffer>();
                lEsFrame.mData->append(videoNal, lFileSize);
                lEsFrame.mPts = lRecalcPts;
                lEsFrame.mDts = lRecalcDts;
                lEsFrame.mPcr = lRecalcDts;
                lEsFrame.mStreamType = TYPE_VIDEO;
                lEsFrame.mStreamId = 224;
                lEsFrame.mPid = VIDEO_PID;
                lEsFrame.mExpectedPesPacketLength = 0;
                lEsFrame.mRandomAccess = lIdrFound;
                lEsFrame.mCompleted = true;

                gpMuxer->encode(lEsFrame);

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

    std::map<uint8_t, int> gStreamPidMap;
    gStreamPidMap[TYPE_AUDIO] = AUDIO_PID;
    gStreamPidMap[TYPE_VIDEO] = VIDEO_PID;
    gpMuxer = new MpegTsMuxer(gStreamPidMap, PMT_PID, VIDEO_PID);

    gpMuxer->tsOutCallback = std::bind(&muxOutput, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

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

#ifdef SAVE_TO_FILE
        if (savedOutputSeconds++ == SAVED_TS_NUM_SECONDS) {
            savingFrames = false;
        }
#endif
    }

    //Will never execute
   delete gpMuxer;

    return EXIT_SUCCESS;
}
