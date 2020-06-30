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
    MpegTsMuxer(std::map<uint8_t, int> lStreamPidMap, uint16_t lPmtPid, uint16_t lPcrPid);

    virtual ~MpegTsMuxer();

    void createPat(SimpleBuffer *pSb, uint16_t lPmtPid, uint8_t lCc);

    void createPmt(SimpleBuffer *pSb, std::map<uint8_t, int> lStreamPidMap, uint16_t lPmtPid, uint8_t lCc);

    void createPes(EsFrame *pFrame, SimpleBuffer *pSb);

    void createPcr(SimpleBuffer *pSb);

    void createNull(SimpleBuffer *pSb);

    void encode(EsFrame *pFrame, SimpleBuffer *pSb);

private:
    uint8_t getCc(uint32_t lWithPid);

    bool shouldCreatePat();

    std::map<uint32_t, uint8_t> mPidCcMap;

    uint16_t mPmtPid = 0;

    std::map<uint8_t, int> mStreamPidMap;

    uint16_t mPcrPid;

};

