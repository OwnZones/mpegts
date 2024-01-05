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

class MpegTsDemuxer {
public:
    MpegTsDemuxer();

    virtual ~MpegTsDemuxer();

    uint8_t decode(SimpleBuffer &rIn);

    std::function<void(const EsFrame *pEs)> esOutCallback = nullptr;
    std::function<void(uint64_t lPcr)> pcrOutCallback = nullptr;
    std::function<void(const std::string&)> streamInfoCallback = nullptr;

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
    std::map<int, std::shared_ptr<EsFrame>> mEsFrames;
    int mPcrId;
    SimpleBuffer mRestData;
};

