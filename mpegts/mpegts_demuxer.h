#pragma once

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// l local scope

#include "common.h"
#include "ts_packet.h"
#include "simple_buffer.h"

#include <cstdint>
#include <memory>
#include <map>
#include <functional>
#include <mutex>

namespace mpegts {

class MpegTsDemuxer {
public:

    MpegTsDemuxer();

    virtual ~MpegTsDemuxer();

    uint8_t decode(SimpleBuffer &rIn);

    std::function<void(const EsFrame& pEs)> esOutCallback = nullptr;
    std::function<void(uint64_t lPcr)> pcrOutCallback = nullptr;
    std::function<void(LogLevel level, const std::string&)> streamInfoCallback = nullptr;
    std::function<void(uint16_t pid, uint8_t expectedCC, uint8_t actualCC)> ccErrorCallback = nullptr;

    // stream, pid
    std::map<uint8_t, int> mStreamPidMap;
    int mPmtId;

    // PAT
    PATHeader mPatHeader;
    bool mPatIsValid = false;

    // PMT
    PMTHeader mPmtHeader;
    bool mPmtIsValid = false;

private:
    // pid, Elementary data frame
    std::map<int, EsFrame> mEsFrames;
    int mPcrId;
    SimpleBuffer mRestData;

    // Continuity counters. Map from PID to current CC value.
    std::unordered_map<uint16_t, uint8_t> mCCs;

    // Check that the CC is incremented according to spec. ccErrorCallback is called on error.
    void checkContinuityCounter(const TsHeader &rTsHeader, uint8_t discontinuityIndicator);
};

}
