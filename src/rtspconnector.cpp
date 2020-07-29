#include "rtspconnector.h"
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "rtspsender.h"
#include <thread>
#include "rtspsession.h"

#define MAX_BUFF_SIZE 1024
RtspConnector* RtspConnector::s_instance = nullptr;

RtspConnector* RtspConnector::instance()
{
    if(!s_instance)
        s_instance = new RtspConnector;
    return s_instance;
}


void RtspConnector::waitForConnection()
{
    int listenfd, connfd;
	if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return;
    }
    struct sockaddr_in addr;
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8554);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if( bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1){
        printf("reciever bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        close(listenfd);
        exit(-1);
    }
    if( listen(listenfd, 10) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        close(listenfd);
        exit(-1);
    }
	struct sockaddr_in addr_c;
    socklen_t len = sizeof(addr_c);
	char ipbuf[20];
	while(1)
    {
        if((connfd = accept(listenfd, (struct sockaddr*)&addr_c, &len)) != -1)
        {
            bzero(&ipbuf,sizeof(ipbuf));
            inet_ntop(AF_INET,&addr_c.sin_addr.s_addr,ipbuf,sizeof(ipbuf));
            printf("Connection established: Ip is %s,port is %d\n",ipbuf,ntohs(addr_c.sin_port));
            RtspSession* session = RtspSession::createNew(ipbuf, ntohs(addr_c.sin_port), connfd);
			std::thread(&RtspSession::processConnection, session).detach();
            m_rtspSessiones.push_back(session);
        }
    }
    close(listenfd);
    return;
}

void RtspConnector::sendRtspData(uint8_t* data, int length, uint16_t seq, uint32_t timestamp, uint32_t ssrc)
{
    for(int i = 0; i < m_rtspSessiones.size(); ++i)
    {
        RtspSession* session = m_rtspSessiones.at(i);
        if(session->getSsrc() == ssrc)
            session->rtspSender()->sendData(data, length, seq, timestamp, ssrc);
    }
}