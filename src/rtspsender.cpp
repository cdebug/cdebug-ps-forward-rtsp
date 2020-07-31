#include "rtspsender.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define RTP_MAX_PKT_SIZE        1400

RtspSender::RtspSender(std::string ip, int port)
{
    pthread_mutex_init(&m_mutex, nullptr);
    m_ip = ip;
    m_port = port;
    m_seq = 0;
    m_rtpHeader = new RtpHeader;
    rtpHeaderInit(m_rtpHeader, 2, 0 , 0, 0, 0, 96, m_seq, 0, 0);
}

int RtspSender::createUdpSocket(int localRtpPort)
{
    int fd;
    int on = 1;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0)
        return -1;
    struct sockaddr_in addr;
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(localRtpPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(fd, (sockaddr*)&addr, sizeof(addr)) == -1)
    {
        printf("bind local port error:%d\n", localRtpPort);
        close(fd);
        return -1;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return fd;
}

bool RtspSender::allocSocket(int localRtpPort)
{
    if((m_sockfd = createUdpSocket(localRtpPort)) > 0)
        return true;
    return false;
}

void RtspSender::sendData(uint8_t* data, int length, uint16_t /*cseq*/, uint32_t timestamp, uint32_t ssrc)
{
    pthread_mutex_lock(&m_mutex);
    if(data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01)
    {
        data += 3;
        length -= 3;
    }
    else if(data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01)
    {
        data += 4;
        length -= 4;
    }
    if (length <= RTP_MAX_PKT_SIZE) // nalu长度小于最大包场：单一NALU单元模式
    {
        if(m_seq == 0xFFFF)
            m_seq = 0;
        else
            m_seq++;
        m_rtpHeader->bs.seq = htons(m_seq);
        m_rtpHeader->bs.timestamp = htonl(timestamp);
        m_rtpHeader->bs.ssrc = htonl(ssrc);
        uint8_t rtpData[4096];
        int n = parserRtpData(rtpData, m_rtpHeader, data, length);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(m_port);
        addr.sin_addr.s_addr = inet_addr(m_ip.c_str());
        // printf("send n=%d\n", n);
        if(sendto(m_sockfd, rtpData, n, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            printf("send error size:%d\n", n);
    }
    else
    {
        uint8_t naluType = data[0];
        data+=1;
        length -= 1;
        uint8_t tmpData[RTP_MAX_PKT_SIZE + 10];
        int num = length % RTP_MAX_PKT_SIZE == 0 ? length / RTP_MAX_PKT_SIZE : length / RTP_MAX_PKT_SIZE + 1;
        for(int i = 0; i < num; ++i)
        {
            int pktLength = 0;
            if(i + 1 >= num)
                pktLength = length - RTP_MAX_PKT_SIZE * i;
            else
                pktLength = RTP_MAX_PKT_SIZE;
            if(m_seq == 0xFFFF)
                m_seq = 0;
            else
                m_seq++;
            m_rtpHeader->bs.seq = htons(m_seq);
            m_rtpHeader->bs.timestamp = htonl(timestamp);
            m_rtpHeader->bs.ssrc = htonl(ssrc);

            tmpData[0] = (naluType & 0xE0) | 28;
            tmpData[1] = naluType & 0x1F;
            if (i == 0) //第一包数据
                tmpData[1] |= 0x80; // start
            else if (i == num - 1) //最后一包数据
                tmpData[1] |= 0x40; // end
            memcpy(tmpData + 2, data, pktLength);
            uint8_t rtpData[4096];
            int n = parserRtpData(rtpData, m_rtpHeader, tmpData, pktLength+2);
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(m_port);
            addr.sin_addr.s_addr = inet_addr(m_ip.c_str());
            if(sendto(m_sockfd, rtpData, n, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            {
                printf("send error size:%d\n", n);
            }
            data += pktLength;
        }
    }
    pthread_mutex_unlock(&m_mutex);
}

void RtspSender::rtpHeaderInit(RtpHeader* rtp, int version, int padding, int extention, int csrcnum, 
    int mark, int pt, uint16_t cseq, uint32_t timestamp, uint32_t ssrc)
{
    rtp->bs.version = version;
    rtp->bs.padding = padding;
    rtp->bs.extension = extention;
    rtp->bs.csrcLen = csrcnum;
    rtp->bs.marker = mark;
    rtp->bs.payloadType = pt;
    rtp->bs.seq = htons(cseq);
    rtp->bs.timestamp = htonl(timestamp);
    rtp->bs.ssrc = htonl(ssrc);
}

int RtspSender::parserRtpData(uint8_t* dest, RtpHeader* rtpHeader, uint8_t* data, int len)
{
    memcpy(dest, rtpHeader->v8, 12);
    memcpy(dest+12, data,len);
    return len + 12;
}