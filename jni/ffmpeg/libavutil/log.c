/*
 * log functions
 * Copyright (c) 2003 Michel Bardiaux
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
 * logging functions
 */

#include <unistd.h>
#include <stdlib.h>
#include "avutil.h"
//#include "log.h"
#include <android/log.h>
static int use_log_report = 0;

#define FF_LOG_TAG     "FFMPEG"


#define FF_LOG_UNKNOWN        ANDROID_LOG_UNKNOWN
#define FF_LOG_DEFAULT        ANDROID_LOG_DEFAULT


#define FF_LOG_VERBOSE        ANDROID_LOG_VERBOSE
#define FF_LOG_DEBUG          ANDROID_LOG_DEBUG
#define FF_LOG_INFO           ANDROID_LOG_INFO
#define FF_LOG_WARN           ANDROID_LOG_WARN
#define FF_LOG_ERROR          ANDROID_LOG_ERROR
#define FF_LOG_FATAL          ANDROID_LOG_FATAL
#define FF_LOG_SILENT         ANDROID_LOG_SILENT



#define FF_LOG_VERBOSE        ANDROID_LOG_VERBOSE
#define FF_LOG_DEBUG          ANDROID_LOG_DEBUG
#define FF_LOG_INFO           ANDROID_LOG_INFO
#define FF_LOG_WARN           ANDROID_LOG_WARN
#define FF_LOG_ERROR          ANDROID_LOG_ERROR
#define FF_LOG_FATAL          ANDROID_LOG_FATAL
#define FF_LOG_SILENT         ANDROID_LOG_SILENT

// 打印可变参数
#define VLOG(level, TAG, ...)    ((void)__android_log_vprint(level, TAG, __VA_ARGS__))
#define VLOGV(...)  VLOG(FF_LOG_VERBOSE,   FF_LOG_TAG, __VA_ARGS__)
#define VLOGD(...)  VLOG(FF_LOG_DEBUG,     FF_LOG_TAG, __VA_ARGS__)
#define VLOGI(...)  VLOG(FF_LOG_INFO,      FF_LOG_TAG, __VA_ARGS__)
#define VLOGW(...)  VLOG(FF_LOG_WARN,      FF_LOG_TAG, __VA_ARGS__)
#define VLOGE(...)  VLOG(FF_LOG_ERROR,     FF_LOG_TAG, __VA_ARGS__)


#define ALOG(level, TAG, ...)    ((void)__android_log_print(level, TAG, __VA_ARGS__))
#define ALOGV(...)  ALOG(FF_LOG_VERBOSE,   FF_LOG_TAG, __VA_ARGS__)
#define ALOGD(...)  ALOG(FF_LOG_DEBUG,     FF_LOG_TAG, __VA_ARGS__)
#define ALOGI(...)  ALOG(FF_LOG_INFO,      FF_LOG_TAG, __VA_ARGS__)
#define ALOGW(...)  ALOG(FF_LOG_WARN,      FF_LOG_TAG, __VA_ARGS__)
#define ALOGE(...)  ALOG(FF_LOG_ERROR,     FF_LOG_TAG, __VA_ARGS__)

#define LOGE(format, ...)  __android_log_print(ANDROID_LOG_ERROR, FF_LOG_TAG, format, ##__VA_ARGS__)
#define LOGI(format, ...)  __android_log_print(ANDROID_LOG_INFO,  FF_LOG_TAG, format, ##__VA_ARGS__)

// 原样输出FFmpeg日志
static void ffp_log_callback_brief(void *ptr, int level, const char *fmt, va_list vl)
{
    int ffplv = FF_LOG_VERBOSE;
    if (level <= AV_LOG_ERROR)
        ffplv = FF_LOG_ERROR;
    else if (level <= AV_LOG_WARNING)
        ffplv = FF_LOG_WARN;
    else if (level <= AV_LOG_INFO)
        ffplv = FF_LOG_INFO;
    else if (level <= AV_LOG_VERBOSE)
        ffplv = FF_LOG_VERBOSE;
    else
        ffplv = FF_LOG_DEBUG;


    if (level <= AV_LOG_INFO)
        VLOG(ffplv, FF_LOG_TAG, fmt, vl);
}


static int av_log_level = AV_LOG_INFO;
static int flags;

#if defined(_WIN32) && !defined(__MINGW32CE__)
#include <windows.h>
static const uint8_t color[] = {12,12,12,14,7,7,7};
static int16_t background, attr_orig;
static HANDLE con;
#define set_color(x)  SetConsoleTextAttribute(con, background | color[x])
#define reset_color() SetConsoleTextAttribute(con, attr_orig)
#else
static const uint8_t color[]={0x41,0x41,0x11,0x03,9,9,9};
#define set_color(x)  fprintf(stderr, "\033[%d;3%dm", color[x]>>4, color[x]&15)
#define reset_color() fprintf(stderr, "\033[0m")
#endif
static int use_color=-1;

#undef fprintf
static void colored_fputs(int level, const char *str){
    if(use_color<0){
#if defined(_WIN32) && !defined(__MINGW32CE__)
        CONSOLE_SCREEN_BUFFER_INFO con_info;
        con = GetStdHandle(STD_ERROR_HANDLE);
        use_color = (con != INVALID_HANDLE_VALUE) && !getenv("NO_COLOR") && !getenv("AV_LOG_FORCE_NOCOLOR");
        if (use_color) {
            GetConsoleScreenBufferInfo(con, &con_info);
            attr_orig  = con_info.wAttributes;
            background = attr_orig & 0xF0;
        }
#elif HAVE_ISATTY
        use_color= !getenv("NO_COLOR") && !getenv("AV_LOG_FORCE_NOCOLOR") &&
            (getenv("TERM") && isatty(2) || getenv("AV_LOG_FORCE_COLOR"));
#else
        use_color= getenv("AV_LOG_FORCE_COLOR") && !getenv("NO_COLOR") && !getenv("AV_LOG_FORCE_NOCOLOR");
#endif
    }

    if(use_color){
        set_color(level);
    }
    fputs(str, stderr);
    if(use_color){
        reset_color();
    }
}

const char* av_default_item_name(void* ptr){
    return (*(AVClass**)ptr)->class_name;
}

static void sanitize(uint8_t *line){
    while(*line){
        if(*line < 0x08 || (*line > 0x0D && *line < 0x20))
            *line='?';
        line++;
    }
}

void av_log_default_callback(void* ptr, int level, const char* fmt, va_list vl)
{
    static int print_prefix=1;
    static int count;
    static char prev[1024];
    char line[1024];
    static int is_atty;
    AVClass* avc= ptr ? *(AVClass**)ptr : NULL;
    if(level>av_log_level)
        return;
    line[0]=0;
#undef fprintf
    if(print_prefix && avc) {
        if (avc->parent_log_context_offset) {
            AVClass** parent= *(AVClass***)(((uint8_t*)ptr) + avc->parent_log_context_offset);
            if(parent && *parent){
                snprintf(line, sizeof(line), "[%s @ %p] ", (*parent)->item_name(parent), parent);
            }
        }
        snprintf(line + strlen(line), sizeof(line) - strlen(line), "[%s @ %p] ", avc->item_name(ptr), ptr);
    }

    vsnprintf(line + strlen(line), sizeof(line) - strlen(line), fmt, vl);

    print_prefix = strlen(line) && line[strlen(line)-1] == '\n';

#if HAVE_ISATTY
    if(!is_atty) is_atty= isatty(2) ? 1 : -1;
#endif

    if(print_prefix && (flags & AV_LOG_SKIP_REPEATED) && !strcmp(line, prev)){
        count++;
        if(is_atty==1)
            fprintf(stderr, "    Last message repeated %d times\r", count);
        return;
    }
    if(count>0){
        fprintf(stderr, "    Last message repeated %d times\n", count);
        count=0;
    }
    strcpy(prev, line);
    sanitize(line);
    colored_fputs(av_clip(level>>3, 0, 6), line);
}

//static void (*av_log_callback)(void*, int, const char*, va_list) = av_log_default_callback;
static void (*av_log_callback)(void*, int, const char*, va_list) = ffp_log_callback_brief;


void av_log(void* avcl, int level, const char *fmt, ...)
{
    AVClass* avc= avcl ? *(AVClass**)avcl : NULL;
    va_list vl;
    va_start(vl, fmt);
    if(avc && avc->version >= (50<<16 | 15<<8 | 2) && avc->log_level_offset_offset && level>=AV_LOG_FATAL)
        level += *(int*)(((uint8_t*)avcl) + avc->log_level_offset_offset);
    av_vlog(avcl, level, fmt, vl);
    va_end(vl);
}

void av_vlog(void* avcl, int level, const char *fmt, va_list vl)
{
    av_log_callback(avcl, level, fmt, vl);
}

int av_log_get_level(void)
{
    return av_log_level;
}

void av_log_set_level(int level)
{
    av_log_level = level;
}

void av_log_set_flags(int arg)
{
    flags= arg;
}

void av_log_set_callback(void (*callback)(void*, int, const char*, va_list))
{
    av_log_callback = callback;
}
