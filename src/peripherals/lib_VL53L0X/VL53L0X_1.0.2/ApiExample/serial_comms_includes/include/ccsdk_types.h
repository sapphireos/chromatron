// ======================================================================================
//                               COMMS & CAPTURE SDK
// ======================================================================================
// Copyright 2008-2014 STMicroelectronics. All Rights Reserved.
//
// NOTICE:  STMicroelectronics permits you to use this file in accordance with the terms
// of the STMicroelectronics license agreement accompanying it.
// ======================================================================================
#ifndef CCSDK_TYPES_H
#define CCSDK_TYPES_H

#ifndef _WINDOWS_
#define ASSERT

#if defined(_WIN64)
 typedef unsigned __int64 ULONG_PTR;
 typedef unsigned __int64 PULONG;
#else
 typedef unsigned long ULONG_PTR;
 typedef unsigned long PULONG;
#endif

typedef ULONG_PTR DWORD_PTR;
#define MAKEWORD(a, b)      ((WORD)(((BYTE)((DWORD_PTR)(a) & 0xff)) | ((WORD)((BYTE)((DWORD_PTR)(b) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)((DWORD_PTR)(a) & 0xffff)) | ((DWORD)((WORD)((DWORD_PTR)(b) & 0xffff))) << 16))
#define LOWORD(l)           ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l)           ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w)           ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w)           ((BYTE)((DWORD_PTR)(w) >> 8))

typedef unsigned long   DWORD;
typedef unsigned long*  PDWORD;
typedef int             BOOL;
typedef unsigned int    UINT;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned short  USHORT;
typedef unsigned short* PUSHORT;
typedef char*           PCHAR;
typedef unsigned char*  PBYTE;
typedef long            LONG;
typedef void*           PVOID;
typedef unsigned long   ULONG;
typedef char*           LPSTR;

#undef FALSE
#undef TRUE
#undef NULL

#define FALSE   0
#define TRUE    1
#define NULL    0

#define IN
#define OUT

#endif // _WINDOWS_ (ndef)
#endif // CCSDK_TYPES_H
