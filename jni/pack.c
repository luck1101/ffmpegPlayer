#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

#include <string.h>
#include <jni.h>
#include <pthread.h>

#include <android/log.h>
#define INFO(msg) __android_log_write(ANDROID_LOG_INFO,"native",msg);

#define RE(msg) return (*env)->NewStringUTF(env, msg);

char debugMsg[100];
int test = 0;

AVFormatContext *pFormatCtx;
int             i, videoStream, frameCount = 0;
AVCodecContext  *pCodecCtx;
AVCodec         *pCodec;
AVFrame         *pFrame;
AVPacket        packet;
int             frameFinished;
float           aspect_ratio;

AVFrame         *pFrameRGB;
int             numBytes;
uint8_t         *buffer;
// BE for Big Endian, LE for Little Endian
int dstFmt = PIX_FMT_RGB565;

struct SwsContext *img_convert_ctx;
int width, height, bit_rate;
int screenWidth, screenHeight;


/*****************************************************/
/* test */
/*****************************************************/

jstring
Java_sysu_ss_xu_FFmpeg_stringFromJNI( JNIEnv* env, jobject thiz )
{  
 return (*env)->NewStringUTF(env, "Helloworld from FFmpeg!");
}

void
Java_sysu_ss_xu_FFmpeg_nativeTest( JNIEnv* env, jobject thiz)
{
	++test;
	sprintf(debugMsg, "%d", test);
	INFO(debugMsg);
}

jintArray Java_sysu_ss_xu_FFmpeg_jniIntArray( JNIEnv* env, jobject thiz )
{
INFO("hi");
	int nativeIntArray[2];
	nativeIntArray[0] = 330;
	nativeIntArray[1] = 2;
	sprintf(debugMsg, "0 %d", nativeIntArray[0]);
	INFO(debugMsg);
	sprintf(debugMsg, "1 %d", nativeIntArray[1]);
	INFO(debugMsg);

	jintArray nativeReturn = (*env)->NewIntArray(env, 2);
	(*env)->SetIntArrayRegion(env, nativeReturn, 0, 2, nativeIntArray);
	return nativeReturn;
}

/*****************************************************/
/* / test */
/*****************************************************/

/*****************************************************/
/* FFmpeg API */
/*****************************************************/

void Java_sysu_ss_xu_FFmpeg_avRegisterAll( JNIEnv* env, jobject thiz )
{
	av_register_all();
}

/*
JNI_FALSE and JNI_TRUE are constants defined for the jboolean type:

#define JNI_FALSE  0
#define JNI_TRUE   1
*/

jboolean Java_sysu_ss_xu_FFmpeg_avOpenInputFile( JNIEnv* env, jobject thiz, jstring jfilePath )
{
	char* filePath = (char *)(*env)->GetStringUTFChars(env, jfilePath, NULL);
	if( av_open_input_file(&pFormatCtx, filePath, NULL, 0, NULL) != 0)
		return 0;
	else
		return 1;
}

jboolean Java_sysu_ss_xu_FFmpeg_avFindStreamInfo( JNIEnv* env, jobject thiz )
{
	if( av_find_stream_info(pFormatCtx) < 0)
		return 0;
	else
		return 1;
}

jboolean Java_sysu_ss_xu_FFmpeg_findVideoStream( JNIEnv* env, jobject thiz )
{
	videoStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++)
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
		videoStream=i;
		break;
		}
	if( videoStream  == -1)
		return 0;
	else
		return 1;
}

jboolean Java_sysu_ss_xu_FFmpeg_avcodecFindDecoder( JNIEnv* env, jobject thiz )
{
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;
	sprintf(debugMsg, "pCodecCtx->codec_id = %d,CODEC_ID_H264 = %d,pFormatCtx->iformat->name=%s\n", pCodecCtx->codec_id,CODEC_ID_H264,pFormatCtx->iformat->name);
	INFO(debugMsg);
	//add by smile
	if(pCodecCtx->codec_id == CODEC_ID_H264){
		  INFO("video is h264.\n");
		  pCodec = avcodec_find_decoder_by_name("h264_cedarX");
		  if(pCodec == NULL){
			  INFO("couldn't find h264_cedarX.\n");
			  pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
		  }else{
			  INFO("find h264_cedarX.\n");
		  }
	  }else{
		  pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	  }
  //end
	//pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) 
		return 0;
	else
		return 1;
}

jboolean Java_sysu_ss_xu_FFmpeg_avcodecOpen( JNIEnv* env, jobject thiz )
{
	if(avcodec_open(pCodecCtx, pCodec)<0)
		return 0;
	else
		return 1;
}

void Java_sysu_ss_xu_FFmpeg_avcodecAllocFrame( JNIEnv* env, jobject thiz )
{
	pFrame=avcodec_alloc_frame();
}

void Java_sysu_ss_xu_FFmpeg_avFree( JNIEnv* env, jobject thiz )
{
	av_free(pFrame);
}

void Java_sysu_ss_xu_FFmpeg_avcodecClose( JNIEnv* env, jobject thiz )
{
	avcodec_close(pCodecCtx);
}

void Java_sysu_ss_xu_FFmpeg_avCloseInputFile( JNIEnv* env, jobject thiz )
{
	av_close_input_file(pFormatCtx);
}

/*****************************************************/
/* / FFmpeg API */
/*****************************************************/

/*****************************************************/
/* functional call */
/*****************************************************/

jstring Java_sysu_ss_xu_FFmpeg_getCodecName( JNIEnv* env, jobject thiz )
{
	return (*env)->NewStringUTF(env, pCodec->name);
}

jint Java_sysu_ss_xu_FFmpeg_getWidth( JNIEnv* env, jobject thiz )
{
	width = pCodecCtx->width;
	return pCodecCtx->width;
}

jint Java_sysu_ss_xu_FFmpeg_getHeight( JNIEnv* env, jobject thiz )
{
	height = pCodecCtx->height;
	return pCodecCtx->height;
}

jint Java_sysu_ss_xu_FFmpeg_getBitRate( JNIEnv* env, jobject thiz )
{
	bit_rate = pCodecCtx->bit_rate;
	return pCodecCtx->bit_rate;
}

jboolean Java_sysu_ss_xu_FFmpeg_allocateBuffer( JNIEnv* env, jobject thiz )
{
	// Allocate an AVFrame structure
	pFrameRGB=avcodec_alloc_frame();
	if(pFrameRGB==NULL)
		return 0;
sprintf(debugMsg, "%d %d", screenWidth, screenHeight);
INFO(debugMsg);
	// Determine required buffer size and allocate buffer
	numBytes=avpicture_get_size(dstFmt, screenWidth, screenHeight);
/*
	numBytes=avpicture_get_size(dstFmt, pCodecCtx->width,
			      pCodecCtx->height);
*/
	buffer=(uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameRGB, buffer, dstFmt, screenWidth, screenHeight);

	return 1;
}

void Java_sysu_ss_xu_FFmpeg_setScreenSize( JNIEnv* env, jobject thiz, int sWidth, int sHeight)
{
	screenWidth = sWidth;
	screenHeight = sHeight;
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

/* for each decoded frame */
jbyteArray Java_sysu_ss_xu_FFmpeg_getNextDecodedFrame( JNIEnv* env, jobject thiz )
{

av_free_packet(&packet);

while(av_read_frame(pFormatCtx, &packet)>=0) {

	if(packet.stream_index==videoStream) {

		int ret = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
		sprintf(debugMsg, "ret = %d,frameFinished=%d\n", ret,frameFinished);
		INFO(debugMsg);

		if(frameFinished) {
			sprintf(debugMsg, "pCodecCtx->width =%d,pCodecCtx->height=%d,pCodecCtx->pix_fmt = %d,dstFmt=%d,key_frame = %d\n", pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,dstFmt,pFrame->key_frame);
			INFO(debugMsg);

			img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, screenWidth, screenHeight, dstFmt, SWS_BICUBIC, NULL, NULL, NULL);
			/*
			img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, dstFmt, SWS_BICUBIC, NULL, NULL, NULL);
			*/

			sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize,0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
			//pFrameRGB->data = pFrame->data;
			//pFrameRGB->linesize =  pFrame->linesize;
			//YUV420toRGB24(pFrame->data[0],pFrame->data[1],pFrame->data[2],buffer,pCodecCtx->width,pCodecCtx->height);

			++frameCount;

			/* uint8_t == unsigned 8 bits == jboolean */
			jbyteArray nativePixels = (*env)->NewByteArray(env, numBytes);
			(*env)->SetByteArrayRegion(env, nativePixels, 0, numBytes, buffer);
			return nativePixels;
		}

	}

	av_free_packet(&packet);
}

return NULL;
}

/*****************************************************/
/* / functional call */
/*****************************************************/


jstring
Java_sysu_ss_xu_FFmpeg_play( JNIEnv* env, jobject thiz, jstring jfilePath )
{

char* filePath = (char *)(*env)->GetStringUTFChars(env, jfilePath, NULL);
RE(filePath);

/*****************************************************/

  AVFormatContext *pFormatCtx;
  int             i, videoStream;
  AVCodecContext  *pCodecCtx;
  AVCodec         *pCodec;
  AVFrame         *pFrame; 
  AVPacket        packet;
  int             frameFinished;
  float           aspect_ratio;
  struct SwsContext *img_convert_ctx;

INFO(filePath);

/* FFmpeg */

  av_register_all();

  if(av_open_input_file(&pFormatCtx, filePath, NULL, 0, NULL)!=0)
	RE("failed av_open_input_file ");
  
  if(av_find_stream_info(pFormatCtx)<0)
    	RE("failed av_find_stream_info");

  videoStream=-1;
  for(i=0; i<pFormatCtx->nb_streams; i++)
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
      videoStream=i;
      break;
    }

  INFO(pFormatCtx->iformat->name);

  if(videoStream==-1)
    	RE("failed videostream == -1");
  
  pCodecCtx=pFormatCtx->streams[videoStream]->codec;
  if(pCodecCtx->codec_id == CODEC_ID_H264){
	  INFO("video is h264.\n");
	  pCodec = avcodec_find_decoder_by_name("h264_cedarX");
	  if(pCodec == NULL){
		  INFO("couldn't find h264_cedarX.\n");
		  pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	  }else{
		  INFO("find h264_cedarX.\n");
	  }
  }else{
	  pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
  }
  if(pCodec==NULL) {
    RE("Unsupported codec!");
  }
  
  if(avcodec_open(pCodecCtx, pCodec)<0)
    RE("failed codec_open");
  
  pFrame=avcodec_alloc_frame();

/* /FFmpeg */

INFO("codec name:");
INFO(pCodec->name);
INFO("Getting into stream decode:");

/* video stream */

  i=0;
  while(av_read_frame(pFormatCtx, &packet)>=0) {

    if(packet.stream_index==videoStream) {
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, 
			   &packet);
      if(frameFinished) {
++i;
INFO("frame finished");

	AVPicture pict;
/*
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
 pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
 PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize,
 0, pCodecCtx->height, pict.data, pict.linesize);
*/
      }

    }

    av_free_packet(&packet);
  }

/* /video stream */

  av_free(pFrame);
  
  avcodec_close(pCodecCtx);
  
  av_close_input_file(pFormatCtx);
  
  RE("end of main");
}
