#include "mpegts_demuxer.h"
#include "common.h"

namespace mpegts {

MpegTsDemuxer::MpegTsDemuxer()
        : mPmtId(0), mPcrId(0) {

}

MpegTsDemuxer::~MpegTsDemuxer() = default;

uint8_t MpegTsDemuxer::decode(SimpleBuffer &rIn) {
    if (mRestData.size()) {
        rIn.prepend(mRestData.data(),mRestData.size());
        mRestData.clear();
    }
    while ((rIn.size() - rIn.pos()) >= 188 ) {
        int lPos = rIn.pos();
        TsHeader lTsHeader;
        lTsHeader.decode(rIn);

        // found pat & get pmt pid
        if (lTsHeader.mPid == 0 && mPmtId == 0) {
            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mAdaptionOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                AdaptationFieldHeader lAdaptionField;
                lAdaptionField.decode(rIn);
                rIn.skip(lAdaptionField.mAdaptationFieldLength > 0 ? (lAdaptionField.mAdaptationFieldLength - 1) : 0);
            }

            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                if (lTsHeader.mPayloadUnitStartIndicator == 0x01) {
                    uint8_t lPointField = rIn.read1Byte();
                }

                mPatHeader.decode(rIn);
                rIn.read2Bytes();
                mPmtId = rIn.read2Bytes() & 0x1fff;
                mPatIsValid = true;

                if (streamInfoCallback != nullptr) {
                    mPatHeader.print(LogLevel::kTrace, streamInfoCallback);
                }
            }
        }

        // found pmt
        if (mEsFrames.empty() && mPmtId != 0 && lTsHeader.mPid == mPmtId) {
            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mAdaptionOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                AdaptationFieldHeader lAdaptionField;
                lAdaptionField.decode(rIn);
                rIn.skip(lAdaptionField.mAdaptationFieldLength > 0 ? (lAdaptionField.mAdaptationFieldLength - 1) : 0);
            }

            if (lTsHeader.mPayloadUnitStartIndicator == 0x01) {
                uint8_t lPointField = rIn.read1Byte();

                mPmtHeader.decode(rIn);
                mPcrId = mPmtHeader.mPcrPid;
                for (const std::shared_ptr<PMTElementInfo>& mInfo : mPmtHeader.mInfos) {
                    mEsFrames[mInfo->mElementaryPid] = EsFrame(mInfo->mStreamType, mInfo->mElementaryPid);

                    mStreamPidMap[mInfo->mStreamType] = mInfo->mElementaryPid;
                }
                mPmtIsValid = true;

                if (streamInfoCallback != nullptr) {
                    mPmtHeader.print(LogLevel::kTrace, streamInfoCallback);
                }
            }
        }

        if (mEsFrames.find(lTsHeader.mPid) != mEsFrames.end()) {
            uint8_t lPcrFlag = 0;
            uint64_t lPcr = 0;
            uint8_t lRandomAccessIndicator = 0;
            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mAdaptionOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                AdaptationFieldHeader lAdaptionField;
                lAdaptionField.decode(rIn);
                lRandomAccessIndicator = lAdaptionField.mRandomAccessIndicator;
                int lAdaptFieldLength = lAdaptionField.mAdaptationFieldLength;
                if (lAdaptionField.mPcrFlag == 1) {
                    lPcr = readPcr(rIn);

                    if (pcrOutCallback) {
                        pcrOutCallback(lPcr);
                    }

                    // just adjust buffer pos
                    lAdaptFieldLength -= 6;
                }
                rIn.skip(lAdaptFieldLength > 0 ? (lAdaptFieldLength - 1) : 0);
            }

            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {

                EsFrame& lEsFrame = mEsFrames[lTsHeader.mPid];
                if (lTsHeader.mPayloadUnitStartIndicator == 0x01) {
                    // TODO: Transport streams with no PES packet size set on video streams will not output the very
                    // last frame, since there will be no final packet with mPayloadUnitStartIndicator set. Add a flush
                    // function to the demuxer API that the user can call once it has ran out of input data.

                    if (!lEsFrame.mData->empty()) {
                        // We have an old packet with data, flush and reset it before starting a new one
                        if (esOutCallback) {
                            if (lEsFrame.mExpectedPesPacketLength == 0) {
                                lEsFrame.mCompleted = true;
                            } else {
                                // Expected length is set, but we didn't get enough data, deliver what we have as broken
                                lEsFrame.mBroken = true;
                            }
                            esOutCallback(lEsFrame);
                        }
                        lEsFrame.reset();
                    }

                    // Potential old package has been flushed, set random access indicator and start new package
                    lEsFrame.mRandomAccess = lRandomAccessIndicator;

                    PESHeader lPesHeader;
                    lPesHeader.decode(rIn);
                    lEsFrame.mStreamId = lPesHeader.mStreamId;
                    lEsFrame.mExpectedPesPacketLength = lPesHeader.mPesPacketLength;
                    if (lPesHeader.mPtsDtsFlags == 0x02) {
                        lEsFrame.mPts = lEsFrame.mDts = readPts(rIn);
                    } else if (lPesHeader.mPtsDtsFlags == 0x03) {
                        lEsFrame.mPts = readPts(rIn);
                        // readPts function reads a "timestamp", so we use it for DTS as well
                        lEsFrame.mDts = readPts(rIn);
                    }
                    if (lPesHeader.mPesPacketLength != 0) {

                        int payloadLength = lPesHeader.mPesPacketLength - 3 -
                                            lPesHeader.mHeaderDataLength;

                        lEsFrame.mExpectedPayloadLength = payloadLength;

                        if (payloadLength + rIn.pos() > 188 || payloadLength < 0) {
                            lEsFrame.mData->append(rIn.data() + rIn.pos(),
                                                   188 - (rIn.pos() - lPos));
                        } else {
                            lEsFrame.mData->append(rIn.data() + rIn.pos(),
                                                   lPesHeader.mPesPacketLength - 3 - lPesHeader.mHeaderDataLength);
                        }

                        if (lEsFrame.mData->size() == payloadLength) {
                            if (esOutCallback) {
                                lEsFrame.mCompleted = true;
                                esOutCallback(lEsFrame);
                            }
                            lEsFrame.reset();
                        }

                        rIn.skip(188 - (rIn.pos() - lPos));
                        continue;
                    }
                }

                if (lEsFrame.mExpectedPesPacketLength != 0 &&
                    lEsFrame.mData->size() + 188 - (rIn.pos() - lPos) >
                    lEsFrame.mExpectedPesPacketLength) {

                    uint8_t *dataPosition = rIn.data() + rIn.pos();
                    int size = lEsFrame.mExpectedPesPacketLength - lEsFrame.mData->size();
                    lEsFrame.mData->append(dataPosition,size);
                } else {
                    lEsFrame.mData->append(rIn.data() + rIn.pos(), 188 - (rIn.pos() - lPos));
                }

                //Enough data to deliver?
                if (lEsFrame.mExpectedPayloadLength == lEsFrame.mData->size()) {
                    if (esOutCallback) {
                        lEsFrame.mCompleted = true;
                        esOutCallback(lEsFrame);
                    }
                    lEsFrame.reset();
                }
            }
        } else if (mPcrId != 0 && mPcrId == lTsHeader.mPid) {
            AdaptationFieldHeader lAdaptField;
            lAdaptField.decode(rIn);
            uint64_t lPcr = readPcr(rIn);
            if (pcrOutCallback) {
                pcrOutCallback(lPcr);
            }
        }
        rIn.skip(188 - (rIn.pos() - lPos));
    }

    if (rIn.size()-rIn.pos()) {
        mRestData.append(rIn.data()+rIn.pos(),rIn.size()-rIn.pos());
    }

    rIn.clear();
    return 0;
}

}
