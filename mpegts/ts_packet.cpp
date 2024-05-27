#include "ts_packet.h"
#include "simple_buffer.h"

#include <iostream>

namespace mpegts {

EsFrame::EsFrame() {
    mData = std::make_shared<SimpleBuffer>();
}

EsFrame::EsFrame(uint8_t streamType, uint16_t pid) : mStreamType(streamType), mPid(pid) {
    mData = std::make_shared<SimpleBuffer>();
}

bool EsFrame::empty() {
    return mData->size() == 0;
}

void EsFrame::reset() {
    mPts = 0;
    mDts = 0;
    mPcr = 0;
    mRandomAccess = 0;
    mExpectedPesPacketLength = 0;
    mExpectedPayloadLength = 0;
    mCompleted = false;
    mBroken = false;
    mData = std::make_shared<SimpleBuffer>();
}

TsHeader::TsHeader() = default;

TsHeader::~TsHeader() = default;

void TsHeader::encode(SimpleBuffer& rSb) {
    rSb.write1Byte(mSyncByte);

    uint16_t lB1b2 = mPid & 0x1FFF;
    lB1b2 |= (mTransportPriority << 13) & 0x2000;
    lB1b2 |= (mPayloadUnitStartIndicator << 14) & 0x4000;
    lB1b2 |= (mTransportErrorIndicator << 15) & 0x8000;
    rSb.write2Bytes(lB1b2);

    uint8_t lB3 = mContinuityCounter & 0x0F;
    lB3 |= (mAdaptationFieldControl << 4) & 0x30;
    lB3 |= (mTransportScramblingControl << 6) & 0xC0;
    rSb.write1Byte(lB3);
}

void TsHeader::decode(SimpleBuffer& rSb) {
    mSyncByte = rSb.read1Byte();

    uint16_t lB1b2 = rSb.read2Bytes();
    mPid = lB1b2 & 0x1FFF;
    mTransportErrorIndicator = (lB1b2 >> 13) & 0x01;
    mPayloadUnitStartIndicator = (lB1b2 >> 14) & 0x01;
    mTransportErrorIndicator = (lB1b2 >> 15) & 0x01;

    uint8_t lB3 = rSb.read1Byte();
    mContinuityCounter = lB3 & 0x0F;
    mAdaptationFieldControl = (lB3 >> 4) & 0x03;
    mTransportScramblingControl = (lB3 >> 6) & 0x03;
}

PATHeader::PATHeader() = default;

PATHeader::~PATHeader() = default;

void PATHeader::encode(SimpleBuffer& rSb) {
    rSb.write1Byte(mTableId);

    uint16_t lB1b2 = mSectionLength & 0x0FFF;
    lB1b2 |= (mReserved0 << 12) & 0x3000;
    lB1b2 |= (mB0 << 14) & 0x4000;
    lB1b2 |= (mSectionSyntaxIndicator << 15) & 0x8000;
    rSb.write2Bytes(lB1b2);

    rSb.write2Bytes(mTransportStreamId);

    uint8_t lB5 = mCurrentNextIndicator & 0x01;
    lB5 |= (mVersionNumber << 1) & 0x3E;
    lB5 |= (mReserved1 << 6) & 0xC0;
    rSb.write1Byte(lB5);

    rSb.write1Byte(mSectionNumber);
    rSb.write1Byte(mLastSectionNumber);
}

void PATHeader::decode(SimpleBuffer& rSb) {
    mTableId = rSb.read1Byte();

    uint16_t lB1b2 = rSb.read2Bytes();
    mSectionSyntaxIndicator = (lB1b2 >> 15) & 0x01;
    mB0 = (lB1b2 >> 14) & 0x01;
    mSectionLength = lB1b2 & 0x0FFF;

    mTransportStreamId = rSb.read2Bytes();

    uint8_t lB5 = rSb.read1Byte();
    mReserved1 = (lB5 >> 6) & 0x03;
    mVersionNumber = (lB5 >> 1) & 0x1F;
    mCurrentNextIndicator = lB5 & 0x01;

    mSectionNumber = rSb.read1Byte();

    mLastSectionNumber = rSb.read1Byte();
}

void PATHeader::print(LogLevel logLevel,
                      const std::function<void(LogLevel level, const std::string&)>& streamInfoCallback) {
    streamInfoCallback(logLevel, "----------PAT information----------");
    streamInfoCallback(logLevel, "table_id: " + std::to_string(mTableId));
    streamInfoCallback(logLevel, "section_syntax_indicator: " + std::to_string(mSectionSyntaxIndicator));
    streamInfoCallback(logLevel, "b0: " + std::to_string(mB0));
    streamInfoCallback(logLevel, "reserved0: " + std::to_string(mReserved0));
    streamInfoCallback(logLevel, "section_length: " + std::to_string(mSectionLength));
    streamInfoCallback(logLevel, "transport_stream_id: " + std::to_string(mTransportStreamId));
    streamInfoCallback(logLevel, "reserved1: " + std::to_string(mReserved1));
    streamInfoCallback(logLevel, "version_number: " + std::to_string(mVersionNumber));
    streamInfoCallback(logLevel, "current_next_indicator: " + std::to_string(mCurrentNextIndicator));
    streamInfoCallback(logLevel, "section_number: " + std::to_string(mSectionNumber));
    streamInfoCallback(logLevel, "last_section_number: " + std::to_string(mLastSectionNumber));
}

PMTElementInfo::PMTElementInfo(uint8_t lSt, uint16_t lPid)
        : mStreamType(lSt), mElementaryPid(lPid) {

}

PMTElementInfo::PMTElementInfo()
        : PMTElementInfo(0, 0) {
}

PMTElementInfo::~PMTElementInfo() = default;

void PMTElementInfo::encode(SimpleBuffer& rSb) {
    rSb.write1Byte(mStreamType);

    uint16_t lB1b2 = mElementaryPid & 0x1FFF;
    lB1b2 |= (mReserved0 << 13) & 0xE000;
    rSb.write2Bytes(lB1b2);

    int16_t lB3b4 = mEsInfoLength & 0x0FFF;
    lB3b4 |= (mReserved1 << 12) & 0xF000;
    rSb.write2Bytes(lB3b4);

    if (mEsInfoLength > 0) {
        // TODO:
    }
}

void PMTElementInfo::decode(SimpleBuffer& rSb) {
    mStreamType = rSb.read1Byte();

    uint16_t lB1b2 = rSb.read2Bytes();
    mReserved0 = (lB1b2 >> 13) & 0x07;
    mElementaryPid = lB1b2 & 0x1FFF;

    uint16_t lB3b4 = rSb.read2Bytes();
    mReserved1 = (lB3b4 >> 12) & 0xF;
    mEsInfoLength = lB3b4 & 0xFFF;

    if (mEsInfoLength > 0) {
        mEsInfo = rSb.readString(mEsInfoLength);
    }
}

uint16_t PMTElementInfo::size() {
    return 5 + mEsInfoLength;
}

void PMTElementInfo::print(LogLevel logLevel,
                           const std::function<void(LogLevel logLevel, const std::string&)>& streamInfoCallback) {
    streamInfoCallback(logLevel, "**********PMTElement information**********");
    streamInfoCallback(logLevel, "stream_type: " + std::to_string(mStreamType));
    streamInfoCallback(logLevel, "reserved0: " + std::to_string(mReserved0));
    streamInfoCallback(logLevel, "elementary_PID: " + std::to_string(mElementaryPid));
    streamInfoCallback(logLevel, "reserved1: " + std::to_string(mReserved1));
    streamInfoCallback(logLevel, "ES_info_length: " + std::to_string(mEsInfoLength));
    streamInfoCallback(logLevel, "ES_info: " + mEsInfo);
}

PMTHeader::PMTHeader() = default;

PMTHeader::~PMTHeader() = default;

void PMTHeader::encode(SimpleBuffer& rSb) {
    rSb.write1Byte(mTableId);

    uint16_t lB1b2 = mSectionLength & 0xFFFF;
    lB1b2 |= (mReserved0 << 12) & 0x3000;
    lB1b2 |= (mB0 << 14) & 0x4000;
    lB1b2 |= (mSectionSyntaxIndicator << 15) & 0x8000;
    rSb.write2Bytes(lB1b2);

    rSb.write2Bytes(mProgramNumber);

    uint8_t lB5 = mCurrentNextIndicator & 0x01;
    lB5 |= (mVersionNumber << 1) & 0x3E;
    lB5 |= (mReserved1 << 6) & 0xC0;
    rSb.write1Byte(lB5);

    rSb.write1Byte(mSectionNumber);
    rSb.write1Byte(mLastSectionNumber);

    uint16_t lB8b9 = mPcrPid & 0x1FFF;
    lB8b9 |= (mReserved2 << 13) & 0xE000;
    rSb.write2Bytes(lB8b9);

    uint16_t lB10b11 = mProgramInfoLength & 0xFFF;
    lB10b11 |= (mReserved3 << 12) & 0xF000;
    rSb.write2Bytes(lB10b11);

    for (std::shared_ptr<PMTElementInfo>& mInfo : mInfos) {
        mInfo->encode(rSb);
    }
}

void PMTHeader::decode(SimpleBuffer& rSb) {
    mTableId = rSb.read1Byte();

    uint16_t lB1b2 = rSb.read2Bytes();
    mSectionSyntaxIndicator = (lB1b2 >> 15) & 0x01;
    mB0 = (lB1b2 >> 14) & 0x01;
    mReserved0 = (lB1b2 >> 12) & 0x03;
    mSectionLength = lB1b2 & 0xFFF;

    mProgramNumber = rSb.read2Bytes();

    uint8_t lB5 = rSb.read1Byte();
    mReserved1 = (lB5 >> 6) & 0x03;
    mVersionNumber = (lB5 >> 1) & 0x1F;
    mCurrentNextIndicator = lB5 & 0x01;

    mSectionNumber = rSb.read1Byte();
    mLastSectionNumber = rSb.read1Byte();

    uint16_t lB8b9 = rSb.read2Bytes();
    mReserved2 = (lB8b9 >> 13) & 0x07;
    mPcrPid = lB8b9 & 0x1FFF;

    uint16_t lB10b11 = rSb.read2Bytes();
    mReserved3 = (lB10b11 >> 12) & 0xF;
    mProgramInfoLength = lB10b11 & 0xFFF;

    if (mProgramInfoLength > 0) {
        rSb.readString(mProgramInfoLength);
    }

    int lRemainBytes = mSectionLength - 4 - 9 - mProgramInfoLength;
    while (lRemainBytes > 0) {
        std::shared_ptr<PMTElementInfo> element_info(new PMTElementInfo);
        element_info->decode(rSb);
        mInfos.push_back(element_info);
        lRemainBytes -= element_info->size();
    }
}

uint16_t PMTHeader::size() {
    uint16_t lRet = 12;
    for (std::shared_ptr<PMTElementInfo>& mInfo : mInfos) {
        lRet += mInfo->size();
    }

    return lRet;
}

void PMTHeader::print(LogLevel logLevel,
                      const std::function<void(LogLevel logLevel, const std::string&)>& streamInfoCallback) {
    streamInfoCallback(logLevel, "----------PMT information----------");
    streamInfoCallback(logLevel, "table_id: " + std::to_string(mTableId));
    streamInfoCallback(logLevel, "section_syntax_indicator: " + std::to_string(mSectionSyntaxIndicator));
    streamInfoCallback(logLevel, "b0: " + std::to_string(mB0));
    streamInfoCallback(logLevel, "reserved0: " + std::to_string(mReserved0));
    streamInfoCallback(logLevel, "section_length: " + std::to_string(mSectionLength));
    streamInfoCallback(logLevel, "program_number: " + std::to_string(mProgramNumber));
    streamInfoCallback(logLevel, "reserved1: " + std::to_string(mReserved1));
    streamInfoCallback(logLevel, "version_number: " + std::to_string(mVersionNumber));
    streamInfoCallback(logLevel, "current_next_indicator: " + std::to_string(mCurrentNextIndicator));
    streamInfoCallback(logLevel, "section_number: " + std::to_string(mSectionNumber));
    streamInfoCallback(logLevel, "last_section_number: " + std::to_string(mLastSectionNumber));
    streamInfoCallback(logLevel, "reserved2: " + std::to_string(mReserved2));
    streamInfoCallback(logLevel, "PCR_PID: " + std::to_string(mPcrPid));
    streamInfoCallback(logLevel, "reserved3: " + std::to_string(mReserved3));
    streamInfoCallback(logLevel, "program_info_length: " + std::to_string(mProgramInfoLength));
    for (std::shared_ptr<PMTElementInfo>& mInfo : mInfos) {
        mInfo->print(logLevel, streamInfoCallback);
    }
}

AdaptationFieldHeader::AdaptationFieldHeader() = default;

AdaptationFieldHeader::~AdaptationFieldHeader() = default;

void AdaptationFieldHeader::encode(SimpleBuffer& rSb) {
    rSb.write1Byte(mAdaptationFieldLength);
    if (mAdaptationFieldLength != 0) {
        uint8_t lVal = mAdaptationFieldExtensionFlag & 0x01;
        lVal |= (mTransportPrivateDataFlag << 1) & 0x02;
        lVal |= (mSplicingPointFlag << 2) & 0x04;
        lVal |= (mOpcrFlag << 3) & 0x08;
        lVal |= (mPcrFlag << 4) & 0x10;
        lVal |= (mElementaryStreamPriorityIndicator << 5) & 0x20;
        lVal |= (mRandomAccessIndicator << 6) & 0x40;
        lVal |= (mDiscontinuityIndicator << 7) & 0x80;
        rSb.write1Byte(lVal);
    }
}

void AdaptationFieldHeader::decode(SimpleBuffer& rSb) {
    mAdaptationFieldLength = rSb.read1Byte();
    if (mAdaptationFieldLength != 0) {
        uint8_t lVal = rSb.read1Byte();
        mAdaptationFieldExtensionFlag = lVal & 0x01;
        mTransportPrivateDataFlag = (lVal >> 1) & 0x01;
        mSplicingPointFlag = (lVal >> 2) & 0x01;
        mOpcrFlag = (lVal >> 3) & 0x01;
        mPcrFlag = (lVal >> 4) & 0x01;
        mElementaryStreamPriorityIndicator = (lVal >> 5) & 0x01;
        mRandomAccessIndicator = (lVal >> 6) & 0x01;
        mDiscontinuityIndicator = (lVal >> 7) & 0x01;
    }
}

PESHeader::PESHeader() = default;

PESHeader::~PESHeader() = default;

void PESHeader::encode(SimpleBuffer& rSb) {
    uint32_t lB0b1b2b3 = (mPacketStartCode << 8) & 0xFFFFFF00;
    lB0b1b2b3 |= mStreamId & 0xFF;
    rSb.write4Bytes(lB0b1b2b3);

    rSb.write2Bytes(mPesPacketLength);

    uint8_t lB6 = mOriginalOrCopy & 0x01;
    lB6 |= (mCopyright << 1) & 0x02;
    lB6 |= (mDataAlignmentIndicator << 2) & 0x04;
    lB6 |= (mPesPriority << 3) & 0x08;
    lB6 |= (mPesScramblingControl << 4) & 0x30;
    lB6 |= (mMarkerBits << 6) & 0xC0;
    rSb.write1Byte(lB6);

    uint8_t lB7 = mPesExtFlag & 0x01;
    lB7 |= (mPesCrcFlag << 1) & 0x02;
    lB7 |= (mAddCopyInfoFlag << 2) & 0x04;
    lB7 |= (mDsmTrickModeFlag << 3) & 0x08;
    lB7 |= (mEsRateFlag << 4) & 0x10;
    lB7 |= (mEscrFlag << 5) & 0x20;
    lB7 |= (mPtsDtsFlags << 6) & 0xC0;
    rSb.write1Byte(lB7);

    rSb.write1Byte(mHeaderDataLength);
}

void PESHeader::decode(SimpleBuffer& rSb) {
    uint32_t lB0b1b2b3 = rSb.read4Bytes();
    mPacketStartCode = (lB0b1b2b3 >> 8) & 0x00FFFFFF;
    mStreamId = (lB0b1b2b3) & 0xFF;

    mPesPacketLength = rSb.read2Bytes();

    uint8_t lB6 = rSb.read1Byte();
    mOriginalOrCopy = lB6 & 0x01;
    mCopyright = (lB6 >> 1) & 0x01;
    mDataAlignmentIndicator = (lB6 >> 2) & 0x01;
    mPesPriority = (lB6 >> 3) & 0x01;
    mPesScramblingControl = (lB6 >> 4) & 0x03;
    mMarkerBits = (lB6 >> 6) & 0x03;

    uint8_t lB7 = rSb.read1Byte();
    mPesExtFlag = lB7 & 0x01;
    mPesCrcFlag = (lB7 >> 1) & 0x01;
    mAddCopyInfoFlag = (lB7 >> 2) & 0x01;
    mDsmTrickModeFlag = (lB7 >> 3) & 0x01;
    mEsRateFlag = (lB7 >> 4) & 0x01;
    mEscrFlag = (lB7 >> 5) & 0x01;
    mPtsDtsFlags = (lB7 >> 6) & 0x03;

    mHeaderDataLength = rSb.read1Byte();
}

}
