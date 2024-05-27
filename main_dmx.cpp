#include <iostream>
#include <fstream>
#include <sstream>
#include "mpegts_demuxer.h"

//AAC (ADTS) audio
#define TYPE_AUDIO 0x0f
//H.264 video
#define TYPE_VIDEO_H264 0x1b
//H.265 video
#define TYPE_VIDEO_H265 0x24

using namespace mpegts;

MpegTsDemuxer gDemuxer;

int aFrameCounter = {1};
int vFrameCounter = {1};

void dmxOutput(const EsFrame &esFrame) {

        if (esFrame.mStreamType == TYPE_AUDIO) {
            std::cout << " AAC Frame, PID: " << unsigned(esFrame.mPid) << " PTS:" << unsigned(esFrame.mPts) << std::endl;
            size_t lDataPointer = 0;

            //Break out the individual ADTS frames
            do {
                uint8_t b1= esFrame.mData->data()[lDataPointer];
                uint8_t b2= esFrame.mData->data()[lDataPointer + 1] & 0xf0;
                if (b1 == 0xff && b2 == 0xf0) {
                    if ((esFrame.mData->data()[lDataPointer + 1] & 0x08)) {
                        std::cout << std::endl;
                        std::cout << "Error -> Not MPEG-4 ADTS Audio " << std::endl;
                        break;
                    }
                } else {
                    std::cout << std::endl;
                    std::cout << "Error -> ADTS sync word missing " << std::endl;
                    break;
                }

                b1 = esFrame.mData->data()[lDataPointer + 3] & 0x3;
                b2 = esFrame.mData->data()[lDataPointer + 4];
                uint8_t b3 = esFrame.mData->data()[lDataPointer + 5];
                uint32_t lLength = (((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | (uint32_t)b3) >> 5;

                std::string aFilename;
                std::ostringstream aFilenameBld;
                aFilenameBld << "bars_dmx/af" << aFrameCounter++ << ".adts";
                aFilename = aFilenameBld.str();
                std::cout << " Filename: " << aFilename  << std::endl;
                std::ofstream oAFile(aFilename, std::ofstream::binary | std::ofstream::out);
                oAFile.write((const char *)esFrame.mData->data() + lDataPointer, lLength);
                oAFile.close();
                lDataPointer += lLength;
            } while (lDataPointer < esFrame.mData->size());
        } else if (esFrame.mStreamType == TYPE_VIDEO_H264) {
            std::cout << " H.264 , PID: " << unsigned(esFrame.mPid) << " PTS:" << unsigned(esFrame.mPts) << " DTS:" << unsigned(esFrame.mDts) << std::endl;

            std::string vFilename;
            std::ostringstream vFilenameBld;
            vFilenameBld << "bars_dmx/vf" << vFrameCounter << ".h264";
            vFilename = vFilenameBld.str();
            std::cout << " Filename: " << vFilename  << std::endl;
            std::ofstream oVFile(vFilename, std::ofstream::binary | std::ofstream::out);
            oVFile.write((const char *)esFrame.mData->data(), esFrame.mData->size());
            oVFile.close();

            std::string ptsDtsFilename;
            std::ostringstream ptsDtsFilenameBld;
            ptsDtsFilenameBld << "bars_dmx/ptsdts" << vFrameCounter++ << ".txt";
            ptsDtsFilename = ptsDtsFilenameBld.str();
            std::cout << " Filename: " << ptsDtsFilename  << std::endl;
            std::ofstream oPtsDtsFile(ptsDtsFilename);
            oPtsDtsFile << std::to_string(esFrame.mPts) << std::endl;
            oPtsDtsFile << std::to_string(esFrame.mDts) << std::endl;
            oPtsDtsFile.close();


        } else if (esFrame.mStreamType == TYPE_VIDEO_H265) {
            std::cout << " H.265 frame, PID: " << unsigned(esFrame.mPid) << std::endl;
        } else {
            std::cout << " streamType: " << unsigned(esFrame.mStreamType) << " streamId: "
                      << unsigned(esFrame.mStreamId) << " PID:" << unsigned(esFrame.mPid) << std::endl;
        }

}

int main(int argc, char *argv[]) {
    std::cout << "TS - demuxlib test " << std::endl;
    gDemuxer.esOutCallback = std::bind(&dmxOutput, std::placeholders::_1);
    std::ifstream ifile("bars.ts", std::ios::binary | std::ios::in);
    uint8_t packet[188] = {0};
    SimpleBuffer in;

    while (!ifile.eof()) {
        ifile.read((char*)&packet[0], 188);
        in.append(packet, 188);
        gDemuxer.decode(in);
    }
    ifile.close();
    return EXIT_SUCCESS;
}
