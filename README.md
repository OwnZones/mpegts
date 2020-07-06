# MPEG-TS

**This fork is WIP.. meaning you can't trust API's or examples untill this line of text is gone**

A simple C++ implementation of a MPEG-TS Muxer and Demuxer.

The multiplexer and demultiplexer is **FAR** from supporting ITU-T H.222. Only the basics are supported meaning a video stream and a audio stream without using any 'trick mode' flags or extensions. Enough for multiplexing some elementary streams to watch using FFPLAY for example. This code should not be used to do anything advanced as it's not suited for that.

This is a [cloned](https://github.com/akanchi/mpegts) projet. This clone adds support for using the multiplexer / demultiplexer as a library into your projects among other features as can be seen in the examples and unit tests.

The upstream master project also contains a lot of serious bugs fixed in this clone. There might still be bugs in the code please let me know if you find any.


**Build status ->**

![Ubuntu 18.04](https://github.com/Unit-X/mpegts/workflows/Ubuntu%2018.04/badge.svg)

![Windows](https://github.com/Unit-X/mpegts/workflows/Windows/badge.svg)

![MacOS](https://github.com/Unit-X/mpegts/workflows/MacOS/badge.svg)

**Unit tests status ->**

![Unit tests](https://github.com/Unit-X/mpegts/workflows/Unit%20tests/badge.svg)


# Build

Requires cmake version >= **3.10** and **C++14**

**Release:**

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

***Debug:***

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug
```

Outputs the mpegts static library and the example executables 

# Invoke into your CMake project

It's simple to add the MPEG-TS multiplexer/demultiplexer into your existing CMake project. Just follow the simple steps below.

**Add the mpegts library as a external project ->**

```
include(ExternalProject)
ExternalProject_Add(project_mpegts
        GIT_REPOSITORY https://github.com/Unit-X/mpegts.git
        GIT_SUBMODULES ""
        UPDATE_COMMAND git pull
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mpegts
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mpegts
        GIT_PROGRESS 1
        CONFIGURE_COMMAND cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ${CMAKE_CURRENT_SOURCE_DIR}/mpegts
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/mpegts --config ${CMAKE_BUILD_TYPE} --target mpegts
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )
add_library(mpegts STATIC IMPORTED)
set_property(TARGET mpegts PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/mpegts/libmpegts.a)
```

**Add header search paths ->**

```
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mpegts/mpegts/)
```

**Link against the library ->**

```
add_executable((your_executable) (source_files))
add_dependencies((your_executable) project_mpegts)
target_link_libraries((your_executable) mpegts)
```


That will trigger building and linking the mpegts library
(For windows the path and name of the library needs to be changed)


# Usage

Demuxer ->

```cpp

#include "mpegts_demuxer.h"

//Callback where the demuxed data ends up
void dmxOutput(EsFrame *pEs) {
//The EsFrame contains all information about the Elementary data
}


//Create a input buffer
SimpleBuffer lIn;
//Create a demuxer
MpegTsDemuxer lDemuxer;

//Provide a callback for the ES data
lDemuxer.esOutCallback = std::bind(&dmxOutput, std::placeholders::_1);

//Append data to the input buffer 
lIn.append(packet, 188);

//Demux the data
demuxer.decode(&lIn);

//Example usage of the demuxer can be seen here ->
//https://github.com/Unit-X/ts2efp/blob/master/main.cpp


```

Muxer ->
 
```cpp

#include "mpegts_muxer.h"

//AAC audio
#define TYPE_AUDIO 0x0f
//H.264 video
#define TYPE_VIDEO 0x1b

//Audio PID
#define AUDIO_PID 257
//Video PID
#define VIDEO_PID 256
//PMT PID
#define PMT_PID 100

//A callback where all the TS-packets are sent from the multiplexer
void muxOutput(SimpleBuffer &rTsOutBuffer){

}

//Create the map defining what datatype to map to what PID
std::map<uint8_t, int> streamPidMap;
streamPidMap[TYPE_AUDIO] = AUDIO_PID;
streamPidMap[TYPE_VIDEO] = VIDEO_PID;

//Create the multiplexer
//param1 = PID map
//param2 = PMT PID 
//param3 = PCR PID
MpegTsMuxer lMuxer(streamPidMap, PMT_PID, VIDEO_PID);

//Provide the callback where TS packets are fed to
lMuxer.tsOutCallback = std::bind(&muxOutput, std::placeholders::_1);

//Build a frame of data (ES)
EsFrame esFrame;
esFrame.mData = std::make_shared<SimpleBuffer>();
//Append your ES-Data
esFrame.mData->append((const char *) rPacket->pFrameData, rPacket->mFrameSize);
esFrame.mPts = rPacket->mPts;
esFrame.mDts = rPacket->mPts;
esFrame.mPcr = 0;
esFrame.mStreamType = TYPE_AUDIO;
esFrame.mStreamId = 192;
esFrame.mPid = AUDIO_PID;
esFrame.mExpectedPesPacketLength = 0;
esFrame.mCompleted = true;

//Multiplex your data
lMuxer.encode(&esFrame);

```

# Runing the included demux/mux example

(Currently only MacOS and Linux)

The tests has to be run in the order as described below

* Generate 'bars.ts' file by running the script *./generate_bars.sh* . This generates the file to be used by the tests. The build instructions has to be followed also since the tests expect the file to ba available from the location ../bars.ts meaning the executables needs to be a directory level lower IE.. {source_root}/build/{test_file_location}

* Run the demuxer *./mpeg_ts_dmx_tests* . The demuxer is demuxing all ES frames and generates one file per audio/video frame in the directory **bars_dmx**

* Run the muxer *./mpeg_ts_mx_tests*. The multiplexer is assembling the frames from the demuxed files and generates a TS over UDP output on local host port 8100. In order to view the output you can for example use *ffplay udp://127.0.0.1:8100*



   

Example usage of the mux library can alse be seen here -> [Example multiplexer/demultiplexer](https://github.com/Unit-X/ts2efp)
