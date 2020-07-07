/*
 * H.26L/H.264/AVC/JVT/14496-10/... decoder
 * Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * H.264 / AVC / MPEG4 part10 codec.
 * @author Michael Niedermayer <michaelni@gmx.at>
 */

#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "internal.h"
#include "dsputil.h"
#include "avcodec.h"
#include "config.h"
#include "CdxParser.h"
#include "vdecoder.h"
#include "adapter.h"
#include "AwPluginManager.h"


typedef struct CEDARXContext
{
	VideoDecoder *pVideoDec;
}CEDARXContext;


av_cold int decode_init(AVCodecContext *avctx){
	int ret = -1;
	CEDARXContext *cedarContext = avctx->priv_data;
	VConfig 			VideoConf;
	VideoStreamInfo 	VideoInfo;
	//memset(cedarContext,0,sizeof(CEDARXContext));
	memset(&VideoInfo, 0, sizeof(VideoStreamInfo));
	memset(&VideoConf, 0, sizeof(VConfig));
	AwPluginInit();
	cedarContext->pVideoDec = CreateVideoDecoder();
	av_log(NULL, AV_LOG_WARNING,"after CreateVideoDecoder() pVideoDec = %p\n",cedarContext->pVideoDec);
	if(cedarContext->pVideoDec == NULL){
		av_log(NULL, AV_LOG_WARNING,"decoder demom CreateVideoDecoder() error\n");
		return -1;
	}
	//init video stream info
	av_log(NULL, AV_LOG_WARNING,"%s 3\n",__FUNCTION__);
	VideoStreamInfo *vp = &VideoInfo;
	vp->eCodecFormat = VIDEO_CODEC_FORMAT_H264;
	vp->nWidth = 800;
	vp->nHeight = 480;
	vp->nCodecSpecificDataLen = 0;
	vp->pCodecSpecificData = NULL;
	
	VideoConf.eOutputPixelFormat  = PIXEL_FORMAT_YV12;//PIXEL_FORMAT_YV12;
	ret = InitializeVideoDecoder(cedarContext->pVideoDec, &VideoInfo, &VideoConf);
	av_log(NULL, AV_LOG_WARNING,"after InitializeVideoDecoder() ret = %d\n",ret);
	if(ret != 0)
	{
		av_log(NULL, AV_LOG_WARNING,"decoder demom initialize video decoder fail.\n");
		DestroyVideoDecoder(cedarContext->pVideoDec);
		cedarContext->pVideoDec = NULL;
		return -1;
	}
	
	av_log(NULL, AV_LOG_WARNING,"%s open cedarx success.\n",__FUNCTION__);
	return 0;
    
}





static int decode_frame(AVCodecContext *avctx,void *data, int *data_size,AVPacket *avpkt)
{
	VideoStreamDataInfo  dataInfo;
	VideoPicture* pPicture;
	const uint8_t *buf = avpkt->data;
	CEDARXContext *cedarContext = (CEDARXContext*)avctx->priv_data;
	VideoDecoder *pVideoDec = cedarContext->pVideoDec;
    int buf_size = avpkt->size;
	AVFrame *pict = data;
	CdxPacketT packet;
	int ret = -1;
	av_log(NULL, AV_LOG_WARNING, "%s buf_size = %d\n",__FUNCTION__,buf_size);
	memset(&packet, 0, sizeof(packet));
	if(buf_size != 0){
		ret = RequestVideoStreamBuffer(pVideoDec,
											buf_size,
											(char**)&packet.buf,
											&packet.buflen,
											(char**)&packet.ringBuf,
											&packet.ringBufLen,
											0);
		if(ret != 0)
		{
			av_log(NULL, AV_LOG_WARNING, "%s RequestVideoStreamBuffer fail. request size: %d\n",__FUNCTION__, buf_size);
			return -1;
		}
		if(packet.buflen + packet.ringBufLen < buf_size)
		{
			av_log(NULL, AV_LOG_WARNING, " RequestVideoStreamBuffer fail, require size is too small ");
			return -1;
		}

		memcpy(packet.buf,buf,buf_size);

		memset(&dataInfo, 0, sizeof(VideoStreamDataInfo));
		dataInfo.pData		  = packet.buf;
		dataInfo.nLength	  = buf_size;
		dataInfo.nPts		  = avpkt->pts;
		//dataInfo.nPcr		  = packet.pcr;
		dataInfo.bIsFirstPart = 1;
		dataInfo.bIsLastPart = 1;
		av_log(NULL, AV_LOG_WARNING, "%s SubmitVideoStreamData before.\n",__FUNCTION__);
		ret = SubmitVideoStreamData(pVideoDec , &dataInfo, 0);
		av_log(NULL, AV_LOG_WARNING, "%s SubmitVideoStreamData ret = %d\n",__FUNCTION__,ret);
		if(ret != 0){
			av_log(NULL, AV_LOG_WARNING, "%s SubmitVideoStreamData error,ret = %d\n",__FUNCTION__,ret);
		}
		ret = DecodeVideoStream(pVideoDec, 0 /*eos*/,0/*key frame only*/, 0/*drop b frame*/,0/*current time*/);
		av_log(NULL, AV_LOG_WARNING, "%s DecodeVideoStream ret = %d\n",__FUNCTION__,ret);
		if(ret == VDECODE_RESULT_KEYFRAME_DECODED || ret == VDECODE_RESULT_FRAME_DECODED){
			pPicture = RequestPicture(pVideoDec, 0/*the major stream*/);
			av_log(NULL, AV_LOG_WARNING, "%s RequestPicture pPicture = %p\n",__FUNCTION__,pPicture);
			if(pPicture != NULL){
				av_log(NULL, AV_LOG_WARNING, "%s pPicture.nWidth = %d,pPicture.nHeight = %d\n",__FUNCTION__,pPicture->nWidth,pPicture->nHeight);
				av_log(NULL, AV_LOG_WARNING, "%s pPicture.ePixelFormat = %d,pPicture.nLineStride = %d\n",__FUNCTION__,pPicture->ePixelFormat,pPicture->nLineStride);
				int nSizeY = pPicture->nWidth * pPicture->nHeight;
				int nSizeUV = nSizeY >> 2;
				//YV12 to YUV
				memcpy(pict->data[0],pPicture->pData0,nSizeY);
				memcpy(pict->data[1],pPicture->pData0 + nSizeY + nSizeUV,nSizeUV);
				memcpy(pict->data[2],pPicture->pData0 + nSizeY,nSizeUV);
				*data_size = sizeof(AVFrame);

				ReturnPicture(pVideoDec, pPicture);
			}
		}
		return buf_size;

	}
	return ret;
    
}


av_cold int decode_close(AVCodecContext *avctx)
{
	CEDARXContext *cedarContext = (CEDARXContext*)avctx->priv_data;
	av_log(NULL, AV_LOG_WARNING, "%s\n",__FUNCTION__);
	DestroyVideoDecoder(cedarContext->pVideoDec);
}



AVCodec ff_h264_cedarX_decoder = {
    .name           = "h264_cedarX",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = CODEC_ID_H264,
    .priv_data_size = sizeof(CEDARXContext),
    .init           = decode_init,
    .close          = decode_close,
    .decode         = decode_frame,
    .long_name = NULL_IF_CONFIG_SMALL("H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10"),

};
extern AVCodec ff_h264_cedarX_decoder;
