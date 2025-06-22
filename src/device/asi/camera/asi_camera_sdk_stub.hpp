/*
 * asi_camera_sdk_stub.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera SDK Stub Implementation

This file provides stub definitions for the ASI Camera SDK types and functions
when the actual SDK is not available, allowing for compilation and testing
without hardware dependencies.

*************************************************/

#pragma once

// ASI Camera SDK stub definitions
#ifndef LITHIUM_ASI_CAMERA_ENABLED

typedef enum {
    ASI_SUCCESS = 0,
    ASI_ERROR_INVALID_INDEX,
    ASI_ERROR_INVALID_ID,
    ASI_ERROR_INVALID_CONTROL_TYPE,
    ASI_ERROR_CAMERA_CLOSED,
    ASI_ERROR_CAMERA_REMOVED,
    ASI_ERROR_INVALID_PATH,
    ASI_ERROR_INVALID_FILEFORMAT,
    ASI_ERROR_INVALID_SIZE,
    ASI_ERROR_INVALID_IMGTYPE,
    ASI_ERROR_OUTOF_BOUNDARY,
    ASI_ERROR_TIMEOUT,
    ASI_ERROR_INVALID_SEQUENCE,
    ASI_ERROR_BUFFER_TOO_SMALL,
    ASI_ERROR_VIDEO_MODE_ACTIVE,
    ASI_ERROR_EXPOSURE_IN_PROGRESS,
    ASI_ERROR_GENERAL_ERROR,
    ASI_ERROR_INVALID_MODE,
    ASI_ERROR_END
} ASI_ERROR_CODE;

typedef enum {
    ASI_IMG_RAW8 = 0,
    ASI_IMG_RGB24,
    ASI_IMG_RAW16,
    ASI_IMG_Y8,
    ASI_IMG_END
} ASI_IMG_TYPE;

typedef enum {
    ASI_GUIDE_NORTH = 0,
    ASI_GUIDE_SOUTH,
    ASI_GUIDE_EAST,
    ASI_GUIDE_WEST
} ASI_GUIDE_DIRECTION;

typedef enum {
    ASI_FLIP_NONE = 0,
    ASI_FLIP_HORIZ,
    ASI_FLIP_VERT,
    ASI_FLIP_BOTH
} ASI_FLIP_STATUS;

typedef enum {
    ASI_MODE_NORMAL = 0,
    ASI_MODE_TRIG_SOFT,
    ASI_MODE_TRIG_RISE_EDGE,
    ASI_MODE_TRIG_FALL_EDGE,
    ASI_MODE_TRIG_SOFT_EDGE,
    ASI_MODE_TRIG_HIGH,
    ASI_MODE_TRIG_LOW,
    ASI_MODE_END
} ASI_CAMERA_MODE;

typedef enum {
    ASI_BAYER_RG = 0,
    ASI_BAYER_BG,
    ASI_BAYER_GR,
    ASI_BAYER_GB
} ASI_BAYER_PATTERN;

typedef enum {
    ASI_EXPOSURE_IDLE = 0,
    ASI_EXPOSURE_WORKING,
    ASI_EXPOSURE_SUCCESS,
    ASI_EXPOSURE_FAILED
} ASI_EXPOSURE_STATUS;

typedef enum {
    ASI_GAIN = 0,
    ASI_EXPOSURE,
    ASI_GAMMA,
    ASI_WB_R,
    ASI_WB_B,
    ASI_OFFSET,
    ASI_BANDWIDTHOVERLOAD,
    ASI_OVERCLOCK,
    ASI_TEMPERATURE,
    ASI_FLIP,
    ASI_AUTO_MAX_GAIN,
    ASI_AUTO_MAX_EXP,
    ASI_AUTO_TARGET_BRIGHTNESS,
    ASI_HARDWARE_BIN,
    ASI_HIGH_SPEED_MODE,
    ASI_COOLER_POWER_PERC,
    ASI_TARGET_TEMP,
    ASI_COOLER_ON,
    ASI_MONO_BIN,
    ASI_FAN_ON,
    ASI_PATTERN_ADJUST,
    ASI_ANTI_DEW_HEATER,
    ASI_CONTROL_TYPE_END
} ASI_CONTROL_TYPE;

typedef enum {
    ASI_FALSE = 0,
    ASI_TRUE = 1
} ASI_BOOL;

typedef struct _ASI_CAMERA_INFO {
    char Name[64];
    int CameraID;
    long MaxHeight;
    long MaxWidth;
    ASI_BOOL IsColorCam;
    ASI_BAYER_PATTERN BayerPattern;
    int SupportedBins[16];
    ASI_IMG_TYPE SupportedVideoFormat[8];
    double PixelSize;
    ASI_BOOL MechanicalShutter;
    ASI_BOOL ST4Port;
    ASI_BOOL IsCoolerCam;
    ASI_BOOL IsUSB3HOST;
    ASI_BOOL IsUSB3Camera;
    double ElecPerADU;
    int BitDepth;
    ASI_BOOL IsTriggerCam;
    char Unused[16];
} ASI_CAMERA_INFO;

typedef struct _ASI_CONTROL_CAPS {
    char Name[64];
    char Description[128];
    long MaxValue;
    long MinValue;
    long DefaultValue;
    ASI_BOOL IsAutoSupported;
    ASI_BOOL IsWritable;
    ASI_CONTROL_TYPE ControlType;
    char Unused[32];
} ASI_CONTROL_CAPS;

typedef struct _ASI_ID {
    unsigned char id[8];
} ASI_ID;

// Stub function declarations
extern "C" {
    int ASIGetNumOfConnectedCameras();
    ASI_ERROR_CODE ASIGetCameraProperty(ASI_CAMERA_INFO *pASICameraInfo, int iCameraIndex);
    ASI_ERROR_CODE ASIGetCameraPropertyByID(int iCameraID, ASI_CAMERA_INFO *pASICameraInfo);
    ASI_ERROR_CODE ASIOpenCamera(int iCameraID);
    ASI_ERROR_CODE ASIInitCamera(int iCameraID);
    ASI_ERROR_CODE ASICloseCamera(int iCameraID);
    ASI_ERROR_CODE ASIGetNumOfControls(int iCameraID, int *piNumberOfControls);
    ASI_ERROR_CODE ASIGetControlCaps(int iCameraID, int iControlIndex, ASI_CONTROL_CAPS *pControlCaps);
    ASI_ERROR_CODE ASIGetControlValue(int iCameraID, ASI_CONTROL_TYPE ControlType, long *plValue, ASI_BOOL *pbAuto);
    ASI_ERROR_CODE ASISetControlValue(int iCameraID, ASI_CONTROL_TYPE ControlType, long lValue, ASI_BOOL bAuto);
    ASI_ERROR_CODE ASISetROIFormat(int iCameraID, int iWidth, int iHeight, int iBin, ASI_IMG_TYPE Img_type);
    ASI_ERROR_CODE ASIGetROIFormat(int iCameraID, int *piWidth, int *piHeight, int *piBin, ASI_IMG_TYPE *pImg_type);
    ASI_ERROR_CODE ASISetStartPos(int iCameraID, int iStartX, int iStartY);
    ASI_ERROR_CODE ASIGetStartPos(int iCameraID, int *piStartX, int *piStartY);
    ASI_ERROR_CODE ASIGetDroppedFrames(int iCameraID, int *piDropFrames);
    ASI_ERROR_CODE ASIStartExposure(int iCameraID, ASI_BOOL bIsDark);
    ASI_ERROR_CODE ASIStopExposure(int iCameraID);
    ASI_ERROR_CODE ASIGetExpStatus(int iCameraID, ASI_EXPOSURE_STATUS *pExpStatus);
    ASI_ERROR_CODE ASIGetDataAfterExp(int iCameraID, unsigned char *pBuffer, long lBuffSize);
    ASI_ERROR_CODE ASIGetID(int iCameraID, ASI_ID *pID);
    ASI_ERROR_CODE ASISetID(int iCameraID, ASI_ID ID);
    ASI_ERROR_CODE ASIGetGainOffset(int iCameraID, int *pOffset_HighestDR, int *pOffset_UnityGain, int *pGain_LowestRN, int *pOffset_LowestRN);
    const char* ASIGetSDKVersion();
    ASI_ERROR_CODE ASIGetCameraSupportMode(int iCameraID, ASI_CAMERA_MODE *pSupportedMode);
    ASI_ERROR_CODE ASIGetCameraMode(int iCameraID, ASI_CAMERA_MODE *mode);
    ASI_ERROR_CODE ASISetCameraMode(int iCameraID, ASI_CAMERA_MODE mode);
    ASI_ERROR_CODE ASISendSoftTrigger(int iCameraID, ASI_BOOL bStart);
    ASI_ERROR_CODE ASIStartVideoCapture(int iCameraID);
    ASI_ERROR_CODE ASIStopVideoCapture(int iCameraID);
    ASI_ERROR_CODE ASIGetVideoData(int iCameraID, unsigned char *pBuffer, long lBuffSize, int iWaitms);
    ASI_ERROR_CODE ASIPulseGuideOn(int iCameraID, ASI_GUIDE_DIRECTION direction);
    ASI_ERROR_CODE ASIPulseGuideOff(int iCameraID, ASI_GUIDE_DIRECTION direction);
    ASI_ERROR_CODE ASIStartGuide(int iCameraID, ASI_GUIDE_DIRECTION direction, int iDurationms);
    ASI_ERROR_CODE ASIStopGuide(int iCameraID, ASI_GUIDE_DIRECTION direction);
    ASI_ERROR_CODE ASIGetSerialNumber(int iCameraID, ASI_ID *pID);
    ASI_ERROR_CODE ASISetTriggerOutputIOConf(int iCameraID, int pin, ASI_BOOL bPinHigh, long lDelay, long lDuration);
    ASI_ERROR_CODE ASIGetTriggerOutputIOConf(int iCameraID, int pin, ASI_BOOL *bPinHigh, long *lDelay, long *lDuration);
}

#endif // LITHIUM_ASI_CAMERA_ENABLED
