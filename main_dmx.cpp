#include <iostream>
#include <memory>
#include <fstream>
#include "mpegts_muxer.h"
#include "mpegts_demuxer.h"

//AAC audio
#define TYPE_AUDIO 0x0f
//H.264 video
#define TYPE_VIDEO 0x1b

int main(int argc, char *argv[])
{
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
        std::cout << " AAC Frame";
      } else if (frame->mStreamType == TYPE_VIDEO) {
        std::cout << " H.264 frame";
      }
      std::cout << std::endl;
    }
  }
  return EXIT_SUCCESS;
}
