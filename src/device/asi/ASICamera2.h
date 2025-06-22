/*
 * ASICamera2_stub.h
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI SDK stub definitions for compilation

*************************************************/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ASI SDK return codes
typedef enum ASI_ERROR_CODE {
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

// ASI camera info structure
typedef struct _ASI_CAMERA_INFO {
    char Name[64];
    int CameraID;
    long MaxHeight;
    long MaxWidth;
    int IsColorCam;
    int BayerPattern;
    int SupportedBins[16];
    int SupportedVideoFormat[8];
    double PixelSize;
    int MechanicalShutter;
    int ST4Port;
    int IsCoolerCam;
    int IsUSB3Host;
    int IsUSB3Camera;
    float ElecPerADU;
    int BitDepth;
    int IsTriggerCam;
    char Unused[16];
} ASI_CAMERA_INFO;

// ASI image types
typedef enum ASI_IMG_TYPE {
    ASI_IMG_RAW8 = 0,
    ASI_IMG_RGB24,
    ASI_IMG_RAW16,
    ASI_IMG_Y8,
    ASI_IMG_END
} ASI_IMG_TYPE;

// ASI control types
typedef enum ASI_CONTROL_TYPE {
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

// ASI guide directions
typedef enum ASI_GUIDE_DIRECTION {
    ASI_GUIDE_NORTH = 0,
    ASI_GUIDE_SOUTH,
    ASI_GUIDE_EAST,
    ASI_GUIDE_WEST
} ASI_GUIDE_DIRECTION;

// ASI flip modes
typedef enum ASI_FLIP_STATUS {
    ASI_FLIP_NONE = 0,
    ASI_FLIP_HORIZ,
    ASI_FLIP_VERT,
    ASI_FLIP_BOTH
} ASI_FLIP_STATUS;

// ASI camera modes
typedef enum ASI_CAMERA_MODE {
    ASI_MODE_NORMAL = 0,
    ASI_MODE_TRIGGER_SOFT_EDGE,
    ASI_MODE_TRIGGER_RISE_EDGE,
    ASI_MODE_TRIGGER_FALL_EDGE,
    ASI_MODE_TRIGGER_SOFT_LEVEL,
    ASI_MODE_TRIGGER_HIGH_LEVEL,
    ASI_MODE_TRIGGER_LOW_LEVEL,
    ASI_MODE_END
} ASI_CAMERA_MODE;

// ASI trig output modes
typedef enum ASI_TRIG_OUTPUT {
    ASI_TRIG_OUTPUT_PINA = 0,
    ASI_TRIG_OUTPUT_PINB,
    ASI_TRIG_OUTPUT_NONE = -1
} ASI_TRIG_OUTPUT;

// ASI exposure status
typedef enum ASI_EXPOSURE_STATUS {
    ASI_EXP_IDLE = 0,
    ASI_EXP_WORKING,
    ASI_EXP_SUCCESS,
    ASI_EXP_FAILED
} ASI_EXPOSURE_STATUS;

// ASI boolean type
typedef enum ASI_BOOL {
    ASI_FALSE = 0,
    ASI_TRUE
} ASI_BOOL;

// ASI bayer patterns
typedef enum ASI_BAYER_PATTERN {
    ASI_BAYER_RG = 0,
    ASI_BAYER_BG,
    ASI_BAYER_GR,
    ASI_BAYER_GB
} ASI_BAYER_PATTERN;

// ASI control capabilities
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

// ASI supported modes
typedef struct _ASI_SUPPORTED_MODE {
    ASI_CAMERA_MODE SupportedCameraMode[16];
} ASI_SUPPORTED_MODE;

// Additional type definitions
typedef struct _ASI_ID {
    unsigned char id[8];
} ASI_ID;

typedef struct _ASI_SN {
    unsigned char id[8];
} ASI_SN;

// Function declarations (stubs)
int ASIGetNumOfConnectedCameras(void);
ASI_ERROR_CODE ASIGetCameraProperty(ASI_CAMERA_INFO* pASICameraInfo, int iCameraIndex);
ASI_ERROR_CODE ASIGetCameraPropertyByID(int iCameraID, ASI_CAMERA_INFO* pASICameraInfo);
ASI_ERROR_CODE ASIOpenCamera(int iCameraID);
ASI_ERROR_CODE ASIInitCamera(int iCameraID);
ASI_ERROR_CODE ASICloseCamera(int iCameraID);
ASI_ERROR_CODE ASIGetNumOfControls(int iCameraID, int* piNumberOfControls);
ASI_ERROR_CODE ASIGetControlCaps(int iCameraID, int iControlIndex, ASI_CONTROL_CAPS* pControlCaps);
ASI_ERROR_CODE ASIGetControlValue(int iCameraID, ASI_CONTROL_TYPE ControlType, long* plValue, ASI_BOOL* pbAuto);
ASI_ERROR_CODE ASISetControlValue(int iCameraID, ASI_CONTROL_TYPE ControlType, long lValue, ASI_BOOL bAuto);
ASI_ERROR_CODE ASISetROIFormat(int iCameraID, int iWidth, int iHeight, int iBin, ASI_IMG_TYPE Img_type);
ASI_ERROR_CODE ASIGetROIFormat(int iCameraID, int* piWidth, int* piHeight, int* piBin, ASI_IMG_TYPE* pImg_type);
ASI_ERROR_CODE ASISetStartPos(int iCameraID, int iStartX, int iStartY);
ASI_ERROR_CODE ASIGetStartPos(int iCameraID, int* piStartX, int* piStartY);
ASI_ERROR_CODE ASIGetDroppedFrames(int iCameraID, int* piDropFrames);
ASI_ERROR_CODE ASIEnableDarkSubtract(int iCameraID, char* pcBMPPath);
ASI_ERROR_CODE ASIDisableDarkSubtract(int iCameraID);
ASI_ERROR_CODE ASIStartVideoCapture(int iCameraID);
ASI_ERROR_CODE ASIStopVideoCapture(int iCameraID);
ASI_ERROR_CODE ASIGetVideoData(int iCameraID, unsigned char* pBuffer, long lBuffSize, int iWaitms);
ASI_ERROR_CODE ASIPulseGuideOn(int iCameraID, ASI_GUIDE_DIRECTION direction, int iPulseMS);
ASI_ERROR_CODE ASIPulseGuideOff(int iCameraID, ASI_GUIDE_DIRECTION direction);
ASI_ERROR_CODE ASIStartExposure(int iCameraID, ASI_BOOL bIsDark);
ASI_ERROR_CODE ASIStopExposure(int iCameraID);
ASI_ERROR_CODE ASIGetExpStatus(int iCameraID, ASI_EXPOSURE_STATUS* pExpStatus);
ASI_ERROR_CODE ASIGetDataAfterExp(int iCameraID, unsigned char* pBuffer, long lBuffSize);
ASI_ERROR_CODE ASIGetID(int iCameraID, ASI_ID* pID);
ASI_ERROR_CODE ASISetID(int iCameraID, ASI_ID ID);
ASI_ERROR_CODE ASIGetGainOffset(int iCameraID, int* pOffset_HighestDR, int* pOffset_UnityGain, int* pGain_LowestRN, int* pOffset_LowestRN);
const char* ASIGetSDKVersion(void);
ASI_ERROR_CODE ASIGetCameraSupportMode(int iCameraID, ASI_SUPPORTED_MODE* pSupportedMode);
ASI_ERROR_CODE ASIGetCameraMode(int iCameraID, ASI_CAMERA_MODE* mode);
ASI_ERROR_CODE ASISetCameraMode(int iCameraID, ASI_CAMERA_MODE mode);
ASI_ERROR_CODE ASISendSoftTrigger(int iCameraID, ASI_BOOL bStart);
ASI_ERROR_CODE ASIGetSerialNumber(int iCameraID, ASI_SN* pSN);
ASI_ERROR_CODE ASISetTriggerOutputIOConf(int iCameraID, ASI_TRIG_OUTPUT pin, ASI_BOOL bPinHigh, long lDelay, long lDuration);
ASI_ERROR_CODE ASIGetTriggerOutputIOConf(int iCameraID, ASI_TRIG_OUTPUT pin, ASI_BOOL* bPinHigh, long* lDelay, long* lDuration);

#ifdef __cplusplus
}
#endif
