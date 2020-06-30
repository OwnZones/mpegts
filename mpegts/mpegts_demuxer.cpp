#include "mpegts_demuxer.h"
#include "common.h"

MpegTsDemuxer::MpegTsDemuxer()
        : mPmtId(0), mPcrId(0) {

}

MpegTsDemuxer::~MpegTsDemuxer() {
}

int MpegTsDemuxer::decode(SimpleBuffer &rIn, EsFrame *&prOut) {
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
                    uint8_t lPointField = rIn.read_1byte();
                }

                mPatHeader.decode(rIn);
                rIn.read_2bytes();
                mPmtId = rIn.read_2bytes() & 0x1fff;
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
                uint8_t lPointField = rIn.read_1byte();

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
            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mAdaptionOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                AdaptationFieldHeader lAdaptionField;
                lAdaptionField.decode(rIn);
                int lAdaptFieldLength = lAdaptionField.mAdaptationFieldLength;
                lPcrFlag = lAdaptionField.mPcrFlag;
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
                    if (mEsFrames[lTsHeader.mPid]->mCompleted) {
                        mEsFrames[lTsHeader.mPid]->reset();
                    }

                    if (!mEsFrames[lTsHeader.mPid]->empty()) {
                        mEsFrames[lTsHeader.mPid]->mCompleted = true;
                        mEsFrames[lTsHeader.mPid]->mPid = lTsHeader.mPid;
                        prOut = mEsFrames[lTsHeader.mPid].get();
                        // got the frame, reset pos
                        rIn.skip(lPos - rIn.pos());
                        return mEsFrames[lTsHeader.mPid]->mStreamType;
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
                        if ((lPesHeader.mPesPacketLength - 3 - lPesHeader.mHeaderDataLength) >= 188 ||
                            (lPesHeader.mPesPacketLength - 3 - lPesHeader.mHeaderDataLength) < 0) {
                            mEsFrames[lTsHeader.mPid]->mData->append(rIn.data() + rIn.pos(),
                                                                     188 - (rIn.pos() - lPos));
                        } else {
                            mEsFrames[lTsHeader.mPid]->mData->append(rIn.data() + rIn.pos(),
                                                                     lPesHeader.mPesPacketLength - 3 -
                                                                     lPesHeader.mHeaderDataLength);
                        }

                        rIn.skip(188 - (rIn.pos() - lPos));
                        continue;
                    }
                }

                if (mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength != 0 &&
                    mEsFrames[lTsHeader.mPid]->mData->size() + 188 - (rIn.pos() - lPos) >
                    mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength) {
                    mEsFrames[lTsHeader.mPid]->mData->append(rIn.data() + rIn.pos(),
                                                             mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength -
                                                             mEsFrames[lTsHeader.mPid]->mData->size());
                } else {
                    mEsFrames[lTsHeader.mPid]->mData->append(rIn.data() + rIn.pos(), 188 - (rIn.pos() - lPos));
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
