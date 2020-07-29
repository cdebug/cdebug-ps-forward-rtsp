#ifndef RTPFACTORY_H
#define RTPFACTORY_H
#include "common.h"

class RtpFactory
{
public:
    RtpFactory();
    ~RtpFactory();
    
    void parserRtpData(RtpSource*);
    int getPayloadType(RtpSource*);
    void accessPsPacket(RtpSource*, int);
    void resolvePsPacket(RtpSource*, int);
    void resolvePesPacket(RtpSource*, int);
    void getVideoFrame(RtpSource*, int);
    void getAudioFrame(RtpSource*, int);
    void appendFrameData(RtpSource*, int);
    void writeVideoFrame(RtpSource*);
    void writeAudioFrame(RtpSource*);
};

#endif //RTPFACTORY_H