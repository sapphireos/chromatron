// ======================================================================================
//                               COMMS & CAPTURE SDK
// ======================================================================================
// Copyright 2008-2014 STMicroelectronics. All Rights Reserved.
//
// NOTICE:  STMicroelectronics permits you to use this file in accordance with the terms
// of the STMicroelectronics license agreement accompanying it.
// ======================================================================================
#ifndef COMMS_PLATFORM_H
#define COMMS_PLATFORM_H

// if windows.h has been included, we don't need the extra typedefs
#ifndef _WINDOWS_
    #include "ccsdk_types.h"
#endif

// Mandatory functions - always supported
typedef DWORD (*P_COMMS_GET_DISPLAY_STRING)     (PCHAR);
typedef DWORD (*P_COMMS_GET_NUM_COMMS_TYPES)    (void);
typedef DWORD (*P_COMMS_GET_COMMS_TYPE_STRING)  (DWORD, PCHAR);
typedef DWORD (*P_COMMS_ENUM_DEVICES)           (DWORD, PBYTE, PDWORD);
typedef DWORD (*P_COMMS_INIT)                   (DWORD, DWORD, PCHAR*);
typedef DWORD (*P_COMMS_FINI)                   (void);
typedef DWORD (*P_COMMS_READ)                   (DWORD, DWORD, DWORD, PBYTE, DWORD);
typedef DWORD (*P_COMMS_WRITE)                  (DWORD, DWORD, DWORD, PBYTE, DWORD);
typedef DWORD (*P_COMMS_WRITE_AND_READ)         (DWORD dwAddress, DWORD dwIndexHi, 
                                                  DWORD dwIndexLo, BYTE* pcWriteValues, 
                                                  BYTE* pcReadValues, DWORD dwBufferSize);

typedef struct tagEVENT_PARAMS
{
    DWORD   dwParam1;
    DWORD   dwParam2;
    DWORD   dwParam3;
    DWORD   dwParam4;
} EVENT_PARAMS, *PEVENT_PARAMS;

typedef struct tagINDEX_DATA_PAIR
{
    DWORD   dwAddress;
    DWORD   dwIndexHi;
    DWORD   dwIndexLo;
    BYTE*   pcData;
    DWORD   dwDataLen;
    struct tagINDEX_DATA_PAIR* pNext;
} INDEX_DATA_PAIR, *PINDEX_DATA_PAIR;

// Optional functions - app can't depend on these being available
typedef DWORD (*P_COMMS_GET_ERROR_TEXT)         (PCHAR);
typedef DWORD (*P_COMMS_WRITE_NON_CONTIGUOUS)   (PEVENT_PARAMS, PINDEX_DATA_PAIR);

// #defines
#define ERROR_TEXT_LENGTH 0x100	// Callers of 'GetErrorText' should set their error text buffer to this size

// Function return codes
enum CP_STATUS
{
    CP_STATUS_OK,
    CP_STATUS_FAIL,
    CP_STATUS_INIT_PARAMETER_ERR,
    CP_STATUS_NUM_STATUS_CODES	// Num of status codes
};

// Comms API version info structure
typedef struct tagCOMMS_VERSION_INFO
{
    char major;
    char minor;
    char build;
    int  revision;
} COMMS_VERSION_INFO, *PCOMMS_VERSION_INFO;

#define CP_SUCCESS(x) (x == CP_STATUS_OK)

#endif // COMMS_PLATFORM_H
