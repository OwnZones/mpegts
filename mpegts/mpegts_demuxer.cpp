#include "mpegts_demuxer.h"
#include "common.h"

MpegTsDemuxer::MpegTsDemuxer()
        : mPmtId(0), mPcrId(0) {

}

MpegTsDemuxer::~MpegTsDemuxer() {
}

int MpegTsDemuxer::decode(SimpleBuffer *pIn, TsFrame *&prOut) {
    while (!pIn->empty()) {
        int lPos = pIn->pos();
        TsHeader lTsHeader;
        lTsHeader.decode(pIn);

        // found pat & get pmt pid
        if (lTsHeader.mPid == 0 && mPmtId == 0) {
            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mAdaptionOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                AdaptationFieldHeader lAdaptionField;
                lAdaptionField.decode(pIn);
                pIn->skip(lAdaptionField.mAdaptationFieldLength > 0 ? (lAdaptionField.mAdaptationFieldLength - 1) : 0);
            }

            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                if (lTsHeader.mPayloadUnitStartIndicator == 0x01) {
                    uint8_t lPointField = pIn->read_1byte();
                }

                mPatHeader.decode(pIn);
                pIn->read_2bytes();
                mPmtId = pIn->read_2bytes() & 0x1fff;
                mPatIsValid = true;
#ifdef DEBUG
                mPatHeader.print();
#endif
            }
        }

        // found pmt
        if (mTsFrames.empty() && mPmtId != 0 && lTsHeader.mPid == mPmtId) {
            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mAdaptionOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                AdaptationFieldHeader lAdaptionField;
                lAdaptionField.decode(pIn);
                pIn->skip(lAdaptionField.mAdaptationFieldLength > 0 ? (lAdaptionField.mAdaptationFieldLength - 1) : 0);
            }

            if (lTsHeader.mPayloadUnitStartIndicator == 0x01) {
                uint8_t lPointField = pIn->read_1byte();

                mPmtHeader.decode(pIn);
                mPcrId = mPmtHeader.mPcrPid;
                for (size_t lI = 0; lI < mPmtHeader.mInfos.size(); lI++) {
                    mTsFrames[mPmtHeader.mInfos[lI]->mElementaryPid] = std::shared_ptr<TsFrame>(
                            new TsFrame(mPmtHeader.mInfos[lI]->mStreamType));
                    mStreamPidMap[mPmtHeader.mInfos[lI]->mStreamType] = mPmtHeader.mInfos[lI]->mElementaryPid;
                }
                mPmtIsValid = true;
#ifdef DEBUG
                mPmtHeader.print();
#endif
            }
        }

        if (mTsFrames.find(lTsHeader.mPid) != mTsFrames.end()) {
            uint8_t lPcrFlag = 0;
            uint64_t lPcr = 0;
            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mAdaptionOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                AdaptationFieldHeader lAdaptionField;
                lAdaptionField.decode(pIn);
                int lAdaptFieldLength = lAdaptionField.mAdaptationFieldLength;
                lPcrFlag = lAdaptionField.mPcrFlag;
                if (lAdaptionField.mPcrFlag == 1) {
                    lPcr = readPcr(pIn);
                    // just adjust buffer pos
                    lAdaptFieldLength -= 6;
                }
                pIn->skip(lAdaptFieldLength > 0 ? (lAdaptFieldLength - 1) : 0);
            }

            if (lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadOnly ||
                lTsHeader.mAdaptationFieldControl == MpegTsAdaptationFieldType::mPayloadAdaptionBoth) {
                PESHeader lPesHeader;
                if (lTsHeader.mPayloadUnitStartIndicator == 0x01) {
                    if (mTsFrames[lTsHeader.mPid]->mCompleted) {
                        mTsFrames[lTsHeader.mPid]->reset();
                    }

                    if (!mTsFrames[lTsHeader.mPid]->empty()) {
                        mTsFrames[lTsHeader.mPid]->mCompleted = true;
                        mTsFrames[lTsHeader.mPid]->mPid = lTsHeader.mPid;
                        prOut = mTsFrames[lTsHeader.mPid].get();
                        // got the frame, reset pos
                        pIn->skip(lPos - pIn->pos());
                        return mTsFrames[lTsHeader.mPid]->mStreamType;
                    }

                    lPesHeader.decode(pIn);
                    mTsFrames[lTsHeader.mPid]->mStreamId = lPesHeader.mStreamId;
                    mTsFrames[lTsHeader.mPid]->mExpectedPesPacketLength = lPesHeader.mPesPacketLength;
                    if (lPesHeader.mPtsDtsFlags == 0x02) {
                        mTsFrames[lTsHeader.mPid]->mPts = mTsFrames[lTsHeader.mPid]->mDts = readPts(pIn);
                    } else if (lPesHeader.mPtsDtsFlags == 0x03) {
                        mTsFrames[lTsHeader.mPid]->mPts = readPts(pIn);
                        mTsFrames[lTsHeader.mPid]->mDts = readPts(pIn);
                    }
                    if (lPesHeader.mPesPacketLength != 0) {
                        if ((lPesHeader.mPesPacketLength - 3 - lPesHeader.mHeaderDataLength) >= 188 ||
                            (lPesHeader.mPesPacketLength - 3 - lPesHeader.mHeaderDataLength) < 0) {
                            mTsFrames[lTsHeader.mPid]->mData->append(pIn->data() + pIn->pos(),
                                                                     188 - (pIn->pos() - lPos));
                        } else {
                            mTsFrames[lTsHeader.mPid]->mData->append(pIn->data() + pIn->pos(),
                                                                     lPesHeader.mPesPacketLength - 3 -
                                                                     lPesHeader.mHeaderDataLength);
                        }

                        pIn->skip(188 - (pIn->pos() - lPos));
                        continue;
                    }
                }

                if (mTsFrames[lTsHeader.mPid]->mExpectedPesPacketLength != 0 &&
                    mTsFrames[lTsHeader.mPid]->mData->size() + 188 - (pIn->pos() - lPos) >
                    mTsFrames[lTsHeader.mPid]->mExpectedPesPacketLength) {
                    mTsFrames[lTsHeader.mPid]->mData->append(pIn->data() + pIn->pos(),
                                                             mTsFrames[lTsHeader.mPid]->mExpectedPesPacketLength -
                                                             mTsFrames[lTsHeader.mPid]->mData->size());
                } else {
                    mTsFrames[lTsHeader.mPid]->mData->append(pIn->data() + pIn->pos(), 188 - (pIn->pos() - lPos));
                }
            }
        } else if (mPcrId != 0 && mPcrId == lTsHeader.mPid) {
            AdaptationFieldHeader lAdaptField;
            lAdaptField.decode(pIn);
            uint64_t lPcr = readPcr(pIn);
        }
        pIn->skip(188 - (pIn->pos() - lPos));
    }
    pIn->clear();
    return 0;
}
