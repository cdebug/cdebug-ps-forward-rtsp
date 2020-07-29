#ifndef PSSTREAMUDPSERVER_H
#define PSSTREAMUDPSERVER_H
#include <iostream>
#include <string.h>
#include <vector>
#include "rtpfactory.h"
#include "common.h"

class PsStreamUdpServer
{
public:
    static PsStreamUdpServer* instance();
    void startListening(int);
    bool sourceExists(uint32_t);
private:
    PsStreamUdpServer();
    ~PsStreamUdpServer();
    RtpSource* getRtpSource(uint32_t);

    static PsStreamUdpServer* s_instance;
    std::vector<RtpSource*> m_rtpSources;
    RtpFactory m_rtpFactory;
};

#endif //PSSTREAMUDPSERVER_H