#ifndef COMMON_H
#define COMMON_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <iostream>
#define SAVE_PS_FILE 1
#define SAVE_VIDEO_FILE 1
#define SAVE_AUDIO_FILE 0
#define FORWARD_RTSP 1

enum StreamType{
    VIDEO_STREAM,
    AUDIO_STREAM,
};

struct RtpSource
{
    RtpSource()
    {
        fp_ps = nullptr;
        fp_video = nullptr;
        fp_audio = nullptr;
#if SAVE_PS_FILE
	fp_ps = fopen("gb.ps", "w");
#endif // SAVE_PS_FILE
#if SAVE_VIDEO_FILE
	fp_video = fopen("gb.h264", "w");
#endif 
#if SAVE_AUDIO_FILE
	fp_audio = fopen("gb.acc", "w");
#endif 
    }
    // std::string ip;
    // int port;
    uint8_t data[4096];
    int len;
    uint32_t ssrc;
    
    FILE* fp_ps;
    FILE* fp_video;
    FILE* fp_audio;

    uint8_t frameBuff[409600];
    int frameLen;
    int frameSize;
    int curStream;
};

#endif //COMMON_H