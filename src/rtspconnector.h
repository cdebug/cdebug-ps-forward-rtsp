#ifndef RTSPSCONNECTOR_H
#define RTSPSCONNECTOR_H
#include <iostream>
#include <vector>
#include <stdint.h>

class RtspSession;

class RtspConnector
{
public:
    static RtspConnector* instance();
    RtspConnector() = default;
    ~RtspConnector() = default;
    void waitForConnection();
    char* getLineFromBuff(char* buff, char* line);
    void handleOptions(int connfd, int csep);
    void handleDescribe(int connfd, int cseq);
    void handleSetup(int connfd, int cseq, int crtpPort, int crtcpPort);
    void handlePlay(int connfd, int cseq, char* session, uint32_t ssrc);
    void processConnection(int connfd);
    void sendRtspData(uint8_t*, int , uint16_t, uint32_t, uint32_t);
    bool ssrcMatchSender(uint32_t);
private:
    static RtspConnector* s_instance;
    std::vector<RtspSession*> m_rtspSessiones;
};

#endif //RTSPSCONNECTOR_H