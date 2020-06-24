#include <iostream>
#include <memory>
#include <fstream>
#include "mpegts_muxer.h"
#include "mpegts_demuxer.h"

//AAC (ADTS) audio
#define TYPE_AUDIO 0x0f
//H.264 video
#define TYPE_VIDEO_H264 0x1b
//H.265 video
#define TYPE_VIDEO_H265 0x24

int main(int argc, char *argv[]) {
    std::cout << "TS - demuxlib test " << std::endl;
    std::ifstream ifile("../bars.ts", std::ios::binary | std::ios::in);
    char packet[188] = {0};
    SimpleBuffer in;
    MpegTsDemuxer demuxer;
    while (!ifile.eof()) {
        ifile.read(packet, 188);
        in.append(packet, 188);
        TsFrame *frame = nullptr;
        demuxer.decode(&in, frame);

        if (frame) {
            std::cout << "GOT: " << unsigned(frame->mStreamType);
            if (frame->mStreamType == TYPE_AUDIO) {
                std::cout << " AAC Frame, PID: " << unsigned(frame->mPid);
            } else if (frame->mStreamType == TYPE_VIDEO_H264) {
                std::cout << " H.264 , PID: " << unsigned(frame->mPid);
            } else if (frame->mStreamType == TYPE_VIDEO_H265) {
                std::cout << " H.265 frame, PID: " << unsigned(frame->mPid);
            } else {
                std::cout << " streamType: " << unsigned(frame->mStreamType) << " streamId: "
                          << unsigned(frame->mStreamId) << " PID:" << unsigned(frame->mPid);
            }
            std::cout << std::endl;
        }
    }
    return EXIT_SUCCESS;
}
