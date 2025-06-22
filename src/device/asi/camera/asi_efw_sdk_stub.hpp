/*
 * asi_efw_sdk_stub.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI EFW (Electronic Filter Wheel) SDK stub interface

*************************************************/

#pragma once

#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

// EFW SDK types and constants
typedef enum {
    EFW_SUCCESS = 0,
    EFW_ERROR_INVALID_INDEX,
    EFW_ERROR_INVALID_ID,
    EFW_ERROR_INVALID_VALUE,
    EFW_ERROR_REMOVED,
    EFW_ERROR_MOVING,
    EFW_ERROR_ERROR_STATE,
    EFW_ERROR_GENERAL_ERROR,
    EFW_ERROR_NOT_SUPPORTED,
    EFW_ERROR_CLOSED,
    EFW_ERROR_END
} EFW_ERROR_CODE;

typedef struct {
    int ID;
    char Name[64];
    int slotNum;
} EFW_INFO;

#ifdef LITHIUM_ASI_CAMERA_ENABLED
// Actual EFW SDK functions
extern int EFWGetNum();
extern EFW_ERROR_CODE EFWGetID(int index, int* ID);
extern EFW_ERROR_CODE EFWGetProperty(int ID, EFW_INFO* pInfo);
extern EFW_ERROR_CODE EFWOpen(int ID);
extern EFW_ERROR_CODE EFWClose(int ID);
extern EFW_ERROR_CODE EFWGetPosition(int ID, int* position);
extern EFW_ERROR_CODE EFWSetPosition(int ID, int position);
extern EFW_ERROR_CODE EFWCalibrate(int ID);
extern EFW_ERROR_CODE EFWGetFirmwareVersion(int ID, char* version);
extern EFW_ERROR_CODE EFWSetDirection(int ID, bool unidirection);
extern EFW_ERROR_CODE EFWGetDirection(int ID, bool* unidirection);

#else
// Stub implementations
inline int EFWGetNum() { return 1; }
inline EFW_ERROR_CODE EFWGetID(int, int* ID) { if (ID) *ID = 0; return EFW_SUCCESS; }
inline EFW_ERROR_CODE EFWGetProperty(int, EFW_INFO* pInfo) {
    if (pInfo) {
        pInfo->ID = 0;
        strcpy(pInfo->Name, "EFW-7 Simulator");
        pInfo->slotNum = 7;
    }
    return EFW_SUCCESS;
}
inline EFW_ERROR_CODE EFWOpen(int) { return EFW_SUCCESS; }
inline EFW_ERROR_CODE EFWClose(int) { return EFW_SUCCESS; }
inline EFW_ERROR_CODE EFWGetPosition(int, int* position) { if (position) *position = 1; return EFW_SUCCESS; }
inline EFW_ERROR_CODE EFWSetPosition(int, int) { return EFW_SUCCESS; }
inline EFW_ERROR_CODE EFWCalibrate(int) { return EFW_SUCCESS; }
inline EFW_ERROR_CODE EFWGetFirmwareVersion(int, char* version) { if (version) strcpy(version, "1.3.0"); return EFW_SUCCESS; }
inline EFW_ERROR_CODE EFWSetDirection(int, bool) { return EFW_SUCCESS; }
inline EFW_ERROR_CODE EFWGetDirection(int, bool* unidirection) { if (unidirection) *unidirection = false; return EFW_SUCCESS; }

#endif

#ifdef __cplusplus
}
#endif
