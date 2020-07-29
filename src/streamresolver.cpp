#include "streamresolver.h"
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtppacket.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtpipv4address.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtperrors.h>
#include <jrtplib3/rtpsourcedata.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include "rtspconnector.h"

using namespace jrtplib;

#define PS_BUFF_SIZE 4096000
#define SAVE_PS_FILE 1
#define SAVE_VIDEO_FILE 1
#define SAVE_AUDIO_FILE 0
#define FORWARD_RTSP 1

//
// This function checks if there was a RTP error. If so, it displays an error
// message and exists.
//

uint8_t *_frameBuff;
int _frameSize;
int _buffLen;
uint16_t _seq;
uint32_t _timestamp;
uint32_t _ssrc;

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

StreamResolver::StreamResolver(int port, int secs)
:m_portListen(port),m_secsWait(secs)
{

}
StreamResolver::~StreamResolver()
{
    
}

void StreamResolver::writeVideoFrame()
{
	// printf("write video frame size=%d\n", _buffLen);
	if (_frameSize != _buffLen)
		printf("error:frameSize=%d bufflen=%d\n", _frameSize, _buffLen);
#if SAVE_VIDEO_FILE
	fwrite(_frameBuff, 1, _buffLen, fp_video);
#endif
#if FORWARD_RTSP
	RtspConnector::instance()->sendRtspData(_frameBuff, _buffLen
		, _seq, _timestamp, 0x88888888);
#endif
	memset(_frameBuff, 0, sizeof(_frameBuff));
	_buffLen = 0;
	_frameSize = 0;
}

void StreamResolver::writeAudioFrame()
{
    // printf("write audio frame size=%d\n", _buffLen);
	if (_frameSize != _buffLen)
		printf("error:frameSize=%d bufflen=%d\n", _frameSize, _buffLen);
#if SAVE_AUDIO_FILE
	fwrite(_frameBuff, 1, _buffLen, fp_audio);
#endif
	memset(_frameBuff, 0, sizeof(_frameBuff));
	_buffLen = 0;
	_frameSize = 0;
}

void StreamResolver::resolvePesPacket(uint8_t* payloadData, int payloadLength)
{
	/******  统一 帧  ******/
	while (true)
	{
        if(payloadData[0] == 0x00 && payloadData[1] == 0x00 && 
            payloadData[2] == 0x01 && payloadData[3] == 0xe0)
        {
		    getVideoFrame(payloadData, payloadLength);
		}
        else if(payloadData[0] == 0x00 && payloadData[1] == 0x00 && 
            payloadData[2] == 0x01 && payloadData[3] == 0xc0)
        {
            getAudioFrame(payloadData, payloadLength);
        }
        else
        {
            // printf("no valid stream c0 or e0\n");
            break;
        }
		uint16_t stream_size = payloadData[4] << 8 | payloadData[5];
        payloadData += 6 + stream_size;
        payloadLength -= 6 + stream_size;
        if(payloadLength < 4)
            break;
	}
}

void StreamResolver::getVideoFrame(uint8_t* payloadData, int payloadLength)
{
	memset(_frameBuff, 0, sizeof(_frameBuff));
	_buffLen = 0;
	_frameSize = 0;

    m_curStream = VIDEO_STREAM;
    int pos = 0;
    uint16_t h264_size = payloadData[4] << 8 | payloadData[5];
	uint8_t expand_size = payloadData[8];
	_frameSize = h264_size - 3 - expand_size;
	pos += 9 + expand_size;
	//全部写入并保存帧
	if (_frameSize <= payloadLength - pos)
	{
	    memcpy(_frameBuff, payloadData + pos, _frameSize);
	    _buffLen += _frameSize;
		pos += _frameSize;
		writeVideoFrame();
	}
	else
	{
	    memcpy(_frameBuff, payloadData + pos, payloadLength - pos);
	    _buffLen += payloadLength - pos;
	    // printf("Video frame size:%d\n", _frameSize);
    }
}

void StreamResolver::getAudioFrame(uint8_t* payloadData, int payloadLength)
{
    memset(_frameBuff, 0, sizeof(_frameBuff));
	_buffLen = 0;
	_frameSize = 0;

    m_curStream = AUDIO_STREAM;
    int pos = 0;
    uint16_t stream_size = payloadData[4] << 8 | payloadData[5];
	uint8_t expand_size = payloadData[8];
	_frameSize = stream_size - 3 - expand_size;
	pos += 9 + expand_size;
	//全部写入并保存帧
	if (_frameSize <= payloadLength - pos)
	{
	    memcpy(_frameBuff, payloadData + pos, _frameSize);
	    _buffLen += _frameSize;
		pos += _frameSize;
		writeAudioFrame();
	}
	else
	{
	    memcpy(_frameBuff, payloadData + pos, payloadLength - pos);
	    _buffLen += payloadLength - pos;
	    printf("Audio frame size:%d\n", _frameSize);
    }
}

void StreamResolver::resolvePsPacket(uint8_t* payloadData, int payloadLength)
{
	int pos = 0;
    if (payloadData[0] == 0x00 && payloadData[1] == 0x00 &&
		payloadData[2] == 0x01 && payloadData[3] == 0xba)
    {
        uint8_t expand_size = payloadData[13] & 0x07;//扩展字节
        pos += 14 + expand_size;//ps包头14
        /******  i 帧  ******/
        if (payloadData[pos] == 0x00 && payloadData[pos + 1] == 0x00 && 
            payloadData[pos + 2] == 0x01 && payloadData[pos + 3] == 0xbb)//0x000001bb ps system header
        {
	        uint16_t psh_size = payloadData[pos + 4] << 8 | payloadData[pos + 5];//psh长度
	        pos += 6 + psh_size;
	        if (payloadData[pos] == 0x00 && payloadData[pos + 1] == 0x00 && 
                payloadData[pos + 2] == 0x01 && payloadData[pos + 3] == 0xbc)//0x000001bc ps system map
	        {
		        uint16_t psm_size = payloadData[pos + 4] << 8 | payloadData[pos + 5];
		        pos += 6 + psm_size;
		    }
	        else
	        {
		        printf("no system map and no video stream\n");
		        return;
	        }
	    }
    }
    resolvePesPacket(payloadData + pos, payloadLength - pos);
}


void StreamResolver::executeProcess()
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2, 2), &dat);
#endif // RTP_SOCKETTYPE_WINSOCK

#if SAVE_PS_FILE
	fp_ps = fopen("gb.ps", "w");
#endif // SAVE_PS_FILE
#if SAVE_VIDEO_FILE
	fp_video = fopen("gb.h264", "w");
#endif 
#if SAVE_AUDIO_FILE
	fp_audio = fopen("gb.acc", "w");
#endif 

	RTPSession sess;
	std::string ipstr;
	int status, j;

	// Now, we'll create a RTP session, set the destination
	// and poll for incoming data.

	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;

	// IMPORTANT: The local timestamp unit MUST be set, otherwise
	//            RTCP Sender Report info will be calculated wrong
	// In this case, we'll be just use 8000 samples per second.
	sessparams.SetOwnTimestampUnit(1.0 / 8000.0);

	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(m_portListen);
	status = sess.Create(sessparams, &transparams);
	checkerror(status);
    
	_frameBuff = new uint8_t[PS_BUFF_SIZE];
	memset(_frameBuff, 0, sizeof(_frameBuff));
	_frameSize = 0;
	_buffLen = 0;

	for (j = 1; j <= m_secsWait; j++)
	{
		sess.BeginDataAccess();
		printf("secs gone %d\n", j);
		// check incoming packets
		if (sess.GotoFirstSourceWithData())
		{
			do
			{
				RTPPacket *pack;
				while ((pack = sess.GetNextPacket()) != NULL)
				{
					// printf("Got packet:%d\n", pack->GetSequenceNumber());
					// You can examine the data here
					if (pack->GetPayloadType() == 96)//ps流
					{
						if ((pack->GetPayloadData()[0] == 0x00 && pack->GetPayloadData()[1] == 0x00 &&
							pack->GetPayloadData()[2] == 0x01 && pack->GetPayloadData()[3] == 0xba)||
							(pack->GetPayloadData()[0] == 0x00 && pack->GetPayloadData()[1] == 0x00 &&
							pack->GetPayloadData()[2] == 0x01 && pack->GetPayloadData()[3] == 0xe0)||//0x000001e0 视频流
							(pack->GetPayloadData()[0] == 0x00 && pack->GetPayloadData()[1] == 0x00 &&
							pack->GetPayloadData()[2] == 0x01 && pack->GetPayloadData()[3] == 0xc0))//0x000001c0 音频流
						{
							_seq = pack->GetSequenceNumber();
							_timestamp = pack->GetTimestamp();
							_ssrc = pack->GetSSRC();
						}
                        accessPsPacket(pack->GetPayloadData(), pack->GetPayloadLength());
					}
					// we don't longer need the packet, so
					// we'll delete it
					sess.DeletePacket(pack);
				}
			} while (sess.GotoNextSourceWithData());
		}

		sess.EndDataAccess();

#ifndef RTP_SUPPORT_THREAD
		status = sess.Poll();
		checkerror(status);
#endif // RTP_SUPPORT_THREAD

		RTPTime::Wait(RTPTime(1, 0));
	}
	sess.BYEDestroy(RTPTime(10, 0), 0, 0);

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
#if SAVE_PS_FILE
	fclose(fp_ps);
#endif
#if SAVE_VIDEO_FILE
	fclose(fp_video);
#endif
#if SAVE_AUDIO_FILE
	fclose(fp_audio);
#endif
	printf("StreamReciever exits\n");
}

void StreamResolver::accessPsPacket(uint8_t* payloadData, int payloadLength)
{

#if SAVE_PS_FILE
	fwrite(payloadData, 1, payloadLength, fp_ps);
#endif
	//查找ps头 0x000001BA
	if (payloadData[0] == 0x00 && payloadData[1] == 0x00 &&
		payloadData[2] == 0x01 && payloadData[3] == 0xba)
	{
		resolvePsPacket(payloadData, payloadLength);
	}
    else if (payloadData[0] == 0x00 && payloadData[1] == 0x00 &&
		payloadData[2] == 0x01 && payloadData[3] == 0xe0)//0x000001e0 视频流
    {
        resolvePesPacket(payloadData, payloadLength);
    }
    else if (payloadData[0] == 0x00 && payloadData[1] == 0x00 &&
		payloadData[2] == 0x01 && payloadData[3] == 0xc0)//0x000001c0 音频流
	{ 
        resolvePesPacket(payloadData, payloadLength);
	}
	else if (payloadData[0] == 0x00 && payloadData[1] == 0x00 &&
        payloadData[2] == 0x01 && payloadData[3] == 0xbd)//私有数据
	{ 
	}
	else  //当然如果开头不是0x000001BA,默认为一个帧的中间部分,我们将这部分内存顺着帧的开头向后存储
	{
		appendFrameData(payloadData, payloadLength);
	}
}

void StreamResolver::appendFrameData(uint8_t* payloadData, int payloadLength)
{
    if (payloadLength + _buffLen >= _frameSize)
	{
		int len = _frameSize - _buffLen;
		memcpy(_frameBuff + _buffLen, payloadData, len);
		_buffLen += len;
        if(VIDEO_STREAM == m_curStream)
    		writeVideoFrame();
        else if (AUDIO_STREAM == m_curStream)
            writeAudioFrame();
        
		if (payloadLength > len)
			resolvePsPacket(payloadData + len, payloadLength - len);
	}
	else
	{
		memcpy(_frameBuff + _buffLen, payloadData, payloadLength);
		_buffLen += payloadLength;
	}
}