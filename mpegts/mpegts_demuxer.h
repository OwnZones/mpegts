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

class MpegTsDemuxer final {
public:

    MpegTsDemuxer() = default;

    ~MpegTsDemuxer() = default;

    uint8_t decode(SimpleBuffer &rIn);

    std::function<void(const EsFrame& pEs)> esOutCallback = nullptr;
    std::function<void(uint64_t lPcr)> pcrOutCallback = nullptr;
    std::function<void(LogLevel level, const std::string&)> streamInfoCallback = nullptr;
    std::function<void(uint16_t pid, uint8_t expectedCC, uint8_t actualCC)> ccErrorCallback = nullptr;

    // stream, pid
    std::map<uint8_t, int> mStreamPidMap;
    int mPmtId = 0;

    // PAT
    PATHeader mPatHeader;
    bool mPatIsValid = false;

    // PMT
    PMTHeader mPmtHeader;
    bool mPmtIsValid = false;

private:
    // Check that the CC is incremented according to spec. ccErrorCallback is called on error.
    void checkContinuityCounter(const TsHeader &rTsHeader, uint8_t discontinuityIndicator);

    // Check that the current position in buffer is pointing to a sync byte and if the buffer is larger than a TS packet,
    // the function checks that the first byte after the TS packet is also a sync byte. Returns true if the buffer is in
    // sync with the TS packet boundaries, otherwise false.
    static bool checkSync(SimpleBuffer& rIn);

    // pid, Elementary data frame
    std::map<int, EsFrame> mEsFrames;
    int mPcrId = 0;
    SimpleBuffer mRestData;

    // Continuity counters. Map from PID to current CC value.
    std::unordered_map<uint16_t, uint8_t> mCCs;

    size_t mBytesDroppedToRecoverSync = 0; // Keep track of how many bytes were dropped to recover sync
};

}
