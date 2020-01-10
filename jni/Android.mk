LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := ffmpeg
LOCAL_SRC_FILES := $(LOCAL_PATH)/ffmpeg/libs/armeabi-v7a/libffmpeg.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/ffmpeg
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := pack
LOCAL_SRC_FILES := pack.c
LOCAL_SHARED_LIBRARIES += ffmpeg
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)
