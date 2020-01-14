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
#include "libcedarv/libcedarv/libcedarv.h"	//* for decoding video


cedarv_decoder_t*	 		hcedarv;
cedarv_stream_info_t		stream_info;
cedarv_picture_t			ce_picture;



static int bFirstFrame = 0;
av_cold int decode_init(AVCodecContext *avctx){
	int ret;

	av_log(NULL, AV_LOG_WARNING, "decode_init\n");

	hcedarv = libcedarv_init(&ret);
	av_log(NULL, AV_LOG_WARNING, "libcedarv_init ret = %d,hcedarv = %p.\n",ret,hcedarv);
	if(ret < 0 || hcedarv == NULL)
	{
		av_log(NULL, AV_LOG_WARNING, "libcedarv_init fail, test program quit.\n");
	}

	stream_info.video_width = avctx->width;
	stream_info.video_height = avctx->height;
	stream_info.format = CEDARV_STREAM_FORMAT_H264; 
	stream_info.sub_format = CEDARV_SUB_FORMAT_UNKNOW;
	stream_info.container_format = CEDARV_CONTAINER_FORMAT_UNKNOW;

	stream_info.init_data = NULL;
	stream_info.init_data_len= 0;


	//* set video stream information to libcedarv.
	ret = hcedarv->set_vstream_info(hcedarv, &stream_info);
	if(ret < 0)
	{
		av_log(NULL, AV_LOG_WARNING, "set video stream information to libcedarv fail, test program quit.\n");
	}

	//* open libcedarv.
	ret = hcedarv->open(hcedarv);

	if(ret < 0)
	{
		av_log(NULL, AV_LOG_WARNING,"open libcedarv fail ret=%d, test program quit.\n",ret);
		
	}
		
	//* tell libcedarv to start.
	hcedarv->ioctrl(hcedarv, CEDARV_COMMAND_PLAY, 0);
	bFirstFrame = 1;


	av_log(NULL, AV_LOG_WARNING,"cedarv open ok\n");
	return 0;
    
}

static int decode_frame(AVCodecContext *avctx,void *data, int *data_size,AVPacket *avpkt)
{
	cedarv_stream_data_info_t  stream_data_info;
	u8* 						buf0;
	u32 						bufsize0;
	u8* 						buf1;
	u32 						bufsize1;

	const uint8_t *buf = avpkt->data;
    int buf_size = avpkt->size;
	AVFrame *pict = data;
    int buf_index;
	int ret = -1;
	int64_t pts = avpkt->pts;
	av_log(NULL, AV_LOG_WARNING, "decode_frame buf_size = %d\n",buf_size);
	if(buf_size != 0){
		memset(&stream_data_info, 0, sizeof(cedarv_stream_data_info_t));
		ret = hcedarv->request_write(hcedarv, buf_size, &buf0, &bufsize0, &buf1, &bufsize1);
		av_log(NULL, AV_LOG_WARNING, "decode_frame bufsize0 = %d,bufsize1=%d\n",bufsize0,bufsize1);
		if(ret < 0)
		{
			av_log(NULL, AV_LOG_WARNING, "request bitstream buffer fail.\n");
			return ret;
		}
		if(buf_size <= bufsize0) {
			mem_cpy(buf0, buf, buf_size);
			//cedarx_cache_op(buf0, (buf0 + bufsize0), 1); //flush cache
		}

		else
		{
			//printf("private data request write error\n");
			mem_cpy(buf0,buf,bufsize0);
			mem_cpy(buf1,(buf+bufsize0),bufsize1);
		}
		// set stream data info
		stream_data_info.pts = pts;	//* get video pts
		stream_data_info.lengh = buf_size;
		stream_data_info.flags = CEDARV_FLAG_PTS_VALID | CEDARV_FLAG_FIRST_PART | CEDARV_FLAG_LAST_PART;

		//stream_data_info.flags = CEDARV_FLAG_FIRST_PART | CEDARV_FLAG_LAST_PART;
		
		//* update data to libcedarv.
		hcedarv->update_data(hcedarv, &stream_data_info);
		ret = hcedarv->decode(hcedarv);
		av_log(NULL, AV_LOG_WARNING,"decode result: %d\n", ret);
		if(ret == CEDARV_RESULT_ERR_NO_MEMORY || ret == CEDARV_RESULT_ERR_UNSUPPORTED)
		{
			av_log(NULL, AV_LOG_WARNING, "bitstream is unsupported.\n");
			return -1;
		}
		ret = hcedarv->display_request(hcedarv, &ce_picture);
		av_log(NULL, AV_LOG_WARNING, "display_request = %d.\n",ret);
		if(ret == CEDARV_RESULT_OK){
			//todo:get decode data from ce_picture
			ret = hcedarv->display_release(hcedarv, ce_picture.id);
		}
	    return 0;
	}
	return ret;
    
}


av_cold int decode_close(AVCodecContext *avctx)
{
	av_log(NULL, AV_LOG_WARNING, "decode_close\n");


    if(hcedarv)
	{
		hcedarv->ioctrl(hcedarv, CEDARV_COMMAND_STOP, 0);
		hcedarv->close(hcedarv);
		libcedarv_exit(hcedarv);
	}
}



AVCodec ff_h264_cedarX_decoder = {
    .name           = "h264_cedarX",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = CODEC_ID_H264,
    .init           = decode_init,
    .close          = decode_close,
    .decode         = decode_frame,
    .long_name = NULL_IF_CONFIG_SMALL("H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10"),

};
extern AVCodec ff_h264_cedarX_decoder;
