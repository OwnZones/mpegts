#include "mpegts_demuxer.h"
#include "common.h"

MpegTsDemuxer::MpegTsDemuxer()
        : mPmtId(0), mPcrId(0) {

}

MpegTsDemuxer::~MpegTsDemuxer() {
}

uint8_t MpegTsDemuxer::decode(SimpleBuffer &rIn) {
    while (!rIn.empty()) {
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
#ifdef DEBUG
                mPatHeader.print();
#endif
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
                for (size_t lI = 0; lI < mPmtHeader.mInfos.size(); lI++) {
                    mEsFrames[mPmtHeader.mInfos[lI]->mElementaryPid] = std::shared_ptr<EsFrame>(
                            new EsFrame(mPmtHeader.mInfos[lI]->mStreamType));
                    mStreamPidMap[mPmtHeader.mInfos[lI]->mStreamType] = mPmtHeader.mInfos[lI]->mElementaryPid;
                }
                mPmtIsValid = true;
#ifdef DEBUG
                mPmtHeader.print();
#endif
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
                    // just adjust buffer pos
                    lAdaptFieldLength -= 6;
                }
                rIn.skip(lAdaptFieldLength > 0 ? (lAdaptFieldLength - 1) : 0);
            }

            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                PESHeader lPesHeader;
                if (lTsHeader.mPayloadUnitStartIndicator == 0x01) {

                    mEsFrames[lTsHeader.mPid]->mRandomAccess = lRandomAccessIndicator;

                    if (mEsFrames[lTsHeader.mPid]->mCompleted) {
                        mEsFrames[lTsHeader.mPid]->reset();
                    } else if (mEsFrames[lTsHeader.mPid]->mData->size() && !mEsFrames[lTsHeader.mPid]->mCompleted) {
                        //Its a broken frame deliver that as broken
                        if (esOutCallback) {
                            EsFrame *lEsFrame = mEsFrames[lTsHeader.mPid].get();
                            lEsFrame -> mBroken = true;
                            lEsFrame -> mPid = lTsHeader.mPid;
                            esOutCallback(lEsFrame);
                        }

                        mEsFrames[lTsHeader.mPid]->reset();
                    }

                    lPesHeader.decode(rIn);
                    mEsFrames[lTsHeader.mPid]->mStreamId = lPesHeader.mStreamId;
                    mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength = lPesHeader.mPesPacketLength;
                    if (lPesHeader.mPtsDtsFlags == 0x02) {
                        mEsFrames[lTsHeader.mPid]->mPts = mEsFrames[lTsHeader.mPid]->mDts = readPts(rIn);
                    } else if (lPesHeader.mPtsDtsFlags == 0x03) {
                        mEsFrames[lTsHeader.mPid]->mPts = readPts(rIn);
                        mEsFrames[lTsHeader.mPid]->mDts = readPts(rIn);
                    }
                    if (lPesHeader.mPesPacketLength != 0) {

                        int payloadLength = lPesHeader.mPesPacketLength - 3 -
                                            lPesHeader.mHeaderDataLength;

                        mEsFrames[lTsHeader.mPid]->mExpectedPayloadLength = payloadLength;

                        if (payloadLength + rIn.pos() > 188 || payloadLength < 0) {
                            mEsFrames[lTsHeader.mPid]->mData->append(rIn.data() + rIn.pos(),
                                                                     188 - (rIn.pos() - lPos));
                        } else {
                            mEsFrames[lTsHeader.mPid]->mData->append(rIn.data() + rIn.pos(),
                                                                     lPesHeader.mPesPacketLength - 3 -
                                                                     lPesHeader.mHeaderDataLength);
                        }

                        if(mEsFrames[lTsHeader.mPid]->mData->size() == payloadLength) {
                            mEsFrames[lTsHeader.mPid]->mCompleted = true;
                            mEsFrames[lTsHeader.mPid]->mPid = lTsHeader.mPid;
                            EsFrame *lEsFrame = mEsFrames[lTsHeader.mPid].get();
                            if (esOutCallback) {
                                esOutCallback(lEsFrame);
                            }
                            mEsFrames[lTsHeader.mPid]->reset();
                        }

                        rIn.skip(188 - (rIn.pos() - lPos));
                        continue;
                    }
                }

                if (mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength != 0 &&
                    mEsFrames[lTsHeader.mPid]->mData->size() + 188 - (rIn.pos() - lPos) >
                    mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength) {

                    uint8_t *dataPosition = rIn.data() + rIn.pos();
                    int size = mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength - mEsFrames[lTsHeader.mPid]->mData->size();
                    mEsFrames[lTsHeader.mPid]->mData->append(dataPosition,size);
                } else {
                    mEsFrames[lTsHeader.mPid]->mData->append(rIn.data() + rIn.pos(), 188 - (rIn.pos() - lPos));
                }

                //Enough data to deliver?
                if (mEsFrames[lTsHeader.mPid]->mExpectedPayloadLength == mEsFrames[lTsHeader.mPid]->mData->size()) {
                    mEsFrames[lTsHeader.mPid]->mCompleted = true;
                    mEsFrames[lTsHeader.mPid]->mPid = lTsHeader.mPid;
                    EsFrame *lEsFrame = mEsFrames[lTsHeader.mPid].get();
                    if (esOutCallback) {
                        esOutCallback(lEsFrame);
                    }
                    mEsFrames[lTsHeader.mPid]->reset();
                }

            }
        } else if (mPcrId != 0 && mPcrId == lTsHeader.mPid) {
            AdaptationFieldHeader lAdaptField;
            lAdaptField.decode(rIn);
            uint64_t lPcr = readPcr(rIn);
        }
        rIn.skip(188 - (rIn.pos() - lPos));
    }
    rIn.clear();
    return 0;
}
