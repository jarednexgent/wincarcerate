// Debug.h
#ifndef DEBUG_H
#define DEBUG_H

#include <windows.h>
#include "Common.h"

// Helper macros for string widening
#define DBG_WIDEN2(x) L##x
#define DBG_WIDEN(x)  DBG_WIDEN2(x)

#if defined(LOG_TO_CONSOLE)

BOOL   InitLog(void);
VOID   DeinitLog(void);
LPCSTR ExtractFileName(LPCSTR szFilePath);
VOID   DbgPrintW(const WCHAR* fmt, ...);

#define __FILENAME__ \
    ExtractFileName(__FILE__)

#define PRNT_WIN_ERR(ApiName) \
    DbgPrintW(L"[!] %s Failed (Error: %lu) [%S:%d]\n", \
        DBG_WIDEN(#ApiName), GetLastError(), __FILENAME__, __LINE__)

#define PRNT_NT_ERR(ApiName, Status) \
    DbgPrintW(L"[!] %s Failed (Error: 0x%0.8lX) [%S:%d]\n", \
        DBG_WIDEN(#ApiName), (ULONG)Status, __FILENAME__, __LINE__)

#else 

#define InitLog(...)                  ((BOOL)1)
#define DeinitLog(...)                ((void)0)
#define ExtractFileName(szFilePath)   ((LPCSTR)0)
#define DbgPrintW(...)                ((void)0)
#define PRNT_WIN_ERR(ApiName)         ((void)0)
#define PRNT_NT_ERR(ApiName, Status)  ((void)0)
#define __FILENAME__                  ""

#endif  // LOG_TO_CONSOLE

#endif // DEBUG_H

