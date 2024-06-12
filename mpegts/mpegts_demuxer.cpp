#include "mpegts_demuxer.h"

#include "common.h"

namespace mpegts {

#define LOG(level, msg) if (streamInfoCallback != nullptr) { streamInfoCallback(level, msg); }

bool MpegTsDemuxer::checkSync(SimpleBuffer& rIn) {
    uint8_t currentSyncByte = *(rIn.currentData());

    if (rIn.dataLeft() > kTsPacketSize_188) {
        uint8_t nextSyncByte = *(rIn.currentData() + kTsPacketSize_188);
        return currentSyncByte == kTsPacketSyncByte && nextSyncByte == kTsPacketSyncByte;
    }

    return currentSyncByte == kTsPacketSyncByte;
}

uint8_t MpegTsDemuxer::decode(SimpleBuffer &rIn) {
    if (!mRestData.empty()) {
        rIn.prepend(mRestData.data(), mRestData.size());
        mRestData.clear();
    }

    while (rIn.dataLeft() >= kTsPacketSize_188) {
        if (!checkSync(rIn)) {
            // If TS packet sync is lost, skip one byte and continue until the 188 bytes packet sync is found again
            mBytesDroppedToRecoverSync++;
            rIn.skip(1);
            continue;
        }

        if (mBytesDroppedToRecoverSync > 0) {
            // Here we have recovered sync again, notify the user about the lost sync and the number of bytes dropped
            // and reset the counter and state flag.
            LOG(LogLevel::kWarning, "Lost sync in TS stream, dropped " +
                                    std::to_string(mBytesDroppedToRecoverSync) +
                                    " bytes to recover TS sync");
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

    uint8_t lRandomAccessIndicator = 0;
    if (!parseAdaptationField(lTsHeader, rSb, kTsPacketSize_188 - (rSb.pos() - lTSPacketStartPos), lRandomAccessIndicator)) {
        LOG(LogLevel::kWarning, "Failed to parse adaptation field");
        return;
    }

    if (!lTsHeader.hasPayload()) {
        rSb.skip(kTsPacketSize_188 - (rSb.pos() - lTSPacketStartPos));
        return;
    }

    if (lTsHeader.mPid == kPatId && mPmtId == 0) {
        // Found PAT, parse it to get PMT pid
        if (!parsePat(lTsHeader, rSb)) {
            LOG(LogLevel::kWarning, "Failed to parse PAT");
        }
    }
    else if (mEsFrames.empty() && mPmtId != 0 && lTsHeader.mPid == mPmtId) {
        // Found PMT, parse it to get ES PIDs
        if (!parsePmt(lTsHeader, rSb)) {
            LOG(LogLevel::kWarning, "Failed to parse PMT");
        }
    }
    else if (mEsFrames.find(lTsHeader.mPid) != mEsFrames.end()) {
        // Found an ES PID, parse it, may contain a PCR as well
        if (kTsPacketSize_188 > rSb.pos() - lTSPacketStartPos) {
            size_t sizeOfPesPayload = kTsPacketSize_188 - (rSb.pos() - lTSPacketStartPos);
            if (!parsePes(lTsHeader, lRandomAccessIndicator, rSb, sizeOfPesPayload)) {
                LOG(LogLevel::kWarning, "Failed to parse PES");
            }
        }
    }

    if (kTsPacketSize_188 < rSb.pos() - lTSPacketStartPos) {
        LOG(LogLevel::kWarning, "Read more than expected data from the packet, bytes consumed: " +
                                                   std::to_string(rSb.pos() - lTSPacketStartPos) +
                                                   " expected less than 188");
    } else {
        // Some functions might not consume the whole packet, which is ok, skip the rest
        rSb.skip(kTsPacketSize_188 - (rSb.pos() - lTSPacketStartPos));
    }
}

bool MpegTsDemuxer::parseAdaptationField(const mpegts::TsHeader& rTsHeader, mpegts::SimpleBuffer& rSb, size_t bytesLeftInPacket, uint8_t& randomAccessIndicator) {
    uint8_t lDiscontinuityIndicator = 0;
    randomAccessIndicator = 0;

    if (rTsHeader.hasAdaptationField()) {
        AdaptationFieldHeader lAdaptionField;
        lAdaptionField.decode(rSb);
        lDiscontinuityIndicator = lAdaptionField.mDiscontinuityIndicator;
        randomAccessIndicator = lAdaptionField.mRandomAccessIndicator;

        if (lAdaptionField.mAdaptationFieldLength > 0) {
            bytesLeftInPacket -= 2; // Decode of adaptation field with a length > 0 consumes 2 bytes
            if (lAdaptionField.mAdaptationFieldLength > bytesLeftInPacket) {
                LOG(LogLevel::kWarning, "Adaptation field length is larger than the remaining packet size, broken packet");
                return false;
            }

            if (shouldParsePCR(rTsHeader.mPid)) {
                parsePcr(lAdaptionField, rSb);
            } else {
                // Just skip the adaptation field, -1 for the byte following the length field
                rSb.skip(lAdaptionField.mAdaptationFieldLength - 1);
            }
        }
    }
    checkContinuityCounter(rTsHeader, lDiscontinuityIndicator);

    return true;
}

bool MpegTsDemuxer::parsePat(const TsHeader& rTsHeader, SimpleBuffer &rSb) {
    if (rTsHeader.mPayloadUnitStartIndicator == 0x01) {
        uint8_t lPointField = rSb.read1Byte();
        rSb.skip(lPointField);

        mPatHeader.decode(rSb);
        if (mPatHeader.mSectionLength > 0x3FD) {
            LOG(LogLevel::kWarning, "PAT section length is too large");
            return false;
        } else if (mPatHeader.mB0 != 0 || mPatHeader.mSectionSyntaxIndicator != 1) {
            LOG(LogLevel::kWarning, "PAT section syntax is not as expected");
            return false;
        }

        uint16_t bytesLeftMinusCRC32 = mPatHeader.mSectionLength - 5 - 4; // 5 bytes for the header, 5 bytes for CRC_32
        while (bytesLeftMinusCRC32 >= 4) {
            uint16_t programNumber = rSb.read2Bytes();
            if (programNumber == 0) {
                // Network PID, skip
                rSb.skip(2);
            } else {
                mPmtId = rSb.read2Bytes() & 0x1fff;
                mPatIsValid = true;
            }
            bytesLeftMinusCRC32 -= 4;
        }

        rSb.skip(4); // Skip CRC_32

        if (streamInfoCallback != nullptr) {
            mPatHeader.print(LogLevel::kTrace, streamInfoCallback);
        }
    }
    return mPatIsValid;
}

bool MpegTsDemuxer::parsePmt(const TsHeader& rTsHeader, SimpleBuffer &rSb) {
    if (rTsHeader.mPayloadUnitStartIndicator == 0x01) {
        uint8_t lPointField = rSb.read1Byte();
        rSb.skip(lPointField);

        mPmtHeader.decode(rSb);

        if (mPmtHeader.mSectionLength > 0x3FD) {
            LOG(LogLevel::kWarning, "PMT section length is too large");
            return false;
        } else if (mPmtHeader.mB0 != 0 || mPmtHeader.mSectionSyntaxIndicator != 1) {
            LOG(LogLevel::kWarning, "PMT section syntax is not as expected");
            return false;
        }

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

    return mPmtIsValid;
}

bool MpegTsDemuxer::parsePes(const mpegts::TsHeader& rTsHeader, uint8_t randomAccessIndicator, mpegts::SimpleBuffer& rSb, size_t payloadSize) {
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
        if (lPesHeader.mMarkerBits != 0x02) {
            LOG(LogLevel::kWarning, "Corrupt PES header, marker bits not as expected");
            return false;
        }

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
            LOG(LogLevel::kWarning, "Corrupt PES header, skipping packet");
            return false;
        }
        rSb.skip(lPesHeader.mHeaderDataLength - bytesRead);
    }

    if (rSb.pos() - lPesStartPos > payloadSize) {
        LOG(LogLevel::kWarning, "Read more PES data than expected, bytes read: " +
                                std::to_string(rSb.pos() - lPesStartPos) +
                                " payload size: " + std::to_string(payloadSize));
        return false;
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

    return true;
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
