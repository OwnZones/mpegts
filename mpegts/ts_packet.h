#pragma once

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// l local scope

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class SimpleBuffer;

class EsFrame final {
public:
    EsFrame();

    EsFrame(uint8_t streamType, uint16_t pid);

    ~EsFrame() = default;

    bool empty();

    void reset();

    std::shared_ptr<SimpleBuffer> mData;
    uint64_t mPts = 0;
    uint64_t mDts = 0;
    uint64_t mPcr = 0;
    uint8_t mRandomAccess = 0;
    uint8_t mStreamType = 0;
    uint8_t mStreamId = 0;
    uint16_t mPid = 0;
    uint16_t mExpectedPesPacketLength = 0;
    uint16_t mExpectedPayloadLength = 0;
    bool mCompleted = false;
    bool mBroken = false;
};

class TsHeader final {
public:
    TsHeader();

    ~TsHeader();

    void encode(SimpleBuffer& rSb);

    void decode(SimpleBuffer& rSb);

    uint8_t mSyncByte = 0x47;                // 8 bits
    uint8_t mTransportErrorIndicator = 0;    // 1 bit
    uint8_t mPayloadUnitStartIndicator = 0;  // 1 bit
    uint8_t mTransportPriority = 0;          // 1 bit
    uint16_t mPid = 0;                       // 13 bits
    uint8_t mTransportScramblingControl = 0; // 2 bits
    uint8_t mAdaptationFieldControl = 0;     // 2 bits
    uint8_t mContinuityCounter = 0;          // 4 bits
};

class PATHeader final {
public:
    PATHeader();

    ~PATHeader();

    void encode(SimpleBuffer& rSb);

    void decode(SimpleBuffer& rSb);

    void print(const std::function<void(const std::string&)>& streamInfoCallback);

    uint8_t mTableId = 0;                // 8 bits
    uint8_t mSectionSyntaxIndicator = 0; // 1 bit
    uint8_t mB0 = 0;                     // 1 bit
    uint8_t mReserved0 = 0;              // 2 bits
    uint16_t mSectionLength = 0;         // 12 bits
    uint16_t mTransportStreamId = 0;     // 16 bits
    uint8_t mReserved1 = 0;              // 2 bits
    uint8_t mVersionNumber = 0;          // 5 bits
    uint8_t mCurrentNextIndicator = 0;   // 1 bit
    uint8_t mSectionNumber = 0;          // 8 bits
    uint8_t mLastSectionNumber = 0;      // 8 bits
};

class PMTElementInfo final {
public:
    PMTElementInfo();

    PMTElementInfo(uint8_t lSt, uint16_t lPid);

    ~PMTElementInfo();

    void encode(SimpleBuffer& rSb);

    void decode(SimpleBuffer& rSb);

    uint16_t size();

    void print(const std::function<void(const std::string&)>& streamInfoCallback);

    uint8_t mStreamType = 0;     // 8 bits
    uint8_t mReserved0 = 0x7;    // 3 bits
    uint16_t mElementaryPid = 0; // 13 bits
    uint8_t mReserved1 = 0xf;    // 4 bits
    uint16_t mEsInfoLength = 0;  // 12 bits
    std::string mEsInfo;
};

class PMTHeader final {
public:
    PMTHeader();

    ~PMTHeader();

    void encode(SimpleBuffer& rSb);

    void decode(SimpleBuffer& rSb);

    uint16_t size();

    void print(const std::function<void(const std::string&)>& streamInfoCallback);

    uint8_t mTableId = 0x02;             // 8 bits
    uint8_t mSectionSyntaxIndicator = 0; // 1 bit
    uint8_t mB0 = 0;                     // 1 bit
    uint8_t mReserved0 = 0;              // 2 bits
    uint16_t mSectionLength = 0;         // 12 bits
    uint16_t mProgramNumber = 0;         // 16 bits
    uint8_t mReserved1 = 0;              // 2 bits
    uint8_t mVersionNumber = 0;          // 5 bits
    uint8_t mCurrentNextIndicator = 0;   // 1 bit
    uint8_t mSectionNumber = 0;          // 8 bits
    uint8_t mLastSectionNumber = 0;      // 8 bits
    uint8_t mReserved2 = 0;              // 3 bits
    uint16_t mPcrPid = 0;                // 13 bits
    uint8_t mReserved3 = 0;              // 4 bits
    uint16_t mProgramInfoLength = 0;     // 12 bits
    std::vector<std::shared_ptr<PMTElementInfo>> mInfos;
};

class AdaptationFieldHeader final {
public:
    AdaptationFieldHeader();

    ~AdaptationFieldHeader();

    void encode(SimpleBuffer& rSb);

    void decode(SimpleBuffer& rSb);

    uint8_t mAdaptationFieldLength = 0;             // 8 bits
    uint8_t mAdaptationFieldExtensionFlag = 0;      // 1 bit
    uint8_t mTransportPrivateDataFlag = 0;          // 1 bit
    uint8_t mSplicingPointFlag = 0;                 // 1 bit
    uint8_t mOpcrFlag = 0;                          // 1 bit
    uint8_t mPcrFlag = 0;                           // 1 bit
    uint8_t mElementaryStreamPriorityIndicator = 0; // 1 bit
    uint8_t mRandomAccessIndicator = 0;             // 1 bit
    uint8_t mDiscontinuityIndicator = 0;            // 1 bit
};

class PESHeader final {
public:
    PESHeader();

    ~PESHeader();

    void encode(SimpleBuffer& rSb);

    void decode(SimpleBuffer& rSb);

    uint32_t mPacketStartCode = 0x000001; // 24 bits
    uint8_t mStreamId = 0;                // 8 bits
    uint16_t mPesPacketLength = 0;        // 16 bits
    uint8_t mOriginalOrCopy = 0;          // 1 bit
    uint8_t mCopyright = 0;               // 1 bit
    uint8_t mDataAlignmentIndicator = 0;  // 1 bit
    uint8_t mPesPriority = 0;             // 1 bit
    uint8_t mPesScramblingControl = 0;    // 2 bits
    uint8_t mMarkerBits = 0x02;           // 2 bits
    uint8_t mPesExtFlag = 0;              // 1 bit
    uint8_t mPesCrcFlag = 0;              // 1 bit
    uint8_t mAddCopyInfoFlag = 0;         // 1 bit
    uint8_t mDsmTrickModeFlag = 0;        // 1 bit
    uint8_t mEsRateFlag = 0;              // 1 bit
    uint8_t mEscrFlag = 0;                // 1 bit
    uint8_t mPtsDtsFlags = 0;             // 2 bits
    uint8_t mHeaderDataLength = 0;        // 8 bits
};
