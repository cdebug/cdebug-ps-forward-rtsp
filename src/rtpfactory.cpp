#include "rtpfactory.h"
#include "rtspconnector.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

RtpFactory::RtpFactory()
{

}

RtpFactory::~RtpFactory()
{

}

int RtpFactory::getPayloadType(RtpSource* rtp)
{
    int ret = 0;
    if(rtp->len > 12)
        ret = rtp->data[1] & 0x7F;
    return ret;
}

void RtpFactory::parserRtpData(RtpSource* rtp)
{
    if(getPayloadType(rtp) == 96)
    {
        accessPsPacket(rtp, 12);
    }
}

void RtpFactory::accessPsPacket(RtpSource* rtp, int dataPos)
{
    if(rtp->fp_ps)
    {
    	int size = fwrite(rtp->data + dataPos, 1, rtp->len - dataPos, rtp->fp_ps);
    }

	//查找ps头 0x000001BA
	if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos+1] == 0x00 &&
		rtp->data[dataPos+2] == 0x01 && rtp->data[dataPos+3] == 0xba)
	{
		resolvePsPacket(rtp, dataPos);
	}
    else if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos+1] == 0x00 &&
		rtp->data[dataPos+2] == 0x01 && rtp->data[dataPos+3] == 0xe0)//0x000001e0 视频流
    {
        resolvePesPacket(rtp, dataPos);
    }
    else if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos+1] == 0x00 &&
		rtp->data[dataPos+2] == 0x01 && rtp->data[dataPos+3] == 0xc0)//0x000001c0 音频流
	{ 
        resolvePesPacket(rtp, dataPos);
	}
	else if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos+1] == 0x00 &&
        rtp->data[dataPos+2] == 0x01 && rtp->data[dataPos+3] == 0xbd)//私有数据
	{ 
	}
	else  //当然如果开头不是0x000001BA,默认为一个帧的中间部分,我们将这部分内存顺着帧的开头向后存储
	{
		appendFrameData(rtp, dataPos);
	}
}

void RtpFactory::resolvePsPacket(RtpSource* rtp, int dataPos)
{
    if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos+1] == 0x00 &&
		rtp->data[dataPos+2] == 0x01 && rtp->data[dataPos+3] == 0xba)
    {
        uint8_t expand_size = rtp->data[dataPos+13] & 0x07;//扩展字节
        dataPos += 14 + expand_size;//ps包头14
        /******  i 帧  ******/
        if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos + 1] == 0x00 && 
            rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xbb)//0x000001bb ps system header
        {
	        uint16_t psh_size = rtp->data[dataPos + 4] << 8 | rtp->data[dataPos + 5];//psh长度
	        dataPos += 6 + psh_size;
	        if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos + 1] == 0x00 && 
                rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xbc)//0x000001bc ps system map
	        {
		        uint16_t psm_size = rtp->data[dataPos + 4] << 8 | rtp->data[dataPos + 5];
		        dataPos += 6 + psm_size;
		    }
	        else
	        {
		        printf("no system map and no video stream\n");
		        return;
	        }
	    }
    }
    resolvePesPacket(rtp, dataPos);
}

void RtpFactory::resolvePesPacket(RtpSource* rtp, int dataPos)
{
	/******  统一 帧  ******/
	while (true)
	{
        if(rtp->data[dataPos+0] == 0x00 && rtp->data[dataPos+1] == 0x00 && 
            rtp->data[dataPos+2] == 0x01 && rtp->data[dataPos+3] == 0xe0)
        {
		    getVideoFrame(rtp, dataPos);
		}
        else if(rtp->data[dataPos] == 0x00 && rtp->data[dataPos+1] == 0x00 && 
            rtp->data[dataPos+2] == 0x01 && rtp->data[dataPos+3] == 0xc0)
        {
            getAudioFrame(rtp, dataPos);
        }
        else
        {
            // printf("no valid stream c0 or e0\n");
            break;
        }
		uint16_t stream_size = rtp->data[dataPos+4] << 8 | rtp->data[dataPos+5];
        dataPos += 6 + stream_size;
        if(rtp->len - dataPos < 4)
            break;
	}
}

void RtpFactory::getVideoFrame(RtpSource* rtp, int dataPos)
{
	memset(rtp->frameBuff, 0, sizeof(rtp->frameBuff));
	rtp->frameLen = 0;
	rtp->frameSize = 0;

    rtp->curStream = VIDEO_STREAM;
    uint16_t h264_size = rtp->data[dataPos+4] << 8 | rtp->data[dataPos+5];
	uint8_t expand_size = rtp->data[dataPos+8];
	rtp->frameSize = h264_size - 3 - expand_size;
	dataPos += 9 + expand_size;
	//全部写入并保存帧
	if (rtp->frameSize <= rtp->len - dataPos)
	{
	    memcpy(rtp->frameBuff, rtp->data + dataPos, rtp->frameSize);
	    rtp->frameLen += rtp->frameSize;
		dataPos += rtp->frameSize;
		writeVideoFrame(rtp);
// #if FORWARD_RTSP
//     uint32_t timestamp, ssrc;
//     memcpy((uint8_t*)&timestamp, rtp->data + 4, 4);
//     timestamp = htonl(timestamp);
//     memcpy((uint8_t*)&ssrc, rtp->data + 8, 4);
//     ssrc = htonl(ssrc);
// 	RtspConnector::instance()->sendRtspData(rtp->frameBuff, rtp->frameLen
// 		, 0, timestamp, ssrc);
// #endif
	}
	else
	{
	    memcpy(rtp->frameBuff, rtp->data + dataPos, rtp->len - dataPos);
	    rtp->frameLen += rtp->len - dataPos;
    }
}

void RtpFactory::getAudioFrame(RtpSource* rtp, int dataPos)
{
    memset(rtp->frameBuff, 0, sizeof(rtp->frameBuff));
	rtp->frameLen = 0;
	rtp->frameSize = 0;

    rtp->curStream = AUDIO_STREAM;
    uint16_t stream_size = rtp->data[dataPos+4] << 8 | rtp->data[dataPos+5];
	uint8_t expand_size = rtp->data[dataPos+8];
	rtp->frameSize = stream_size - 3 - expand_size;
	dataPos += 9 + expand_size;
	//全部写入并保存帧
	if (rtp->frameSize <= rtp->len - dataPos)
	{
	    memcpy(rtp->frameBuff, rtp->data + dataPos, rtp->frameSize);
	    rtp->frameLen += rtp->frameSize;
		dataPos += rtp->frameSize;
		writeAudioFrame(rtp);
	}
	else
	{
	    memcpy(rtp->frameBuff, rtp->data + dataPos, rtp->len - dataPos);
	    rtp->frameLen += rtp->len - dataPos;
    }
}

void RtpFactory::writeVideoFrame(RtpSource* rtp)
{
	if (rtp->frameSize != rtp->frameLen)
		printf("error:frameSize=%d bufflen=%d\n", rtp->frameSize, rtp->frameLen);
    if(rtp->fp_video)
    	fwrite(rtp->frameBuff, 1, rtp->frameLen, rtp->fp_video);
#if FORWARD_RTSP
    uint32_t timestamp;
    memcpy((uint8_t*)&timestamp, rtp->data + 4, 4);
    timestamp = htonl(timestamp);
	uint32_t ssrc;
    memcpy((uint8_t*)&ssrc, rtp->data + 8, 4);
    ssrc = htonl(ssrc);
	RtspConnector::instance()->sendRtspData(rtp->frameBuff, rtp->frameLen
		, 0, timestamp, ssrc);
#endif
	memset(rtp->frameBuff, 0, sizeof(rtp->frameBuff));
	rtp->frameLen = 0;
	rtp->frameSize = 0;
}

void RtpFactory::writeAudioFrame(RtpSource* rtp)
{
	if (rtp->frameSize != rtp->frameLen)
		printf("error:frameSize=%d bufflen=%d\n", rtp->frameSize, rtp->frameLen);
#if SAVE_AUDIO_FILE
	fwrite(_frameBuff, 1, _buffLen, fp_audio);
#endif
	memset(rtp->frameBuff, 0, sizeof(rtp->frameBuff));
	rtp->frameLen = 0;
	rtp->frameSize = 0;
}

void RtpFactory::appendFrameData(RtpSource* rtp, int dataPos)
{
    if (rtp->len - dataPos + rtp->frameLen >= rtp->frameSize)
	{
		int len = rtp->frameSize - rtp->frameLen;
		memcpy(rtp->frameBuff + rtp->frameLen, rtp->data + dataPos, len);
		rtp->frameLen += len;
        if(VIDEO_STREAM == rtp->curStream)
    		writeVideoFrame(rtp);
        else if (AUDIO_STREAM == rtp->curStream)
            writeAudioFrame(rtp);
        
		if (rtp->len > len)
			resolvePsPacket(rtp, dataPos+len);
	}
	else
	{
		memcpy(rtp->frameBuff + rtp->frameLen, rtp->data + dataPos, rtp->len - dataPos);
		rtp->frameLen += rtp->len - dataPos;
	}
}