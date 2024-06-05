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

        // We have a full TS packet starting at the current position in rIn, start parsing
        parseTsPacket(rIn);
    }

    if (rIn.dataLeft()) {
        // Save the rest of the data for next decode-call
        mRestData.append(rIn.currentData(), rIn.dataLeft());
    }

    rIn.clear();
    return 0;
}

void MpegTsDemuxer::parseTsPacket(mpegts::SimpleBuffer& rSb) {
    const size_t lTSPacketStartPos = rSb.pos();

    TsHeader lTsHeader;
    lTsHeader.decode(rSb);
    if (lTsHeader.mPid == 0x1FFF) {
        // Null packet, skip the rest
        rSb.skip(kTsPacketSize_188 - (rSb.pos() - lTSPacketStartPos));
        return;
    }

    uint8_t lRandomAccessIndicator = parseAdaptationField(lTsHeader, rSb);

    if (!lTsHeader.hasPayload()) {
        rSb.skip(kTsPacketSize_188 - (rSb.pos() - lTSPacketStartPos));
        return;
    }

    if (lTsHeader.mPid == kPatId && mPmtId == 0) {
        // Found PAT, parse it to get PMT pid
        parsePat(lTsHeader, rSb);
    }
    else if (mEsFrames.empty() && mPmtId != 0 && lTsHeader.mPid == mPmtId) {
        // Found PMT, parse it to get ES PIDs
        parsePmt(lTsHeader, rSb);
    }
    else if (mEsFrames.find(lTsHeader.mPid) != mEsFrames.end()) {
        // Found an ES PID, parse it, may contain the PCR as well
        size_t sizeOfPesPayload = kTsPacketSize_188 - (rSb.pos() - lTSPacketStartPos);
        parsePes(lTsHeader, lRandomAccessIndicator, rSb, sizeOfPesPayload);

        if (rSb.pos() - lTSPacketStartPos != kTsPacketSize_188) {
            if (streamInfoCallback != nullptr) {
                streamInfoCallback(LogLevel::kWarning, "Didn't read the packet correct, bytes consumed: " +
                                                           std::to_string(rSb.pos() - lTSPacketStartPos) +
                                                           " expected: 188");
            }
        }
    }

    if (kTsPacketSize_188 < rSb.pos() - lTSPacketStartPos) {
        if (streamInfoCallback != nullptr) {
            streamInfoCallback(LogLevel::kWarning, "Read more than expected data from the packet, bytes consumed: " +
                                                   std::to_string(rSb.pos() - lTSPacketStartPos) +
                                                   " expected less than 188");
        }
    }

    // Some functions might not consume the whole packet, skip the rest
    rSb.skip(kTsPacketSize_188 - (rSb.pos() - lTSPacketStartPos));
}

uint8_t MpegTsDemuxer::parseAdaptationField(const mpegts::TsHeader& rTsHeader, mpegts::SimpleBuffer& rSb) {
    uint8_t lDiscontinuityIndicator = 0;
    uint8_t lRandomAccessIndicator = 0;

    if (rTsHeader.hasAdaptationField()) {
        AdaptationFieldHeader lAdaptionField;
        lAdaptionField.decode(rSb);
        lDiscontinuityIndicator = lAdaptionField.mDiscontinuityIndicator;
        lRandomAccessIndicator = lAdaptionField.mRandomAccessIndicator;

        if (lAdaptionField.mAdaptationFieldLength > 0) {
            if (shouldParsePCR(rTsHeader.mPid)) {
                parsePcr(lAdaptionField, rSb);
            } else {
                // Just skip the adaptation field, -1 for the byte following the length field
                rSb.skip(lAdaptionField.mAdaptationFieldLength - 1);
            }
        }
    }
    checkContinuityCounter(rTsHeader, lDiscontinuityIndicator);

    return lRandomAccessIndicator;
}

void MpegTsDemuxer::parsePat(const TsHeader& rTsHeader, SimpleBuffer &rSb) {
    if (rTsHeader.mPayloadUnitStartIndicator == 0x01) {
        uint8_t lPointField = rSb.read1Byte();
        if (lPointField != 0x00) {
            if (streamInfoCallback != nullptr) {
                streamInfoCallback(LogLevel::kWarning, "PAT pointer field is not 0x00");
            }
        }
    }

    mPatHeader.decode(rSb);
    rSb.read2Bytes();
    mPmtId = rSb.read2Bytes() & 0x1fff;
    mPatIsValid = true;

    if (streamInfoCallback != nullptr) {
        mPatHeader.print(LogLevel::kTrace, streamInfoCallback);
    }
}

void MpegTsDemuxer::parsePmt(const TsHeader& rTsHeader, SimpleBuffer &rSb) {
    if (rTsHeader.mPayloadUnitStartIndicator == 0x01) {
        uint8_t lPointField = rSb.read1Byte();
        if (lPointField != 0x00) {
            if (streamInfoCallback != nullptr) {
                streamInfoCallback(LogLevel::kWarning, "PMT pointer field is not 0x00");
            }
        }

        mPmtHeader.decode(rSb);
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

void MpegTsDemuxer::parsePes(const mpegts::TsHeader& rTsHeader, uint8_t randomAccessIndicator, mpegts::SimpleBuffer& rSb, size_t payloadSize) {
    size_t lPesStartPos = rSb.pos();

    EsFrame& lEsFrame = mEsFrames[rTsHeader.mPid];
    if (rTsHeader.mPayloadUnitStartIndicator == 0x01) {
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
        lEsFrame.mRandomAccess = randomAccessIndicator;

        PESHeader lPesHeader;
        lPesHeader.decode(rSb);

        const size_t lPositionAfterPesHeaderDataLength = rSb.pos();

        lEsFrame.mStreamId = lPesHeader.mStreamId;
        lEsFrame.mExpectedPesPacketLength = lPesHeader.mPesPacketLength;
        if (lPesHeader.mPtsDtsFlags == 0x02) {
            lEsFrame.mPts = lEsFrame.mDts = readPts(rSb);
        } else if (lPesHeader.mPtsDtsFlags == 0x03) {
            lEsFrame.mPts = readPts(rSb);
            // readPts function reads a "timestamp", so we use it for DTS as well
            lEsFrame.mDts = readPts(rSb);
        }

        if (lPesHeader.mPesPacketLength != 0) {
            size_t payloadLength = lPesHeader.mPesPacketLength - 3 - lPesHeader.mHeaderDataLength;

            lEsFrame.mExpectedPayloadLength = payloadLength;
        }

        // Skip the rest of the header data
        size_t bytesRead = rSb.pos() - lPositionAfterPesHeaderDataLength;
        if (bytesRead > lPesHeader.mHeaderDataLength) {
            this->streamInfoCallback(LogLevel::kWarning, "Corrupt PES header, skipping packet");
            return;
        }
        rSb.skip(lPesHeader.mHeaderDataLength - bytesRead);
    }

    size_t bytesLeftOfPayload = payloadSize - (rSb.pos() - lPesStartPos);

    lEsFrame.mData->append(rSb.currentData(), bytesLeftOfPayload);
    rSb.skip(bytesLeftOfPayload);

    // Enough data to deliver?
    if (lEsFrame.mExpectedPayloadLength == lEsFrame.mData->size()) {
        if (esOutCallback) {
            lEsFrame.mCompleted = true;
            esOutCallback(lEsFrame);
        }
        lEsFrame.reset();
    }
}

void MpegTsDemuxer::parsePcr(const mpegts::AdaptationFieldHeader& rAdaptionField, mpegts::SimpleBuffer& rSb) const {
    int bytesRead = 1; // 1 byte for the fields following the length field
    if (rAdaptionField.mPcrFlag == 1) {
        uint64_t lPcr = readPcr(rSb);
        bytesRead += 6; // 6 bytes for the PCR

        if (pcrOutCallback) {
            pcrOutCallback(lPcr);
        }
    }
    // Skip rest of the adaptation field
    rSb.skip(rAdaptionField.mAdaptationFieldLength - bytesRead);
}

bool MpegTsDemuxer::shouldParsePCR(uint16_t pid) const {
    return pcrOutCallback != nullptr && mPcrId != 0 && pid == mPcrId;
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
