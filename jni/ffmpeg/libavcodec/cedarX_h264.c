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
#include <libcedarv.h>	//* for decoding video


cedarv_decoder_t*	 		hcedarv;
cedarv_stream_info_t		stream_info;
cedarv_picture_t			ce_picture;



static int bFirstFrame = 0;
av_cold int decode_init(AVCodecContext *avctx){
	int ret = -1;
	/* init video engine */
	ret = cedarx_hardware_init(0);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_WARNING, "cedarx_hardware_init failed\n");
	}

	av_log(NULL, AV_LOG_WARNING, "decode_init width:height=%d:%d\n",avctx->width,avctx->height);

	hcedarv = libcedarv_init(&ret);
	av_log(NULL, AV_LOG_WARNING, "libcedarv_init ret = %d,hcedarv = %p.\n",ret,hcedarv);
	if(ret < 0 || hcedarv == NULL)
	{
		av_log(NULL, AV_LOG_WARNING, "libcedarv_init fail, test program quit.\n");
	}
	
	stream_info.video_width = avctx->width;
	stream_info.video_height = avctx->height;
	//stream_info.format = CEDARV_STREAM_FORMAT_H264; 
	stream_info.format = CEDARV_STREAM_FORMAT_H264;
	//stream_info.container_format = CEDARV_CONTAINER_FORMAT_UNKNOW;
	//stream_info.container_format = CEDARV_CONTAINER_FORMAT_PMP;

	stream_info.init_data = NULL;
	stream_info.init_data_len= 0;


	//* set video stream information to libcedarv.
	ret = hcedarv->set_vstream_info(hcedarv, &stream_info);
	av_log(NULL, AV_LOG_WARNING, "set_vstream_info ret = %d\n",ret);
	if(ret < 0)
	{
		av_log(NULL, AV_LOG_WARNING, "set video stream information to libcedarv fail, test program quit.\n");
	}

	av_log(NULL, AV_LOG_WARNING, "prepare open\n");

	//* open libcedarv.
	ret = hcedarv->open(hcedarv);
	av_log(NULL, AV_LOG_WARNING, "open ret = %d\n",ret);

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

void print_head(const uint8_t *buf){
	int i=0;
	for(i=0;i<20;i++){
			av_log(NULL, AV_LOG_WARNING, "%2x ",buf[i]);
		}
}
int pts = 0 ;
int flag_save = 1;

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
	int i= 0;
	av_log(NULL, AV_LOG_WARNING, "--------start------------------decode_frame buf_size = %d,pts= %d------------------------\n",buf_size,pts);

	//print_head(buf);
	
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
			//cedarx_cache_op(buf0, (buf0 + bufsize0), 1);
			mem_cpy(buf1,(buf+bufsize0),bufsize1);
			//cedarx_cache_op(buf1, (buf1 + bufsize1), 1);
		}
		// set stream data info
		stream_data_info.pts = -1;	//* get video pts
		stream_data_info.lengh = buf_size;
		stream_data_info.flags = CEDARV_FLAG_FIRST_PART | CEDARV_FLAG_LAST_PART ;
		
		//* update data to libcedarv.
		hcedarv->update_data(hcedarv, &stream_data_info);
		
		ret = hcedarv->decode(hcedarv);
		
		int is_key_frame = (ret == 3)?1:0;
		av_log(NULL, AV_LOG_WARNING,"decode result: %d\n", ret);
		if(ret == CEDARV_RESULT_FRAME_DECODED || ret == CEDARV_RESULT_KEYFRAME_DECODED){
			ret = hcedarv->display_request(hcedarv, &ce_picture);
			
			av_log(NULL, AV_LOG_WARNING, "display_request = %d.\n",ret);
		
			if(ret == CEDARV_RESULT_OK){
				//todo:get decode data from ce_picture
				av_log(NULL, AV_LOG_WARNING, "%d,%d:%d,pixel_format=%d,pts=%d\n",ce_picture.id,ce_picture.width,ce_picture.height,ce_picture.pixel_format,ce_picture.pts);

				av_log(NULL, AV_LOG_WARNING, 
				"y=%d,%p   u=%d,%p  v=%d,%p\n",
				ce_picture.size_y,ce_picture.y,
				ce_picture.size_u,ce_picture.u,
				ce_picture.size_v,ce_picture.v
				);
				
				pict->format = (ce_picture.pixel_format == CEDARV_PIXEL_FORMAT_AW_YUV420) ? PIX_FMT_YUV420P:PIX_FMT_YUV422P;
				pict->width = ce_picture.width;
				pict->height = ce_picture.height;
				pict->key_frame = is_key_frame;
				pict->pkt_pts = ce_picture.pts;
				
				
				//ret = av_image_alloc(pict->data, pict->linesize, ce_picture.width, ce_picture.height, ce_picture.pixel_format, 32);
				
				//memset(pict->data[0], 0, sizeof(pict->data[0]));
				//memset(pict->data[1], 0, sizeof(pict->data[1]));
				//memset(pict->data[2], 0, sizeof(pict->data[2]));
				//av_log(NULL, AV_LOG_WARNING,"av_image_alloc ret = %d\n",ret);
				unsigned char* src_y = (unsigned char*)cedarv_address_phy2vir((void*)ce_picture.y);
				unsigned char* src_u = (unsigned char*)cedarv_address_phy2vir((void*)ce_picture.u);
				av_log(NULL, AV_LOG_WARNING,"cedarv_address_phy2vir success!!!!\n");
				int u_size = ce_picture.width * ce_picture.height/4;
				pict->data[0] = (uint8_t *)av_malloc(ce_picture.width * ce_picture.height+1);
				pict->data[1] = (uint8_t *)av_malloc(u_size+1);
				pict->data[2] = (uint8_t *)av_malloc(u_size+1);
				memcpy(pict->data[0],src_y,ce_picture.width * ce_picture.height);
				memcpy(pict->data[1],src_u,u_size);
				memcpy(pict->data[2],src_u + u_size,u_size);

				if(flag_save == 1 && is_key_frame==1){
					FILE *fp;
					fp = fopen("/sdcard/output_420.y","wb+");
					fwrite(src_y,1,ce_picture.width * ce_picture.height,fp);
					av_log(NULL, AV_LOG_WARNING,"save y success\n");
					fclose(fp);
					flag_save = 0;

				}

				pict->linesize[0] = ce_picture.width + 32;
				pict->linesize[1] = 176;
				pict->linesize[2] = 176;
				
				*data_size = sizeof(AVFrame);
				
				ret = hcedarv->display_release(hcedarv, ce_picture.id);
			}
		}
		av_log(NULL, AV_LOG_WARNING, "--------------end---------------decode_frame--------------------------\n");
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
	cedarx_hardware_exit(0);
}
void YUV420toRGB24(unsigned char *src0,unsigned char *src1,unsigned char *src2, unsigned char *rgb24, int width, int height) 
{
	int R,G,B,Y,U,V;
	int x,y;
	int nWidth = width>>1; //色度信号宽度
	for (y=0;y<height;y++)
	{
		for (x=0;x<width;x++)
		{
		    Y = *(src0 + y*width + x);
		    U = *(src1 + ((y>>1)*nWidth) + (x>>1));
		    V = *(src2 + ((y>>1)*nWidth) + (x>>1));
		    R = Y + 1.402*(V-128);
		    G = Y - 0.34414*(U-128) - 0.71414*(V-128);
		    B = Y + 1.772*(U-128);

		   //防止越界
		    if (R>255)
				R=255;
		    if (R<0)
				R=0;
		    if (G>255)
				G=255;
		    if (G<0)
				G=0;
		    if (B>255)
				B=255;
		    if (B<0)
				B=0;
		   
		   *(rgb24 + ((height-y-1)*width + x)*3) = B;
		   *(rgb24 + ((height-y-1)*width + x)*3 + 1) = G;
		   *(rgb24 + ((height-y-1)*width + x)*3 + 2) = R;
		 
		}
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
