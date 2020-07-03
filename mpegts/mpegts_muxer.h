#pragma once

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// l local scope

#include "ts_packet.h"
#include "simple_buffer.h"
#include <map>
#include <cstdint>
#include <functional>
#include <mutex>

class MpegTsMuxer {
public:
    enum MuxType: uint8_t {
        unknown,
        segmentType,
        h222Type,
        dvbType
    };

    MpegTsMuxer(std::map<uint8_t, int> lStreamPidMap, uint16_t lPmtPid, uint16_t lPcrPid, MuxType lType = MuxType::segmentType);

    virtual ~MpegTsMuxer();

    void createPat(SimpleBuffer &rSb, uint16_t lPmtPid, uint8_t lCc);

    void createPmt(SimpleBuffer &rSb, std::map<uint8_t, int> lStreamPidMap, uint16_t lPmtPid, uint8_t lCc);

    void createPes(EsFrame &rFrame, SimpleBuffer &rSb);

    void createPcr(uint64_t lPcr);

    void createNull();

    void encode(EsFrame &rFrame);

    std::function<void(SimpleBuffer &rSb)> tsOutCallback = nullptr;


private:
    uint8_t getCc(uint32_t lWithPid);

    bool shouldCreatePat();

    std::map<uint32_t, uint8_t> mPidCcMap;

    uint16_t mPmtPid = 0;

    std::map<uint8_t, int> mStreamPidMap;

    uint16_t mPcrPid;

    MuxType mMuxType = MuxType::unknown;

    std::mutex mMuxMtx;

};

