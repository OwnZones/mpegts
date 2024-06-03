#include "mpegts_demuxer.h"

#include "common.h"

namespace mpegts {

bool MpegTsDemuxer::checkSync(SimpleBuffer& rIn) {
    uint8_t currentSyncByte = *(rIn.currentData());

    if (rIn.bytesLeft() > kTsPacketSize_188) {
        uint8_t nextSyncByte = *(rIn.currentData() + kTsPacketSize_188);
        return currentSyncByte == kTsPacketSyncByte && nextSyncByte == kTsPacketSyncByte;
    }

    return currentSyncByte == kTsPacketSyncByte;
}

uint8_t MpegTsDemuxer::decode(SimpleBuffer &rIn) {
    if (mRestData.size()) {
        rIn.prepend(mRestData.data(), mRestData.size());
        mRestData.clear();
    }

    while (rIn.bytesLeft() >= kTsPacketSize_188) {
        if (!checkSync(rIn)) {
            // If TS packet sync is lost, skip one byte and continue until the 188 bytes packet sync is found again
            mBytesDroppedToRecoverSync++;
            rIn.skip(1);
            continue;
        }

        if (mBytesDroppedToRecoverSync > 0) {
            // Here we have recovered sync again, notify the user about the lost sync and the number of bytes dropped
            // and reset the counter and state flag.
            if (streamInfoCallback != nullptr) {
                streamInfoCallback(LogLevel::kWarning, "Lost sync in TS stream, dropped " +
                                                       std::to_string(mBytesDroppedToRecoverSync) +
                                                       " bytes to recover TS sync");
            }
            mBytesDroppedToRecoverSync = 0;
        }

        const size_t lTSPacketStartPos = rIn.pos();
        TsHeader lTsHeader;
        lTsHeader.decode(rIn);

        uint8_t lDiscontinuityIndicator = 0;

        // found pat & get pmt pid
        if (lTsHeader.mPid == 0 && mPmtId == 0) {
            if (lTsHeader.hasAdaptationField()) {
                AdaptationFieldHeader lAdaptionField;
                lAdaptionField.decode(rIn);
                lDiscontinuityIndicator = lAdaptionField.mDiscontinuityIndicator;
                rIn.skip(lAdaptionField.mAdaptationFieldLength > 0 ? (lAdaptionField.mAdaptationFieldLength - 1) : 0);
            }
            checkContinuityCounter(lTsHeader, lDiscontinuityIndicator);

            if (lTsHeader.hasPayload()) {
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
        else if (mEsFrames.empty() && mPmtId != 0 && lTsHeader.mPid == mPmtId) {
            if (lTsHeader.hasAdaptationField()) {
                AdaptationFieldHeader lAdaptionField;
                lAdaptionField.decode(rIn);
                lDiscontinuityIndicator = lAdaptionField.mDiscontinuityIndicator;
                rIn.skip(lAdaptionField.mAdaptationFieldLength > 0 ? (lAdaptionField.mAdaptationFieldLength - 1) : 0);
            }
            checkContinuityCounter(lTsHeader, lDiscontinuityIndicator);

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

        else if (mEsFrames.find(lTsHeader.mPid) != mEsFrames.end()) {
            uint8_t lPcrFlag = 0;
            uint64_t lPcr = 0;
            uint8_t lRandomAccessIndicator = 0;
            if (lTsHeader.hasAdaptationField()) {
                AdaptationFieldHeader lAdaptionField;
                lAdaptionField.decode(rIn);
                lDiscontinuityIndicator = lAdaptionField.mDiscontinuityIndicator;
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
            checkContinuityCounter(lTsHeader, lDiscontinuityIndicator);

            if (lTsHeader.hasPayload()) {

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

                        if (payloadLength + rIn.pos() > kTsPacketSize_188 || payloadLength < 0) {
                            lEsFrame.mData->append(rIn.currentData(),
                                                   kTsPacketSize_188 - (rIn.pos() - lTSPacketStartPos));
                        } else {
                            lEsFrame.mData->append(rIn.currentData(),
                                                   lPesHeader.mPesPacketLength - 3 - lPesHeader.mHeaderDataLength);
                        }

                        if (lEsFrame.mData->size() == payloadLength) {
                            if (esOutCallback) {
                                lEsFrame.mCompleted = true;
                                esOutCallback(lEsFrame);
                            }
                            lEsFrame.reset();
                        }

                        rIn.skip(kTsPacketSize_188 - (rIn.pos() - lTSPacketStartPos));
                        continue;
                    }
                }

                size_t bytesLeftInTsPacket = kTsPacketSize_188 - (rIn.pos() - lTSPacketStartPos);
                if (lEsFrame.mExpectedPayloadLength != 0 &&
                    lEsFrame.mExpectedPayloadLength != lEsFrame.mData->size() &&
                    lEsFrame.mData->size() + bytesLeftInTsPacket >
                    lEsFrame.mExpectedPayloadLength) {

                    size_t size = lEsFrame.mExpectedPayloadLength - lEsFrame.mData->size();
                    lEsFrame.mData->append(rIn.currentData(), size);
                } else {
                    lEsFrame.mData->append(rIn.currentData(), bytesLeftInTsPacket);
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
            lDiscontinuityIndicator = lAdaptField.mDiscontinuityIndicator;
            uint64_t lPcr = readPcr(rIn);
            if (pcrOutCallback) {
                pcrOutCallback(lPcr);
            }
            checkContinuityCounter(lTsHeader, lDiscontinuityIndicator);
        }
        rIn.skip(kTsPacketSize_188 - (rIn.pos() - lTSPacketStartPos));
    }

    if (rIn.dataLeft()) {
        mRestData.append(rIn.currentData(), rIn.dataLeft());
    }

    rIn.clear();
    return 0;
}

void MpegTsDemuxer::checkContinuityCounter(const TsHeader &rTsHeader, uint8_t discontinuityIndicator) {
    uint16_t pid = rTsHeader.mPid;
    if (pid == 0x1FFF) {
        // CC is undefined for null packets
        return;
    }

    auto it = mCCs.find(pid);
    if (it == mCCs.end()) {
        // First CC on this PID
        mCCs[pid] = rTsHeader.mContinuityCounter;
        return;
    }

    if (discontinuityIndicator) {
        // First CC after discontinuity
        mCCs[pid] = rTsHeader.mContinuityCounter;
        return;
    }

    uint8_t expectedCC;
    uint8_t actualCC = rTsHeader.mContinuityCounter;
    if (rTsHeader.hasPayload()) {
        // CC should have been incremented
        expectedCC = (it->second + 1) & 0xF;
    } else {
        // CC should remain the same
        expectedCC = it->second;
    }

    mCCs[pid] = actualCC;

    if (actualCC != expectedCC) {
        if (ccErrorCallback != nullptr) {
            ccErrorCallback(pid, expectedCC, actualCC);
        }
    }
}

}
