#ifndef STREAMRESOLVER_H
#define STREAMRESOLVER_H
#include <iostream>
#include "common.h"

class StreamResolver
{
public:
    StreamResolver(int, int);
    ~StreamResolver();
    void executeProcess();
    void accessPsPacket(uint8_t*, int);
    void resolvePsPacket(uint8_t*, int);
    void resolvePesPacket(uint8_t*, int);
    void getVideoFrame(uint8_t*, int);
    void getAudioFrame(uint8_t*, int);
    void appendFrameData(uint8_t*, int);
    void writeVideoFrame();
    void writeAudioFrame();
private:
    int m_portListen;
    int m_secsWait;
    FILE* fp_ps;
    FILE* fp_video;
    FILE* fp_audio;
    StreamType m_curStream;
};

#endif //STREAMRESOLVER_H