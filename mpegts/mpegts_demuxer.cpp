#include "mpegts_demuxer.h"
#include "common.h"

MpegTsDemuxer::MpegTsDemuxer()
        : mPmtId(0), mPcrId(0) {

}

MpegTsDemuxer::~MpegTsDemuxer() {
}

int MpegTsDemuxer::decode(SimpleBuffer *pIn, EsFrame *&prOut) {
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
        if (mEsFrames.empty() && mPmtId != 0 && lTsHeader.mPid == mPmtId) {
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
                    if (mEsFrames[lTsHeader.mPid]->mCompleted) {
                        mEsFrames[lTsHeader.mPid]->reset();
                    }

                    if (!mEsFrames[lTsHeader.mPid]->empty()) {
                        mEsFrames[lTsHeader.mPid]->mCompleted = true;
                        mEsFrames[lTsHeader.mPid]->mPid = lTsHeader.mPid;
                        prOut = mEsFrames[lTsHeader.mPid].get();
                        // got the frame, reset pos
                        pIn->skip(lPos - pIn->pos());
                        return mEsFrames[lTsHeader.mPid]->mStreamType;
                    }

                    lPesHeader.decode(pIn);
                    mEsFrames[lTsHeader.mPid]->mStreamId = lPesHeader.mStreamId;
                    mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength = lPesHeader.mPesPacketLength;
                    if (lPesHeader.mPtsDtsFlags == 0x02) {
                        mEsFrames[lTsHeader.mPid]->mPts = mEsFrames[lTsHeader.mPid]->mDts = readPts(pIn);
                    } else if (lPesHeader.mPtsDtsFlags == 0x03) {
                        mEsFrames[lTsHeader.mPid]->mPts = readPts(pIn);
                        mEsFrames[lTsHeader.mPid]->mDts = readPts(pIn);
                    }
                    if (lPesHeader.mPesPacketLength != 0) {
                        if ((lPesHeader.mPesPacketLength - 3 - lPesHeader.mHeaderDataLength) >= 188 ||
                            (lPesHeader.mPesPacketLength - 3 - lPesHeader.mHeaderDataLength) < 0) {
                            mEsFrames[lTsHeader.mPid]->mData->append(pIn->data() + pIn->pos(),
                                                                     188 - (pIn->pos() - lPos));
                        } else {
                            mEsFrames[lTsHeader.mPid]->mData->append(pIn->data() + pIn->pos(),
                                                                     lPesHeader.mPesPacketLength - 3 -
                                                                     lPesHeader.mHeaderDataLength);
                        }

                        pIn->skip(188 - (pIn->pos() - lPos));
                        continue;
                    }
                }

                if (mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength != 0 &&
                    mEsFrames[lTsHeader.mPid]->mData->size() + 188 - (pIn->pos() - lPos) >
                    mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength) {
                    mEsFrames[lTsHeader.mPid]->mData->append(pIn->data() + pIn->pos(),
                                                             mEsFrames[lTsHeader.mPid]->mExpectedPesPacketLength -
                                                             mEsFrames[lTsHeader.mPid]->mData->size());
                } else {
                    mEsFrames[lTsHeader.mPid]->mData->append(pIn->data() + pIn->pos(), 188 - (pIn->pos() - lPos));
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
