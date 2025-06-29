/*
 * qhyccd_stub.h
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: QHY SDK stub definitions for compilation

*************************************************/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// QHY SDK return codes
#define QHYCCD_SUCCESS                0
#define QHYCCD_ERROR                 -1
#define QHYCCD_ERROR_NO_DEVICE       -2
#define QHYCCD_ERROR_SETPARAMS       -3
#define QHYCCD_ERROR_GETPARAMS       -4
#define QHYCCD_ERROR_EXPOSING        -5
#define QHYCCD_ERROR_EXPFAILED       -6
#define QHYCCD_ERROR_GETTINGDATA     -7
#define QHYCCD_ERROR_GETTINGFAILED   -8
#define QHYCCD_ERROR_INITCAMERA      -9
#define QHYCCD_ERROR_RELEASECAMERA   -10
#define QHYCCD_ERROR_GETCCDINFO      -11
#define QHYCCD_ERROR_SETCCDRESOLUTION -12

// QHY Camera Handle
typedef struct _QHYCamHandle QHYCamHandle;

// QHY Camera control types
#define CONTROL_BRIGHTNESS           0
#define CONTROL_CONTRAST             1
#define CONTROL_WBR                  2
#define CONTROL_WBB                  3
#define CONTROL_WBG                  4
#define CONTROL_GAMMA                5
#define CONTROL_GAIN                 6
#define CONTROL_OFFSET               7
#define CONTROL_EXPOSURE             8
#define CONTROL_SPEED                9
#define CONTROL_TRANSFERBIT          10
#define CONTROL_CHANNELS             11
#define CONTROL_USBTRAFFIC           12
#define CONTROL_ROWNOISERE           13
#define CONTROL_CURTEMP              14
#define CONTROL_CURPWM               15
#define CONTROL_MANULPWM             16
#define CONTROL_CFWPORT              17
#define CONTROL_COOLER               18
#define CONTROL_ST4PORT              19
#define CAM_COLOR                    20
#define CAM_BIN1X1MODE               21
#define CAM_BIN2X2MODE               22
#define CAM_BIN3X3MODE               23
#define CAM_BIN4X4MODE               24
#define CAM_MECHANICALSHUTTER        25
#define CAM_TRIGER_INTERFACE         26
#define CAM_TECOVERPROTECT_INTERFACE 27
#define CAM_SINGNALCLAMP_INTERFACE   28
#define CAM_FINETONE_INTERFACE       29
#define CAM_SHUTTERMOTORHEATING_INTERFACE 30
#define CAM_CALIBRATEFPN_INTERFACE   31
#define CAM_CHIPTEMPERATURESENSOR_INTERFACE 32
#define CAM_USBREADOUTSLOWEST_INTERFACE 33

// QHY Image types
#define QHYCCD_RAW8                  0x00
#define QHYCCD_RAW16                 0x01
#define QHYCCD_RGB24                 0x02
#define QHYCCD_RGB48                 0x03

// Function declarations (stubs)
int InitQHYCCDResource(void);
int ReleaseQHYCCDResource(void);
int GetQHYCCDNum(void);
int GetQHYCCDId(int index, char* id);
QHYCamHandle* OpenQHYCCD(char* id);
int CloseQHYCCD(QHYCamHandle* handle);
int InitQHYCCD(QHYCamHandle* handle);
int SetQHYCCDStreamMode(QHYCamHandle* handle, unsigned char mode);
int SetQHYCCDResolution(QHYCamHandle* handle, unsigned int x, unsigned int y, unsigned int xsize, unsigned int ysize);
int SetQHYCCDBinMode(QHYCamHandle* handle, unsigned int wbin, unsigned int hbin);
int SetQHYCCDBitsMode(QHYCamHandle* handle, unsigned int bits);
int ControlQHYCCD(QHYCamHandle* handle, unsigned int controlId, double dValue);
int IsQHYCCDControlAvailable(QHYCamHandle* handle, unsigned int controlId);
int GetQHYCCDParamMinMaxStep(QHYCamHandle* handle, unsigned int controlId, double* min, double* max, double* step);
int GetQHYCCDParam(QHYCamHandle* handle, unsigned int controlId);
int ExpQHYCCDSingleFrame(QHYCamHandle* handle);
int GetQHYCCDSingleFrame(QHYCamHandle* handle, unsigned int* w, unsigned int* h, unsigned int* bpp, unsigned int* channels, unsigned char* imgdata);
int CancelQHYCCDExposingAndReadout(QHYCamHandle* handle);
int GetQHYCCDChipInfo(QHYCamHandle* handle, double* chipw, double* chiph, unsigned int* imagew, unsigned int* imageh, double* pixelw, double* pixelh, unsigned int* bpp);
int GetQHYCCDEffectiveArea(QHYCamHandle* handle, unsigned int* startX, unsigned int* startY, unsigned int* sizeX, unsigned int* sizeY);
int GetQHYCCDOverScanArea(QHYCamHandle* handle, unsigned int* startX, unsigned int* startY, unsigned int* sizeX, unsigned int* sizeY);
int SetQHYCCDParam(QHYCamHandle* handle, unsigned int controlId, double dValue);
int GetQHYCCDMemLength(QHYCamHandle* handle);
int GetQHYCCDCameraStatus(QHYCamHandle* handle, unsigned char* status);
int GetQHYCCDShutterStatus(QHYCamHandle* handle);
int ControlQHYCCDShutter(QHYCamHandle* handle, unsigned char targetStatus);
int GetQHYCCDHumidity(QHYCamHandle* handle, double* hd);
int QHYCCDI2CTwoWrite(QHYCamHandle* handle, unsigned short addr, unsigned short value);
int QHYCCDI2CTwoRead(QHYCamHandle* handle, unsigned short addr);
int GetQHYCCDReadingProgress(QHYCamHandle* handle);
int QHYCCDVendRequestWrite(QHYCamHandle* handle, unsigned char req, unsigned short value, unsigned short index, unsigned int length, unsigned char* data);
int QHYCCDVendRequestRead(QHYCamHandle* handle, unsigned char req, unsigned short value, unsigned short index, unsigned int length, unsigned char* data);
char* GetTimeStamp(void);
int SetQHYCCDLogLevel(unsigned char i);
void EnableQHYCCDMessage(bool enable);
void EnableQHYCCDLogFile(bool enable);
char* GetQHYCCDSDKVersion(void);
unsigned int GetQHYCCDType(QHYCamHandle* handle);
char* GetQHYCCDModel(QHYCamHandle* handle);
int SetQHYCCDBufferNumber(QHYCamHandle* handle, unsigned int value);
int GetQHYCCDNumberOfReadModes(QHYCamHandle* handle, unsigned int* numModes);
int GetQHYCCDReadModeResolution(QHYCamHandle* handle, unsigned int modeNumber, unsigned int* width, unsigned int* height);
int GetQHYCCDReadModeName(QHYCamHandle* handle, unsigned int modeNumber, char* name);
int SetQHYCCDReadMode(QHYCamHandle* handle, unsigned int modeNumber);
int GetQHYCCDReadMode(QHYCamHandle* handle, unsigned int* modeNumber);

#ifdef __cplusplus
}
#endif
