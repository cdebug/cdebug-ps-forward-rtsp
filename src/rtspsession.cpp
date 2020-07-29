#include "rtspsession.h"
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "rtspsender.h"
#include "psstreamudpserver.h"

#define MAX_BUFF_SIZE 1024

RtspSession::RtspSession(std::string ip, int port, int connfd)
:m_ip(ip), m_port(port), m_connfd(connfd)
{
    m_sender = nullptr;
}

RtspSession::~RtspSession()
{

}

RtspSession* RtspSession::createNew(std::string ip, int port, int connfd)
{
    return new RtspSession(ip, port, connfd);
}

char* RtspSession::getLineFromBuff(char* buff, char* line)
{
    memset(line, 0, sizeof(line));
	while((*line++ = *buff++) != '\n');
    *line = '\0';
    return buff;
}

void RtspSession::handleOptions(int csep)
{
    char buff[MAX_BUFF_SIZE];
    memset(buff, 0, sizeof(buff));
    sprintf(buff, "RTSP/1.0 200 OK\r\n"
                "CSeq: %d\r\n"
                "Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n"
                "\r\n", csep);
    int n = send(m_connfd, buff, strlen(buff), 0);
}

void RtspSession::handleDescribe(int cseq)
{
    char buff[MAX_BUFF_SIZE];
    char sdp[MAX_BUFF_SIZE];
    memset(buff, 0, sizeof(buff));
    memset(sdp, 0, sizeof(sdp));
    sprintf(sdp, "v=0\r\n"
                "o=1001 1 1 in IP4 192.168.31.14\r\n"
                "t=0 0\r\n"
                "a=contol:*\r\n"
                "m=video 0 RTP/AVP 96\r\n"
                "a=rtpmap:96 H264/90000\r\n"
                "a=framerate:25\r\n"
                "a=control:track0\r\n");

    sprintf(buff, "RTSP/1.0 200 OK\r\n"
                "CSeq: %d\r\n"
                "Content-length: %d\r\n"
                "Content-type: application/sdp\r\n"
                "\r\n"
                "%s", cseq, strlen(sdp), sdp);
    int n = send(m_connfd, buff, strlen(buff), 0);
}

void RtspSession::handleSetup(int cseq)
{
    int localRtpPort = allocRtpSender();
    if(localRtpPort <= 0)
        return;
    m_localRtpPort = localRtpPort;
    char buff[MAX_BUFF_SIZE];
    memset(buff, 0, sizeof(buff));
    sprintf(buff, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
                    "Session: 66334873\r\n"
                    "\r\n",cseq, m_remoteRtpPort, m_remoteRtcpPort, m_localRtpPort, m_localRtpPort+1);
    int n = send(m_connfd, buff, strlen(buff), 0);
}

void RtspSession::handlePlay(int cseq, char* session)
{
    char buff[MAX_BUFF_SIZE];
    memset(buff, 0, sizeof(buff));
    sprintf(buff, "RTSP/1.0 200 OK\r\n"
                "CSeq: %d\r\n"
                "Range: npt=0.000-\r\n"
                "Session: %s; timeout=60\r\n"
                "\r\n",cseq, session);
    int n = send(m_connfd, buff, strlen(buff), 0);
    printf("currRtpPort=%d localRtpPort=%d\n", m_remoteRtpPort, m_localRtpPort);
}

void RtspSession::processConnection()
{
	int n, cseq, crtpPort, crtcpPort;
    char buff[MAX_BUFF_SIZE], line[MAX_BUFF_SIZE], method[40], url[100], version[40], *tmpBuf, session[20];
	while(1)
	{
		memset(buff, 0, sizeof(buff));
		n = recv(m_connfd, buff, sizeof(buff), 0);
		if(n > 0)
		{
            tmpBuf = buff;
			tmpBuf = getLineFromBuff(tmpBuf, line);
            if(sscanf(line, "%s %s %s\r\n", method, url, version) != 3)
            {
                printf("Parse error in first line\n");
                break;
            }
            char ipbuf[16];
            int port;
            if(sscanf(url, "rtsp://%[^:]:%d/%d", ipbuf, &port, &m_ssrc) != 3)
            {
                printf("url no ssrc\n");
                break;
            }
            if(!PsStreamUdpServer::instance()->sourceExists(m_ssrc))
            {
                printf("ssrc %d not exists\n", m_ssrc);
                break;
            }
            tmpBuf = getLineFromBuff(tmpBuf, line);
            if(sscanf(line, "CSeq: %d\r\n", &cseq) != 1)
            {
                printf("Parse error in second line\n");
                break;
            }
            if(strcmp("OPTIONS", method) == 0)
            {
                handleOptions(cseq);
            }
            else if(strcmp("DESCRIBE", method) == 0)
            {
                handleDescribe(cseq);
            }
            else if(strcmp("SETUP", method) == 0)
            {
                tmpBuf = getLineFromBuff(tmpBuf, line);
                if(strncmp("Transport", line, 9) != 0)
                    tmpBuf = getLineFromBuff(tmpBuf, line);
                if(strncmp("Transport", line, 9) != 0)
                    tmpBuf = getLineFromBuff(tmpBuf, line);
                if(sscanf(line, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n", &crtpPort, &crtcpPort)!=2)
                {
                    printf("Cannot get port\n");
                    break;
                }
                m_remoteRtpPort = crtpPort;
                m_remoteRtcpPort = crtcpPort;
                handleSetup(cseq);
            }
            else if(strcmp("PLAY", method) == 0)
            {
                tmpBuf = getLineFromBuff(tmpBuf, line);
                if(strncmp("Session", line, 7) != 0)
                    tmpBuf = getLineFromBuff(tmpBuf, line);
                if(strncmp("Session", line, 7) != 0)
                    tmpBuf = getLineFromBuff(tmpBuf, line);
                if(sscanf(line, "Session: %s\r\n", session)!=1)
                {
                    printf("Cannot get session\n");
                    break;
                }
                handlePlay(cseq, session);
            }
		}
        else
            break;
    }
    close(m_connfd);
}

uint32_t RtspSession::getSsrc()
{
    return m_ssrc;
}

RtspSender* RtspSession::rtspSender()
{
    return m_sender;
}

int RtspSession::allocRtpSender()
{
    if(!m_sender)
    {
        m_sender = new RtspSender(m_ip, m_remoteRtpPort);
        for(int i = 20000; i < 30000; i = i + 2)
        {
            if(m_sender->allocSocket(i))
                return i;
        }
        delete m_sender;
        m_sender = nullptr;
    }
    return -1;
}
