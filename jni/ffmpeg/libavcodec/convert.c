#include <drv_display.h>
#include <libcedarv.h>
#include "convert.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>


static int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}


static void map32x32_to_yuv_Y(unsigned char* srcY, unsigned char* tarY,unsigned int coded_width,unsigned int coded_height)
{
	unsigned int i,j,l,m,n;
	unsigned int mb_width,mb_height,twomb_line, twomb_width, recon_width;
	unsigned long offset;
	unsigned char *ptr;
	unsigned char *dst_asm,*src_asm;
    unsigned vdecbuf_width, vdecbuf_height;
    int nWidthMatchFlag;
    int nLeftValidLine;  //in the bottom macroblock(32*32), the valid line is < 32.
	ptr = srcY;

    mb_width = ((coded_width+31)&~31) >>4;
	mb_height = ((coded_height+31)&~31) >>4;
	twomb_line = (mb_height+1)>>1;
	recon_width = (mb_width+1)&0xfffffffe;
    twomb_width = (mb_width+1)>>1;
    if(twomb_line < 1 || twomb_width < 1)
    {
        printf("fatal error! twomb_line=%d, twomb_width=%d \n", twomb_line, twomb_width);
    }
    vdecbuf_width = twomb_width*32;
    vdecbuf_height = twomb_line*32;
    if(vdecbuf_width > coded_width)
    {
        nWidthMatchFlag = 0;
        if((vdecbuf_width - coded_width) != 16)
        {
            printf("(f:%s, l:%d) fatal error! vdecbuf_width=%d, gpubuf_width=%d,  the program will crash! \n", __FUNCTION__, __LINE__, vdecbuf_width, coded_width);
        }
    }
    else if(vdecbuf_width == coded_width)
    {
        nWidthMatchFlag = 1;
    }
    else
    {
        nWidthMatchFlag = 0;
    }
	for(i=0;i<twomb_line-1;i++)   //twomb line number
	{
		for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
		{
			for(l=0;l<32;l++)
			{
				//first mb
				m=i*32 + l;     //line num
				n= j*32;        //byte num in one line
				offset = m*coded_width + n;
				//memcpy(tarY+offset,ptr,32);
				dst_asm = tarY+offset;
				src_asm = ptr;
				__asm__ volatile (
				        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
				        "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
				        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
				        :  //[srcY] "r" (srcY)
				        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
				        );

				ptr += 32;  //32 byte in one process.
			}
		}
        //process last macroblock of one line, gpu buf must be 16byte align or 32 byte align
        { //last mb of one line
            for(l=0;l<32;l++)
			{
				//first mb
				m=i*32 + l;     //line num
				n= j*32;        //byte num in one line
				offset = m*coded_width + n;
				//memcpy(tarY+offset,ptr,32);
				dst_asm = tarY+offset;
				src_asm = ptr;
                if(nWidthMatchFlag)
                {
    				__asm__ volatile (
    				        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
    				        "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
    				        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
    				        :  //[srcY] "r" (srcY)
    				        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
    				        );
                }
                else
                {
                    __asm__ volatile (
    				        "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
    				        "vst1.8         {d0,d1}, [%[dst_asm]]              \n\t"
    				        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
    				        :  //[srcY] "r" (srcY)
    				        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
    				        );
                }

				ptr += 32;  //32 byte in one process.
			}
        }
	}
    //last twomb line, we process it alone
    nLeftValidLine = coded_height - (twomb_line-1)*32;
    if(nLeftValidLine!=32)
    {
    }
    for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
	{
		for(l=0;l<nLeftValidLine;l++)
		{
			//first mb
			m=i*32 + l;     //line num
			n= j*32;        //byte num in one line
			offset = m*coded_width + n;
			//memcpy(tarY+offset,ptr,32);
			dst_asm = tarY+offset;
			src_asm = ptr;
			__asm__ volatile (
			        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
			        "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
			        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
			        :  //[srcY] "r" (srcY)
			        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
			        );

			ptr += 32;  //32 byte in one process.
		}
        ptr += (32-nLeftValidLine)*32;
	}
    //process last macroblock of last line, gpu buf must be 16byte align or 32 byte align
    { //last mb of last line
        for(l=0;l<nLeftValidLine;l++)
		{
			//first mb
			m=i*32 + l;     //line num
			n= j*32;        //byte num in one line
			offset = m*coded_width + n;
			//memcpy(tarY+offset,ptr,32);
			dst_asm = tarY+offset;
			src_asm = ptr;
            if(nWidthMatchFlag)
            {
				__asm__ volatile (
				        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
				        "vst1.8         {d0 - d3}, [%[dst_asm]]              \n\t"
				        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
				        :  //[srcY] "r" (srcY)
				        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
				        );
            }
            else
            {
                __asm__ volatile (
				        "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
				        "vst1.8         {d0,d1}, [%[dst_asm]]              \n\t"
				        : [dst_asm] "+r" (dst_asm), [src_asm] "+r" (src_asm)
				        :  //[srcY] "r" (srcY)
				        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
				        );
            }

			ptr += 32;  //32 byte in one process.
		}
        ptr += (32-nLeftValidLine)*32;
    }

}

#define USE_VLD2_8  1
static void map32x32_to_yuv_C(int mode, unsigned char* srcC,unsigned char* tarCb,unsigned char* tarCr,unsigned int coded_width,unsigned int coded_height)
{
	unsigned int i,j,l,m,n,k;
	unsigned int mb_width,mb_height,twomb_line,twomb_width,recon_width;
	unsigned long offset;
	unsigned char *ptr;
	unsigned char *dst0_asm,*dst1_asm,*src_asm;
    unsigned vdecbuf_width, vdecbuf_height; //unit: pixel
    int         nWidthMatchFlag;
    int         nLeftValidLine;  //in the bottom macroblock(32*32), the valid line is < 32.
	unsigned char line[16];
    int dst_stride = mode==0 ? (coded_width + 15) & (~15) : coded_width;

	ptr = srcC;


    mb_width = ((coded_width+15)&~15)>>4;   //vdec's uvBuf is 32byte align, so uBuf and vBuf is 16byte align!
	mb_height = ((coded_height+31)&~31)>>4;
	twomb_line = (mb_height+1)>>1;
    twomb_width = mb_width; //vdec mb32 is uv interleave, so uv_32 byte == u_16byte
    if(twomb_line < 1 || twomb_width < 1)
    {
        printf("map32x32_to_yuv_C() fatal error! twomb_line=%d, twomb_width=%d \n", twomb_line, twomb_width);
    }
    //vdec mb32 uvBuf, one vdec_macro_block, extract u component, u's width and height.
    vdecbuf_width = twomb_width*16;
    vdecbuf_height = twomb_line*32;
    if(vdecbuf_width > coded_width)
    {
        nWidthMatchFlag = 0;
    }
    else if(vdecbuf_width == coded_width)
    {
        nWidthMatchFlag = 1;
    }
    else
    {
        nWidthMatchFlag = 0;
    }

	for(i=0;i<twomb_line-1;i++)
	{
		for(j=0;j<twomb_width-1;j++)
		{
			for(l=0;l<32;l++)
			{
				//first mb
				m=i*32 + l; //line num
				n= j*16;    //byte num in dst_one_line
				offset = m*dst_stride + n;

				dst0_asm = tarCb + offset;
				dst1_asm = tarCr+offset;
				src_asm = ptr;
#if (USE_VLD2_8 == 1)
            __asm__ volatile (
                    "vld2.8         {d0 - d3}, [%[src_asm]]              \n\t"
                    "vst1.8         {d0,d1}, [%[dst0_asm]]              \n\t"
                    "vst1.8         {d2,d3}, [%[dst1_asm]]              \n\t"
                     : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
                     :  //[srcY] "r" (srcY)
                     : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                     );
#else
            __asm__ volatile (
			        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
					"vuzp.8         d0, d1              \n\t"
					"vuzp.8         d2, d3              \n\t"
					"vst1.8         {d0}, [%[dst0_asm]]!              \n\t"
					"vst1.8         {d2}, [%[dst0_asm]]!              \n\t"
					"vst1.8         {d1}, [%[dst1_asm]]!              \n\t"
					"vst1.8         {d3}, [%[dst1_asm]]!              \n\t"
			         : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
			         :  //[srcY] "r" (srcY)
			         : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
			         );
#endif

				ptr += 32;
			}
		}
        //process last twomb_macroblock of one line, gpu buf must be 16 byte align or 8 byte align.
        for(l=0;l<32;l++)
		{
			//first mb
			m=i*32 + l; //line num
			n= j*16;    //byte num in dst_one_line
			offset = m*dst_stride + n;

			dst0_asm = tarCb + offset;
			dst1_asm = tarCr+offset;
			src_asm = ptr;

            if(nWidthMatchFlag)
            {
#if (USE_VLD2_8 == 1)
                __asm__ volatile (
                        "vld2.8         {d0 - d3}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0,d1}, [%[dst0_asm]]              \n\t"
                        "vst1.8         {d2,d3}, [%[dst1_asm]]              \n\t"
                         : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
                         :  //[srcY] "r" (srcY)
                         : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                         );
#else
                __asm__ volatile (
        		        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
        				"vuzp.8         d0, d1              \n\t"
        				"vuzp.8         d2, d3              \n\t"
        				"vst1.8         {d0}, [%[dst0_asm]]!              \n\t"
        				"vst1.8         {d2}, [%[dst0_asm]]!              \n\t"
        				"vst1.8         {d1}, [%[dst1_asm]]!              \n\t"
        				"vst1.8         {d3}, [%[dst1_asm]]!              \n\t"
        		         : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
        		         :  //[srcY] "r" (srcY)
        		         : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
        		         );
#endif

            }
            else
            {
#if (USE_VLD2_8 == 1)
                __asm__ volatile (
                        "vld2.8         {d0,d1}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0}, [%[dst0_asm]]              \n\t"
                        "vst1.8         {d1}, [%[dst1_asm]]              \n\t"
                         : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
                         :  //[srcY] "r" (srcY)
                         : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                         );
#else
                __asm__ volatile (
        		        "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
        				"vuzp.8         d0, d1              \n\t"
        				"vst1.8         {d0}, [%[dst0_asm]]!              \n\t"
        				"vst1.8         {d1}, [%[dst1_asm]]!              \n\t"
        		         : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
        		         :  //[srcY] "r" (srcY)
        		         : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
        		         );
#endif

            }
			ptr += 32;
		}
	}

    //last twomb line, we process it alone
    nLeftValidLine = coded_height - (twomb_line-1)*32;  //uv height can be odd number,must be very careful!
    if(nLeftValidLine!=32)
    {
    }
    for(j=0;j<twomb_width-1;j++)   //macroblock(32*32) number in one line
	{
		for(l=0;l<nLeftValidLine;l++)
		{
			//first mb
			m=i*32 + l;     //line num
			n= j*16;        //byte num in dst_one_line
			offset = m*dst_stride + n;

			dst0_asm = tarCb + offset;
			dst1_asm = tarCr+offset;
			src_asm = ptr;
#if (USE_VLD2_8 == 1)
            __asm__ volatile (
                    "vld2.8         {d0 - d3}, [%[src_asm]]              \n\t"
                    "vst1.8         {d0,d1}, [%[dst0_asm]]              \n\t"
                    "vst1.8         {d2,d3}, [%[dst1_asm]]              \n\t"
                     : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
                     :  //[srcY] "r" (srcY)
                     : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                     );
#else
            __asm__ volatile (
    		        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
    				"vuzp.8         d0, d1              \n\t"
    				"vuzp.8         d2, d3              \n\t"
    				"vst1.8         {d0}, [%[dst0_asm]]!              \n\t"
    				"vst1.8         {d2}, [%[dst0_asm]]!              \n\t"
    				"vst1.8         {d1}, [%[dst1_asm]]!              \n\t"
    				"vst1.8         {d3}, [%[dst1_asm]]!              \n\t"
    		         : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
    		         :  //[srcY] "r" (srcY)
    		         : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
    		         );
#endif
			ptr += 32;  //32 byte in one process.
		}
        ptr += (32-nLeftValidLine)*32;
	}
    //process last macroblock of last line, gpu UVbuf must be 16byte align or 8 byte align
    { //last mb of last line
        for(l=0;l<nLeftValidLine;l++)
		{
			//first mb
			m=i*32 + l;     //line num
			n= j*16;        //byte num in one line
			offset = m*dst_stride + n;

			dst0_asm = tarCb + offset;
			dst1_asm = tarCr+offset;
			src_asm = ptr;
            if(nWidthMatchFlag)
            {
#if (USE_VLD2_8 == 1)
                __asm__ volatile (
                        "vld2.8         {d0 - d3}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0,d1}, [%[dst0_asm]]              \n\t"
                        "vst1.8         {d2,d3}, [%[dst1_asm]]              \n\t"
                         : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
                         :  //[srcY] "r" (srcY)
                         : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                         );
#else
                __asm__ volatile (
        		        "vld1.8         {d0 - d3}, [%[src_asm]]              \n\t"
        				"vuzp.8         d0, d1              \n\t"
        				"vuzp.8         d2, d3              \n\t"
        				"vst1.8         {d0}, [%[dst0_asm]]!              \n\t"
        				"vst1.8         {d2}, [%[dst0_asm]]!              \n\t"
        				"vst1.8         {d1}, [%[dst1_asm]]!              \n\t"
        				"vst1.8         {d3}, [%[dst1_asm]]!              \n\t"
        		         : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
        		         :  //[srcY] "r" (srcY)
        		         : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
        		         );
#endif

            }
            else
            {
#if (USE_VLD2_8 == 1)
                __asm__ volatile (
                        "vld2.8         {d0,d1}, [%[src_asm]]              \n\t"
                        "vst1.8         {d0}, [%[dst0_asm]]              \n\t"
                        "vst1.8         {d1}, [%[dst1_asm]]              \n\t"
                         : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
                         :  //[srcY] "r" (srcY)
                         : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                         );
#else
                __asm__ volatile (
                        "vld1.8         {d0,d1}, [%[src_asm]]              \n\t"
                        "vuzp.8         d0, d1              \n\t"
                        "vst1.8         {d0}, [%[dst0_asm]]!              \n\t"
                        "vst1.8         {d1}, [%[dst1_asm]]!              \n\t"
                        : [dst0_asm] "+r" (dst0_asm), [dst1_asm] "+r" (dst1_asm), [src_asm] "+r" (src_asm)
                        :  //[srcY] "r" (srcY)
                        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                        );
#endif

            }

			ptr += 32;  //32 byte in one process.
		}
        ptr += (32-nLeftValidLine)*32;
    }
}

int SoftwarePictureScaler(ScalerParameter *cdx_scaler_para)
{
	map32x32_to_yuv_Y(cdx_scaler_para->addr_y_in, (unsigned char*)cdx_scaler_para->addr_y_out, cdx_scaler_para->width_out, cdx_scaler_para->height_out);
	map32x32_to_yuv_C(cdx_scaler_para->mode,cdx_scaler_para->addr_c_in, (unsigned char*)cdx_scaler_para->addr_u_out, (unsigned char*)cdx_scaler_para->addr_v_out, cdx_scaler_para->width_out / 2, cdx_scaler_para->height_out / 2);

	return 0;
}

void convert_mb32_to_yv12(ScalerParameter *cdx_scaler_para,void *dstYUV420,cedarv_picture_t *pVdecBuffer)
{
    cedarv_picture_t    *pict = pVdecBuffer;
    ;
	int display_height_align;
	int display_width_align;
	int dst_c_stride;
	int dst_y_size,dst_c_size ,stride;

    int             width_out;
    int             height_out;
    unsigned int    addr_y_out;
    unsigned int    addr_v_out;
    unsigned int    addr_u_out;


    stride = (pict->display_width+15)&~15;
    dst_y_size = stride * pict->display_height;
    dst_c_stride = ALIGN(stride / 2, 16);

	dst_c_size = dst_c_stride * pict->display_height / 2;
	width_out = stride;
	height_out = pict->display_height;
	addr_y_out = dstYUV420;
	addr_v_out = addr_y_out + dst_y_size;
	addr_u_out = addr_v_out + dst_c_size;

	cdx_scaler_para->mode = 0;
	cdx_scaler_para->format_in = (pict->pixel_format == CEDARV_PIXEL_FORMAT_MB_UV_COMBINE_YUV422) ? CONVERT_COLOR_FORMAT_YUV422MB : CONVERT_COLOR_FORMAT_YUV420MB;
	cdx_scaler_para->format_out = CONVERT_COLOR_FORMAT_YUV420PLANNER;
	cdx_scaler_para->width_in = ALIGN(pict->width, 32);
	cdx_scaler_para->height_in = ALIGN(pict->height, 32);
	cdx_scaler_para->addr_y_in = (void*)cedarv_address_phy2vir(pict->y);
	cdx_scaler_para->addr_c_in = (void*)cedarv_address_phy2vir(pict->u);
	cdx_scaler_para->width_out  = width_out;
	cdx_scaler_para->height_out = height_out;
	cdx_scaler_para->addr_y_out = addr_y_out;
	cdx_scaler_para->addr_v_out = addr_v_out;
	cdx_scaler_para->addr_u_out = addr_u_out;

	if(SoftwarePictureScaler(cdx_scaler_para) == 0){
		printf("SoftwarePictureScaler ok \n");
	}
	else {
		printf("Scaler fail!\n");
	}
}
