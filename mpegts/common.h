#pragma once

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// l local scope

#include <stdint.h>

class MpegTsAdaptationFieldType {
public:
    // Reserved for future use by ISO/IEC
    static const uint8_t mReserved = 0x00;
    // No adaptation_field, payload only
    static const uint8_t mPayloadOnly = 0x01;
    // Adaptation_field only, no payload
    static const uint8_t mAdaptionOnly = 0x02;
    // Adaptation_field followed by payload
    static const uint8_t mPayloadAdaptionBoth = 0x03;
};

///
/// @brief Log levels
enum class LogLevel {
    kTrace,    // Detailed diagnostics (for development only)
    kDebug,    // Messages intended for debugging only
    kInfo,     // Messages about normal behavior (default log level)
    kWarning,  // Warnings (functionality intact)
    kError,    // Recoverable errors (functionality impaired)
    kCritical, // Unrecoverable errors (application must stop)
    kOff       // Turns off all logging
};

class SimpleBuffer;

extern void writePcr(SimpleBuffer &rSb, uint64_t lPcr);

extern void writePts(SimpleBuffer &rSb, uint32_t lFb, uint64_t lPts);

extern uint64_t readPts(SimpleBuffer &rSb);

extern uint64_t readPcr(SimpleBuffer &rSb);

