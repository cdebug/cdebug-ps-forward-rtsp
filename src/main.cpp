#include "rtspconnector.h"
#include <iostream>
#include <thread>
#include "psstreamudpserver.h"

void runRtpConnector()
{
    RtspConnector::instance()->waitForConnection();
}

int main()
{
    std::thread(runRtpConnector).detach();
    PsStreamUdpServer::instance()->startListening(19444);
    while(1);
    return 0;
}