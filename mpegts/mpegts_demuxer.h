#pragma once

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// l local scope

#include "ts_packet.h"
#include "simple_buffer.h"

#include <cstdint>
#include <memory>
#include <map>
#include <functional>
#include <mutex>

#define TYPE_VIDEO_H264 0x1b
//H.265 video
#define TYPE_VIDEO_H265 0x24

// added for remux
class TSPacket {
public:
    unsigned char tsPacket[188];  // Actual TS packet data 
    bool containsVideoPES;       // Indicator if packet contains video PES
    int pesFrameNumber;          // PES frame number
    bool containsPCR;            // Indicator if packet contains PCR
    uint64_t pcrValue;          // PCR value

    // Constructor to initialize member variables
    TSPacket(const unsigned char* packet, bool hasVideoPES, int frameNum, bool hasPCR, uint64_t pcr)
        : containsVideoPES(hasVideoPES), pesFrameNumber(frameNum), containsPCR(hasPCR), pcrValue(pcr) {
        std::copy(packet, packet + 188, tsPacket);  // Copy TS packet data
    }
};

class MpegTsDemuxer {
public:
    MpegTsDemuxer();

    virtual ~MpegTsDemuxer();

    uint8_t decode(SimpleBuffer &rIn);

    // added for remux
    uint8_t decode(SimpleBuffer &rIn, std::vector<TSPacket>& packetVector, std::vector<EsFrame>& esFrameVector);

    std::function<void(const EsFrame& pEs)> esOutCallback = nullptr;
    std::function<void(uint64_t lPcr)> pcrOutCallback = nullptr;
    std::function<void(const std::string&)> streamInfoCallback = nullptr;


    // added for remux
    // Function to get EsFrame for a given PID
    EsFrame* getH264Frame() {

        int pid = mStreamPidMap[TYPE_VIDEO_H264];
        auto it = mEsFrames.find(pid);
        if (it != mEsFrames.end()) {
            return &(it->second);  // Return a pointer to the found EsFrame
        }
        return nullptr;  // Return nullptr if EsFrame not found for the given PID
    }

    // stream, pid
    std::map<uint8_t, int> mStreamPidMap;
    int mPmtId;

    // PAT
    PATHeader mPatHeader;
    bool mPatIsValid = false;

    // PMT
    PMTHeader mPmtHeader;
    bool mPmtIsValid = false;

    // added for remux
    int32_t videoFrameNumber;

private:
    // pid, Elementary data frame
    std::map<int, EsFrame> mEsFrames;
    int mPcrId;
    SimpleBuffer mRestData;
};

