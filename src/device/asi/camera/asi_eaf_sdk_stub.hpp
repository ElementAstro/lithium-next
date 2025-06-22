/*
 * asi_eaf_sdk_stub.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI EAF (Electronic Auto Focuser) SDK stub interface

*************************************************/

#pragma once

#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

// EAF SDK types and constants
typedef enum {
    EAF_SUCCESS = 0,
    EAF_ERROR_INVALID_INDEX,
    EAF_ERROR_INVALID_ID,
    EAF_ERROR_INVALID_CONTROL_TYPE,
    EAF_ERROR_CAMERA_CLOSED,
    EAF_ERROR_CAMERA_REMOVED,
    EAF_ERROR_INVALID_PATH,
    EAF_ERROR_INVALID_FILEFORMAT,
    EAF_ERROR_INVALID_SIZE,
    EAF_ERROR_INVALID_IMGTYPE,
    EAF_ERROR_OUTOF_BOUNDARY,
    EAF_ERROR_TIMEOUT,
    EAF_ERROR_INVALID_SEQUENCE,
    EAF_ERROR_BUFFER_TOO_SMALL,
    EAF_ERROR_VIDEO_MODE_ACTIVE,
    EAF_ERROR_EXPOSURE_IN_PROGRESS,
    EAF_ERROR_GENERAL_ERROR,
    EAF_ERROR_INVALID_MODE,
    EAF_ERROR_END
} EAF_ERROR_CODE;

typedef struct {
    int ID;
    char Name[64];
    int MaxStep;
    bool IsReverse;
    bool HasBacklash;
    bool HasTempComp;
    bool HasBeeper;
    bool HasHandController;
} EAF_INFO;

#ifdef LITHIUM_ASI_CAMERA_ENABLED
// Actual EAF SDK functions
extern int EAFGetNum();
extern EAF_ERROR_CODE EAFGetID(int index, int* ID);
extern EAF_ERROR_CODE EAFGetProperty(int ID, EAF_INFO* pInfo);
extern EAF_ERROR_CODE EAFOpen(int ID);
extern EAF_ERROR_CODE EAFClose(int ID);
extern EAF_ERROR_CODE EAFGetPosition(int ID, int* position);
extern EAF_ERROR_CODE EAFMove(int ID, int position);
extern EAF_ERROR_CODE EAFIsMoving(int ID, bool* isMoving);
extern EAF_ERROR_CODE EAFStop(int ID);
extern EAF_ERROR_CODE EAFCalibrate(int ID);
extern EAF_ERROR_CODE EAFGetTemp(int ID, float* temperature);
extern EAF_ERROR_CODE EAFGetFirmwareVersion(int ID, char* version);
extern EAF_ERROR_CODE EAFSetBacklash(int ID, int backlash);
extern EAF_ERROR_CODE EAFGetBacklash(int ID, int* backlash);
extern EAF_ERROR_CODE EAFSetReverse(int ID, bool reverse);
extern EAF_ERROR_CODE EAFGetReverse(int ID, bool* reverse);
extern EAF_ERROR_CODE EAFSetBeep(int ID, bool beep);
extern EAF_ERROR_CODE EAFGetBeep(int ID, bool* beep);

#else
// Stub implementations
inline int EAFGetNum() { return 1; }
inline EAF_ERROR_CODE EAFGetID(int, int* ID) { if (ID) *ID = 0; return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFGetProperty(int, EAF_INFO* pInfo) {
    if (pInfo) {
        pInfo->ID = 0;
        strcpy(pInfo->Name, "EAF Simulator");
        pInfo->MaxStep = 10000;
        pInfo->IsReverse = false;
        pInfo->HasBacklash = true;
        pInfo->HasTempComp = true;
        pInfo->HasBeeper = true;
        pInfo->HasHandController = false;
    }
    return EAF_SUCCESS;
}
inline EAF_ERROR_CODE EAFOpen(int) { return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFClose(int) { return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFGetPosition(int, int* position) { if (position) *position = 5000; return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFMove(int, int) { return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFIsMoving(int, bool* isMoving) { if (isMoving) *isMoving = false; return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFStop(int) { return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFCalibrate(int) { return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFGetTemp(int, float* temperature) { if (temperature) *temperature = 23.5f; return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFGetFirmwareVersion(int, char* version) { if (version) strcpy(version, "1.2.0"); return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFSetBacklash(int, int) { return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFGetBacklash(int, int* backlash) { if (backlash) *backlash = 50; return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFSetReverse(int, bool) { return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFGetReverse(int, bool* reverse) { if (reverse) *reverse = false; return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFSetBeep(int, bool) { return EAF_SUCCESS; }
inline EAF_ERROR_CODE EAFGetBeep(int, bool* beep) { if (beep) *beep = true; return EAF_SUCCESS; }

#endif

#ifdef __cplusplus
}
#endif
