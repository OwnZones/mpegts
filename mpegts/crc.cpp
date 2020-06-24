#include "crc.h"
uint32_t crc32(const uint8_t *pData, int lLen) {
    uint32_t lCrc = 0xffffffff;
    for (int lI = 0; lI < lLen; lI++) {
        lCrc = (lCrc << 8) ^ crcTable[((lCrc >> 24) ^ *pData++) & 0xff];
    }
    return lCrc;
}
