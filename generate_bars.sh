#!/bin/bash
set -e
#Script generating AAC (ADTS) + H.264 (NAL) Elementary data
rate=50
goplength=48


#Generate MPEG-TS multiplexed AAC
ffmpeg \
	-f lavfi -i "smptebars=size=1280x720:rate="$rate \
	-f lavfi -i sine=frequency=3:beep_factor=1000:sample_rate=48000 \
    -vf drawtext="fontfile=Verdana.ttf:\ timecode='00\:00\:00\:00':rate="$rate":fontsize=64:fontcolor='white':\ boxcolor=0x00000088:box=1:boxborderw=5:x=20:y=400" \
	-c:a aac -b:a 96k -ac 2\
	-c:v libx264 -preset medium -b:v 1000k -minrate 1000k -maxrate 1000k -bufsize 1500k\
	-x264opts "no-scenecut:keyint="$goplength":min-keyint="$goplength":nal-hrd=cbr:no-open-gop=1:force-cfr=1:aud=1"\
	-y -f mpegts -t 25 \
	bars.ts
