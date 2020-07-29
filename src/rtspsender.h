#ifndef RTSPSENDER_H
#define RTSPSENDER_H
#include <iostream>
#include <string.h>

union RtpHeader
{
    uint8_t v8[12];
    struct{
        uint8_t csrcLen:4;
        uint8_t extension:1;
        uint8_t padding:1;
        uint8_t version:2;

        uint8_t payloadType:7;
        uint8_t marker:1;
        uint16_t seq;
        uint32_t timestamp;
        uint32_t ssrc;
    }bs;
};

class RtspSender
{
public:
    RtspSender(std::string, int);
    ~RtspSender() = default;
    bool allocSocket(int);
    int createUdpSocket(int);
    void sendData(uint8_t*, int length, uint16_t, uint32_t, uint32_t);

private:
    void rtpHeaderInit(RtpHeader* rtp, int version, int padding, int extention, int csrcnum, 
        int mark, int pt, uint16_t cseq, uint32_t timestamp, uint32_t ssrc);
    int parserRtpData(uint8_t*, RtpHeader*, uint8_t*, int);

    std::string m_ip;
    int m_port;
    int m_sockfd;
    uint16_t m_seq;
};

#endif //RTSPSENDER_H