#include "mpegts_muxer.h"
#include "crc.h"
#include "common.h"
#include <cstring>

static const uint16_t MPEGTS_NULL_PACKET_PID = 0x1FFF;
static const uint16_t MPEGTS_PAT_PID         = 0x0000;
static const uint32_t MPEGTS_PAT_INTERVAL        = 20;

namespace mpegts {

MpegTsMuxer::MpegTsMuxer(std::map<uint8_t, int> lStreamPidMap, uint16_t lPmtPid, uint16_t lPcrPid,  MuxType lType) {
    mPmtPid = lPmtPid;
    mStreamPidMap = lStreamPidMap;
    mPcrPid = lPcrPid;
    mMuxType = lType;
}

MpegTsMuxer::~MpegTsMuxer() {
}

void MpegTsMuxer::createPat(SimpleBuffer &rSb, uint16_t lPmtPid, uint8_t lCc) {
    SimpleBuffer lPatSb;
    TsHeader lTsHeader;
    lTsHeader.mSyncByte = 0x47;
    lTsHeader.mTransportErrorIndicator = 0;
    lTsHeader.mPayloadUnitStartIndicator = 1;
    lTsHeader.mTransportPriority = 0;
    lTsHeader.mPid = MPEGTS_PAT_PID;
    lTsHeader.mTransportScramblingControl = 0;
    lTsHeader.mAdaptationFieldControl = MpegTsAdaptationFieldType::mPayloadOnly;
    lTsHeader.mContinuityCounter = lCc;

    AdaptationFieldHeader lAdaptField;

    PATHeader lPatHeader;
    lPatHeader.mTableId = 0x00;
    lPatHeader.mSectionSyntaxIndicator = 1;
    lPatHeader.mB0 = 0;
    lPatHeader.mReserved0 = 0x3;
    lPatHeader.mTransportStreamId = 0;
    lPatHeader.mReserved1 = 0x3;
    lPatHeader.mVersionNumber = 0;
    lPatHeader.mCurrentNextIndicator = 1;
    lPatHeader.mSectionNumber = 0x0;
    lPatHeader.mLastSectionNumber = 0x0;

    //program_number
    uint16_t lProgramNumber = 0x0001;
    //program_map_PID
    uint16_t lProgramMapPid = 0xe000 | (lPmtPid & 0x1fff);

    unsigned int lSectionLength = 4 + 4 + 5;
    lPatHeader.mSectionLength = lSectionLength & 0x3ff;

    lTsHeader.encode(lPatSb);
    lAdaptField.encode(lPatSb);
    lPatHeader.encode(lPatSb);
    lPatSb.write2Bytes(lProgramNumber);
    lPatSb.write2Bytes(lProgramMapPid);

    // crc32
    uint32_t lCrc32 = crc32((uint8_t *)lPatSb.data() + 5, lPatSb.size() - 5);
    lPatSb.write4Bytes(lCrc32);

    std::vector<uint8_t> lStuff(188 - lPatSb.size(), 0xff);
    lPatSb.append((const uint8_t *)lStuff.data(),lStuff.size());
    rSb.append(lPatSb.data(), lPatSb.size());
}

void MpegTsMuxer::createPmt(SimpleBuffer &rSb, std::map<uint8_t, int> lStreamPidMap, uint16_t lPmtPid, uint8_t lCc) {
    SimpleBuffer lPmtSb;
    TsHeader lTsHeader;
    lTsHeader.mSyncByte = 0x47;
    lTsHeader.mTransportErrorIndicator = 0;
    lTsHeader.mPayloadUnitStartIndicator = 1;
    lTsHeader.mTransportPriority = 0;
    lTsHeader.mPid = lPmtPid;
    lTsHeader.mTransportScramblingControl = 0;
    lTsHeader.mAdaptationFieldControl = MpegTsAdaptationFieldType::mPayloadOnly;
    lTsHeader.mContinuityCounter = lCc;

    AdaptationFieldHeader lAdaptField;

    PMTHeader lPmtHeader;
    lPmtHeader.mTableId = 0x02;
    lPmtHeader.mSectionSyntaxIndicator = 1;
    lPmtHeader.mB0 = 0;
    lPmtHeader.mReserved0 = 0x3;
    lPmtHeader.mSectionLength = 0;
    lPmtHeader.mProgramNumber = 0x0001;
    lPmtHeader.mReserved1 = 0x3;
    lPmtHeader.mVersionNumber = 0;
    lPmtHeader.mCurrentNextIndicator = 1;
    lPmtHeader.mSectionNumber = 0x00;
    lPmtHeader.mLastSectionNumber = 0x00;
    lPmtHeader.mReserved2 = 0x7;
    lPmtHeader.mReserved3 = 0xf;
    lPmtHeader.mProgramInfoLength = 0;
    lPmtHeader.mPcrPid = mPcrPid;
    for (auto lIt = lStreamPidMap.begin(); lIt != lStreamPidMap.end(); lIt++) {
        lPmtHeader.mInfos.push_back(std::shared_ptr<PMTElementInfo>(new PMTElementInfo(lIt->first, lIt->second)));
    }

    uint16_t lSectionLength = lPmtHeader.size() - 3 + 4;
    lPmtHeader.mSectionLength = lSectionLength & 0x3ff;

    lTsHeader.encode(lPmtSb);
    lAdaptField.encode(lPmtSb);
    lPmtHeader.encode(lPmtSb);

    // crc32
    uint32_t crc_32 = crc32((uint8_t *)lPmtSb.data() + 5, lPmtSb.size() - 5);
    lPmtSb.write4Bytes(crc_32);

    std::vector<uint8_t> lStuff(188 - lPmtSb.size(), 0xff);
    lPmtSb.append((const uint8_t *)lStuff.data(),lStuff.size());
    rSb.append(lPmtSb.data(), lPmtSb.size());
}

void MpegTsMuxer::createPes(EsFrame &rFrame, SimpleBuffer &rSb) {
    bool lFirst = true;
    while (!rFrame.mData->empty()) {
        SimpleBuffer lPacket;

        TsHeader lTsHeader;
        lTsHeader.mPid = rFrame.mPid;
        lTsHeader.mAdaptationFieldControl = MpegTsAdaptationFieldType::mPayloadOnly;
        lTsHeader.mContinuityCounter = getCc(rFrame.mStreamType);

        if (lFirst) {
            lTsHeader.mPayloadUnitStartIndicator = 0x01;
            if (rFrame.mPid == mPcrPid) {
                lTsHeader.mAdaptationFieldControl |= 0x02;
                AdaptationFieldHeader adapt_field_header;
                adapt_field_header.mAdaptationFieldLength = 0x07;
                adapt_field_header.mRandomAccessIndicator = rFrame.mRandomAccess;
                adapt_field_header.mPcrFlag = 0x01;

                lTsHeader.encode(lPacket);
                adapt_field_header.encode(lPacket);
                writePcr(lPacket, rFrame.mPcr);
            } else if (rFrame.mRandomAccess) {
                lTsHeader.mAdaptationFieldControl |= 0x02;
                AdaptationFieldHeader adapt_field_header;
                adapt_field_header.mAdaptationFieldLength = 0x01;
                adapt_field_header.mRandomAccessIndicator = rFrame.mRandomAccess;

                lTsHeader.encode(lPacket);
                adapt_field_header.encode(lPacket);

            } else{
                lTsHeader.encode(lPacket);
            }

            PESHeader lPesHeader;
            lPesHeader.mPacketStartCode = 0x000001;
            lPesHeader.mStreamId = rFrame.mStreamId;
            lPesHeader.mMarkerBits = 0x02;
            lPesHeader.mOriginalOrCopy = 0x01;

            if (rFrame.mPts != rFrame.mDts) {
                lPesHeader.mPtsDtsFlags = 0x03;
                lPesHeader.mHeaderDataLength = 0x0A;
            } else {
                lPesHeader.mPtsDtsFlags = 0x2;
                lPesHeader.mHeaderDataLength = 0x05;
            }

            uint32_t lPesSize = (lPesHeader.mHeaderDataLength + rFrame.mData->size() + 3);
            lPesHeader.mPesPacketLength = lPesSize > 0xffff ? 0 : lPesSize;
            lPesHeader.encode(lPacket);

            if (lPesHeader.mPtsDtsFlags == 0x03) {
                writePts(lPacket, 3, rFrame.mPts);
                writePts(lPacket, 1, rFrame.mDts);
            } else {
                writePts(lPacket, 2, rFrame.mPts);
            }
        } else {
            lTsHeader.encode(lPacket);
        }

        uint32_t lPos = lPacket.size();
        uint32_t lBodySize = 188 - lPos;

        std::vector<uint8_t> lFiller(lBodySize, 0x00);
        lPacket.append((const uint8_t *)lFiller.data(),lFiller.size());

        lPacket.skip(lPos);

        uint32_t lInSize = rFrame.mData->size() - rFrame.mData->pos();
        if (lBodySize <=
            lInSize) {
            lPacket.setData(lPos, rFrame.mData->data()+rFrame.mData->pos(), lBodySize);
            rFrame.mData->skip(lBodySize);
        } else {
            uint16_t lStuffSize = lBodySize - lInSize;

            //Take care of the case 1 stuffing byte

            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mAdaptionOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                uint8_t* lpBase = lPacket.data() + 5 + lPacket.data()[4];
                    if (lFirst) {
                        //We need to move the PES header
                        size_t pesHeaderSize = lPos - (lPacket.data()[4] + 5);
                        uint8_t *pesHeader = new uint8_t[pesHeaderSize];
                        memcpy(pesHeader,lpBase,pesHeaderSize);
                        memcpy(lpBase+lStuffSize,pesHeader,pesHeaderSize);
                        delete[] pesHeader;
                    } else {
                        lPacket.setData(lpBase - lPacket.data() + lStuffSize, lpBase,
                                         lPacket.data() + lPacket.pos() - lpBase);
                    }

                   //
                    memset(lpBase, 0xff, lStuffSize);
                    lPacket.skip(lStuffSize);
                    lPacket.data()[4] += lStuffSize;

            } else {
                // adaptationFieldControl |= 0x20 == MpegTsAdaptationFieldType::payload_adaption_both
  //              std::cout << "here" << std::endl;

                if (lFirst) {
                    //we are here since there is no adaption field.. and first is set . That means we got a PES header.
                    lPacket.data()[3] |= 0x20;
                    uint8_t* lpBase = lPacket.data() + 4;
                    size_t pesHeaderSize = lPos - 4;
                    uint8_t *pesHeader = new uint8_t[pesHeaderSize];
                    memcpy(pesHeader,lpBase,pesHeaderSize);
                    memcpy(lpBase+lStuffSize,pesHeader,pesHeaderSize);
                    delete[] pesHeader;

                    lPacket.data()[4] = lStuffSize - 1;
                    if (lStuffSize >= 2) {
                        lPacket.data()[5] = 0;
                        memset(&(lPacket.data()[6]), 0xff, lStuffSize - 2);
                    }

                    lPacket.skip(lStuffSize);
                    lPacket.data()[4] = lStuffSize - 1;
                    if (lStuffSize >= 2) {
                        lPacket.data()[5] = 0;
                        memset(&(lPacket.data()[6]), 0xff, lStuffSize - 2);
                    }

                } else {
                    lPacket.data()[3] |= 0x20;
                    int lDestPosition = 188 - 4 - lStuffSize;
                    uint8_t *lSrcPosition = lPacket.data() + 4;
                    int lLength = lPacket.pos() - 4;
                    lPacket.setData(lDestPosition, lSrcPosition, lLength);
                    lPacket.skip(lStuffSize);
                    lPacket.data()[4] = lStuffSize - 1;
                    if (lStuffSize >= 2) {
                        lPacket.data()[5] = 0;
                        memset(&(lPacket.data()[6]), 0xff, lStuffSize - 2);
                    }
                }
            }
            lPacket.setData(lPacket.pos(), rFrame.mData->data()+rFrame.mData->pos(), lInSize);
            rFrame.mData->skip(lInSize);
        }

        rSb.append(lPacket.data(), lPacket.size());
        lFirst = false;
    }
}

void MpegTsMuxer::createPcr(uint64_t lPcr, uint8_t lTag) {
    std::lock_guard<std::mutex> lock(mMuxMtx);
    TsHeader lTsHeader;
    lTsHeader.mSyncByte = 0x47;
    lTsHeader.mTransportErrorIndicator = 0;
    lTsHeader.mPayloadUnitStartIndicator = 0;
    lTsHeader.mTransportPriority = 0;
    lTsHeader.mPid = mPcrPid;
    lTsHeader.mTransportScramblingControl = 0;
    lTsHeader.mAdaptationFieldControl = MpegTsAdaptationFieldType::mAdaptionOnly;
    lTsHeader.mContinuityCounter = 0;

    AdaptationFieldHeader lAdaptField;
    lAdaptField.mAdaptationFieldLength = 188 - 4 - 1;
    lAdaptField.mDiscontinuityIndicator = 0;
    lAdaptField.mRandomAccessIndicator = 0;
    lAdaptField.mElementaryStreamPriorityIndicator = 0;
    lAdaptField.mPcrFlag = 1;
    lAdaptField.mOpcrFlag = 0;
    lAdaptField.mSplicingPointFlag = 0;
    lAdaptField.mTransportPrivateDataFlag = 0;
    lAdaptField.mAdaptationFieldExtensionFlag = 0;

    SimpleBuffer lSb;
    lTsHeader.encode(lSb);
    lAdaptField.encode(lSb);
    writePcr(lSb, lPcr);
    std::vector<uint8_t> lStuff(188 - lSb.size(), 0xff);
    lSb.append((const uint8_t *)lStuff.data(),lStuff.size());
    if (tsOutCallback) {
        tsOutCallback(lSb, lTag, false);
    }
}

void MpegTsMuxer::createNull(uint8_t lTag) {
    std::lock_guard<std::mutex> lock(mMuxMtx);
    TsHeader lTsHeader;
    lTsHeader.mSyncByte = 0x47;
    lTsHeader.mTransportErrorIndicator = 0;
    lTsHeader.mPayloadUnitStartIndicator = 0;
    lTsHeader.mTransportPriority = 0;
    lTsHeader.mPid = MPEGTS_NULL_PACKET_PID;
    lTsHeader.mTransportScramblingControl = 0;
    lTsHeader.mAdaptationFieldControl = MpegTsAdaptationFieldType::mPayloadOnly;
    lTsHeader.mContinuityCounter = 0;
    SimpleBuffer lSb;
    lTsHeader.encode(lSb);
    if (tsOutCallback) {
        tsOutCallback(lSb, lTag, false);
    }
}

void MpegTsMuxer::encode(EsFrame &rFrame, uint8_t lTag, bool lRandomAccess) {
    std::lock_guard<std::mutex> lock(mMuxMtx);
    SimpleBuffer lSb;

    if (mMuxType == MpegTsMuxer::MuxType::segmentType && lRandomAccess) {
        uint8_t lPatPmtCc = getCc(0);
        createPat(lSb, mPmtPid, lPatPmtCc);
        createPmt(lSb, mStreamPidMap, mPmtPid, lPatPmtCc);
    } else if (mMuxType == MpegTsMuxer::MuxType::h222Type && shouldCreatePat()) {  //FIXME h222Type is NOT implemented correct
        uint8_t lPatPmtCc = getCc(0);
        createPat(lSb, mPmtPid, lPatPmtCc);
        createPmt(lSb, mStreamPidMap, mPmtPid, lPatPmtCc);
    }

    createPes(rFrame, lSb);
    if (tsOutCallback) {
        tsOutCallback(lSb, lTag, lRandomAccess);
    }
}

uint8_t MpegTsMuxer::getCc(uint32_t lWithPid) {
    if (mPidCcMap.find(lWithPid) != mPidCcMap.end()) {
        mPidCcMap[lWithPid] = (mPidCcMap[lWithPid] + 1) & 0x0F;
        return mPidCcMap[lWithPid];
    }

    mPidCcMap[lWithPid] = 0;
    return 0;
}

bool MpegTsMuxer::shouldCreatePat() {
    bool lRet = false;
    if (mCurrentIndex % MPEGTS_PAT_INTERVAL == 0) {
        if (mCurrentIndex > 0) {
            mCurrentIndex = 0;
        }
        lRet = true;
    }

    mCurrentIndex++;

    return lRet;
}

}
