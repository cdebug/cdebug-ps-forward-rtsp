#include "psstreamudpserver.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_BUFF_SIZE 4096
#define SAVE_PS_FILE 1
#define SAVE_VIDEO_FILE 1
#define SAVE_AUDIO_FILE 0
#define FORWARD_RTSP 1

PsStreamUdpServer* PsStreamUdpServer::s_instance;

PsStreamUdpServer::PsStreamUdpServer()
{

}

PsStreamUdpServer::~PsStreamUdpServer()
{

}

PsStreamUdpServer* PsStreamUdpServer::instance()
{
    if(!s_instance)
        s_instance = new PsStreamUdpServer;
    return s_instance;
}

void PsStreamUdpServer::startListening(int listenPort)
{
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd == -1)
    {
        printf("Socket init error\n");
        exit(-1);
    }
    struct sockaddr_in addr_s;  
    memset(&addr_s, 0, sizeof(addr_s));
    addr_s.sin_family = AF_INET;
    addr_s.sin_port = htons(listenPort);
    addr_s.sin_addr.s_addr = htonl(INADDR_ANY);
    /* 绑定socket */  
    if(bind(sock_fd, (struct sockaddr *)&addr_s, sizeof(addr_s)) < 0)  
    {  
        perror("bind error:");  
        exit(1);  
    }  
    char ipbuf[20];
    int n, len;
    char recv_buf[MAX_BUFF_SIZE];  
    struct sockaddr_in addr_c;
    memset(&addr_c, 0, sizeof(addr_c));
    while(1)  
    {  
        n = recvfrom(sock_fd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&addr_c, (socklen_t *)&len);  
        if(n < 0)  
        {
            perror("recvfrom error:");  
            exit(1);  
        }
        // bzero(&ipbuf,sizeof(ipbuf));
        // inet_ntop(AF_INET,&addr_c.sin_addr.s_addr,ipbuf,sizeof(ipbuf));
        if(n > 12)
        {
            uint32_t ssrc;
            memcpy((uint8_t*)&ssrc, (uint8_t*)recv_buf + 8, 4);
            ssrc = htonl(ssrc);
            RtpSource* rtp = getRtpSource(ssrc);
            memcpy(rtp->data, (uint8_t*)recv_buf, n);
            rtp->len = n;
            m_rtpFactory.parserRtpData(rtp);
        }
    }
}

RtpSource* PsStreamUdpServer::getRtpSource(uint32_t ssrc)
{
    RtpSource* rtp;
    for(int i = 0; i < m_rtpSources.size(); ++i)
    {
        rtp = m_rtpSources.at(i);
        if(rtp->ssrc == ssrc)
            return rtp;
    }
    printf("new rtp source:%d\n", ssrc);
    rtp = new RtpSource;
    rtp->ssrc = ssrc;
    m_rtpSources.push_back(rtp);
    return rtp;
}

bool PsStreamUdpServer::sourceExists(uint32_t ssrc)
{
    for(int i = 0; i < m_rtpSources.size(); ++i)
    {
        RtpSource* rtp = m_rtpSources.at(i);
        if(rtp->ssrc == ssrc)
            return true;
    }
    return false;
}