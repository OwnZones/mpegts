#include "common.h"
#include "simple_buffer.h"

void writePcr(SimpleBuffer &rSb, uint64_t lPcr) {
    rSb.write1Byte((int8_t) (lPcr >> 25));
    rSb.write1Byte((int8_t) (lPcr >> 17));
    rSb.write1Byte((int8_t) (lPcr >> 9));
    rSb.write1Byte((int8_t) (lPcr >> 1));
    rSb.write1Byte((int8_t) (lPcr << 7 | 0x7e));
    rSb.write1Byte(0);
}

void writePts(SimpleBuffer &rSb, uint32_t lFb, uint64_t lPts) {
    uint32_t lVal;

    lVal = lFb << 4 | (((lPts >> 30) & 0x07) << 1) | 1;
    rSb.write1Byte((int8_t) lVal);

    lVal = (((lPts >> 15) & 0x7fff) << 1) | 1;
    rSb.write2Bytes((int16_t) lVal);

    lVal = (((lPts) & 0x7fff) << 1) | 1;
    rSb.write2Bytes((int16_t) lVal);
}

uint64_t readPts(SimpleBuffer &rSb) {
    uint64_t lPts = 0;
    uint32_t lVal = 0;
    lVal = rSb.read1Byte();
    lPts |= ((lVal >> 1) & 0x07) << 30;

    lVal = rSb.read2Bytes();
    lPts |= ((lVal >> 1) & 0x7fff) << 15;

    lVal = rSb.read2Bytes();
    lPts |= ((lVal >> 1) & 0x7fff);

    return lPts;
}

uint64_t readPcr(SimpleBuffer &rSb) {
    uint64_t lPcr = 0;
    uint64_t lVal = rSb.read1Byte();
    lPcr |= (lVal << 25) & 0x1FE000000;

    lVal = rSb.read1Byte();
    lPcr |= (lVal << 17) & 0x1FE0000;

    lVal = rSb.read1Byte();
    lPcr |= (lVal << 9) & 0x1FE00;

    lVal = rSb.read1Byte();
    lPcr |= (lVal << 1) & 0x1FE;

    lVal = rSb.read1Byte();
    lPcr |= ((lVal >> 7) & 0x01);

    rSb.read1Byte();

    return lPcr;
}