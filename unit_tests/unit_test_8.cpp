#include <functional>

#include "unit_test_8.h"
#include "unit_test_8_data.h"

#include "common.h"
#include "mpegts_demuxer.h"
#include "simple_buffer.h"
#include "ts_packet.h"

using namespace mpegts;

bool UnitTest8::runTest() {
    bool testStatus = true;

    // Map with the number of cc errors per PID
    std::map<uint16_t, uint32_t> ccErrors;

    // cc error callback
    auto ccErrorCallback = [&](uint16_t pid, uint8_t expectedCC, uint8_t actualCC) {
        auto it = ccErrors.find(pid);
        if (it == ccErrors.end()) {
            ccErrors[pid] = 0;
        }
        ccErrors[pid]++;
    };

    auto printCcErrors = [&]() {
        for (auto it : ccErrors) {
            std::cout << "PID: " << int(it.first) << "  CC errors: " << int(it.second) << std::endl;
        }
    };

    // Decode a sequence without any cc errors
    {
        MpegTsDemuxer dmx;
        dmx.ccErrorCallback = ccErrorCallback;

        SimpleBuffer inputBuffer;
        inputBuffer.append(ccTestData, ccTestDataLength);

        dmx.decode(inputBuffer);

        if (ccErrors.size() != 0) {
            std::cout << "No CC errors were expected but the following exist: " << std::endl;
            printCcErrors();
            testStatus = false;
        }

        ccErrors.clear();
    }

    // Decode a sequence with every other video packet removed
    {
        MpegTsDemuxer dmx;
        dmx.ccErrorCallback = ccErrorCallback;

        SimpleBuffer inputBuffer;

        // Add PAT and PMT
        inputBuffer.append(ccTestData, 2 * 188);

        uint32_t packetIndex = 2;
        while (packetIndex * 188 < ccTestDataLength) {
            if (packetIndex & 1) {
                inputBuffer.append(ccTestData + packetIndex * 188, 188);
            }
            packetIndex++;
        }

        dmx.decode(inputBuffer);

        if (ccErrors.size() == 0) {
            std::cout << "CC errors were expected but none were reported" << std::endl;
            testStatus = false;
        } else if (ccErrors[256] != 8) {
            std::cout << "Expected 8 CC errors on PID 256" << std::endl;
            std::cout << "Reported CC errors:" << std::endl;
            testStatus = false;
            printCcErrors();
        }

        ccErrors.clear();
    }


    return testStatus;
}
