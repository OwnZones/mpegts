#include "common.h"
#include "simple_buffer.h"

void write_pcr(SimpleBuffer *sb, uint64_t pcr) {
    sb->write_1byte((int8_t)(pcr >> 25));
    sb->write_1byte((int8_t)(pcr >> 17));
    sb->write_1byte((int8_t)(pcr >> 9));
    sb->write_1byte((int8_t)(pcr >> 1));
    sb->write_1byte((int8_t)(pcr << 7 | 0x7e));
    sb->write_1byte(0);
}

void write_pts(SimpleBuffer *sb, uint32_t fb, uint64_t pts) {
    uint32_t val;

    val = fb << 4 | (((pts >> 30) & 0x07) << 1) | 1;
    sb->write_1byte((int8_t)val);

    val = (((pts >> 15) & 0x7fff) << 1) | 1;
    sb->write_2bytes((int16_t)val);

    val = (((pts) & 0x7fff) << 1) | 1;
    sb->write_2bytes((int16_t)val);
}

uint64_t read_pts(SimpleBuffer *sb) {
    uint64_t pts = 0;
    uint32_t val = 0;
    val = sb->read_1byte();
    pts |= ((val >> 1) & 0x07) << 30;

    val = sb->read_2bytes();
    pts |= ((val >> 1) & 0x7fff) << 15;

    val = sb->read_2bytes();
    pts |= ((val >> 1) & 0x7fff);

    return pts;
}

uint64_t read_pcr(SimpleBuffer *sb) {
    uint64_t pcr = 0;
    uint64_t val = sb->read_1byte();
    pcr |= (val << 25) & 0x1FE000000;

    val = sb->read_1byte();
    pcr |= (val << 17) & 0x1FE0000;

    val = sb->read_1byte();
    pcr |= (val << 9) & 0x1FE00;

    val = sb->read_1byte();
    pcr |= (val << 1) & 0x1FE;

    val = sb->read_1byte();
    pcr |= ((val >> 7) & 0x01);

    sb->read_1byte();

    return pcr;
}