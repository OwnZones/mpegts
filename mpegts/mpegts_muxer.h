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

    void createPcr(SimpleBuffer &rSb);

    void createNull(SimpleBuffer &rSb);

    void encode(EsFrame &rFrame, SimpleBuffer &rSb);


private:
    uint8_t getCc(uint32_t lWithPid);

    bool shouldCreatePat();

    std::map<uint32_t, uint8_t> mPidCcMap;

    uint16_t mPmtPid = 0;

    std::map<uint8_t, int> mStreamPidMap;

    uint16_t mPcrPid;

    MuxType mMuxType = MuxType::unknown;

};

