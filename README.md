# MPEG-TS
A simple C++ implementation of a MPEG-TS Muxer and Demuxer

This is a fork of the project as noted above. This fork adds support for adding the multiplexer / demultiplexer as a library into your projects among other features as can be seen in the examples.


**Build status ->**

![Ubuntu 18.04](https://github.com/andersc/mpegts/workflows/Ubuntu%2018.04/badge.svg)

![Windows](https://github.com/andersc/mpegts/workflows/Windows/badge.svg)

![MacOS](https://github.com/andersc/mpegts/workflows/MacOS/badge.svg)


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
        GIT_REPOSITORY https://github.com/andersc/mpegts
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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mpegts/amf0/amf0/)
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

//Create a input buffer
SimpleBuffer in;
//Create a demuxer
MpegTsDemuxer demuxer;


//Append data to the input buffer 
in.append(packet, 188);

//Demux the data
TsFrame *frame = nullptr;
demuxer.decode(&in, frame);

//If the TsFrame != nullptr there is a demuxed frame availabile
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

//Create the multiplexer
MpegTsMuxer muxer;

//Create the map defining what datatype to map to what PID
std::map<uint8_t, int> streamPidMap;
streamPidMap[TYPE_AUDIO] = AUDIO_PID;
streamPidMap[TYPE_VIDEO] = VIDEO_PID;

//Build a frame of data (ES)
TsFrame tsFrame;
tsFrame.mData = std::make_shared<SimpleBuffer>();
//Append your ES-Data
tsFrame.mData->append((const char *) rPacket->pFrameData, rPacket->mFrameSize);
tsFrame.mPts = rPacket->mPts;
tsFrame.mDts = rPacket->mPts;
tsFrame.mPcr = 0;
tsFrame.mStreamType = TYPE_AUDIO;
tsFrame.mStreamId = 192;
tsFrame.mPid = AUDIO_PID;
tsFrame.mExpectedPesPacketLength = 0;
tsFrame.mCompleted = true;

//Create your TS-Buffer
SimpleBuffer tsOutBuffer;
//Multiplex your data
muxer.encode(&tsFrame, streamPidMap, PMT_PID, &tsOutBuffer);

//The output TS packets ends up here
tsOutBuffer.size() 
tsOutBuffer.data()

```

Example usage can be found here -> [Example multiplexer/demultiplexer](https://github.com/Unit-X/ts2efp)
