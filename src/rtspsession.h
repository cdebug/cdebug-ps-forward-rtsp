#ifndef RTPSESSION_H
#define RTPSESSION_H
#include <iostream>
#include <string.h>

class RtspSender;

class RtspSession
{
public:
    static RtspSession* createNew(std::string, int, int);
    void processConnection();
    uint32_t getSsrc();
    RtspSender* rtspSender();
private:
    RtspSession(std::string, int, int);
    ~RtspSession();
    char* getLineFromBuff(char* buff, char* line);
    void handleOptions(int csep);
    void handleDescribe(int cseq);
    void handleSetup(int cseq);
    void handlePlay(int cseq, char* session);
    int allocRtpSender();

    RtspSender* m_sender;
    std::string m_ip;
    int m_port;
    int m_connfd;
    int m_remoteRtpPort;
    int m_remoteRtcpPort;
    int m_localRtpPort;
    int m_localRtcpPort;
    uint32_t m_ssrc;
};

#endif //RTPSESSION_H