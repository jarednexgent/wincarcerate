// Debug.c
#ifndef LOG_TO_CONSOLE
#define LOG_TO_CONSOLE  // Debug.c always needs the real implementations
#endif
#include <Windows.h>
#include "Common.h"
#include "Debug.h"

#define DBG_STR_MAX_LEN 1024

// ==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==

static BOOL          g_Created = FALSE;
static HANDLE        g_hConsole = INVALID_HANDLE_VALUE;
static SRWLOCK       g_PrintLock = SRWLOCK_INIT;


typedef struct _PERF_TIMER {
    LARGE_INTEGER   Begin;
    LARGE_INTEGER   End;
    LARGE_INTEGER   Frequency;
} PERF_TIMER, * PPERF_TIMER;

static PERF_TIMER    g_PerfTimer = { 0 };

// ==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==

static VOID CreateDebugConsole(void)
{
    HANDLE hOut = INVALID_HANDLE_VALUE;
    BOOL bAllocated = FALSE;

    if (g_Created)
        return;

    if (!GetConsoleWindow())
        bAllocated = AllocConsole();

    if (GetConsoleWindow() || bAllocated)
    {
        if ((hOut = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE)
        {
            SetStdHandle(STD_OUTPUT_HANDLE, hOut);
            SetStdHandle(STD_ERROR_HANDLE, hOut);
            g_hConsole = hOut;
            g_Created = TRUE;
        }
    }
}

static VOID CloseDebugConsole(void)
{
    if (!g_Created)
        return;

    if (g_hConsole != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hConsole);
        g_hConsole = INVALID_HANDLE_VALUE;
    }

    if (FreeConsole())
        g_Created = FALSE;
}

LPCSTR ExtractFileName(LPCSTR szFilePath)
{
    LPCSTR lpszFileName = szFilePath;
    LPCSTR lpch = NULL;

    for (lpch = szFilePath; *lpch; ++lpch)
    {
        if (*lpch == '\\' || *lpch == '/')
            lpszFileName = lpch + 1;
    }

    return lpszFileName;
}


VOID DbgPrintW(const WCHAR* fmt, ...)
{
    WCHAR wBuffer[DBG_STR_MAX_LEN];
    DWORD cbWritten;
    va_list args;

    va_start(args, fmt);
    wvsprintfW(wBuffer, fmt, args);
    va_end(args);

    OutputDebugStringW(wBuffer);

    if (g_hConsole != INVALID_HANDLE_VALUE)
    {
        AcquireSRWLockExclusive(&g_PrintLock);
        WriteConsoleW(g_hConsole, wBuffer, (DWORD)lstrlenW(wBuffer), &cbWritten, NULL);
        ReleaseSRWLockExclusive(&g_PrintLock);
    }
}

BOOL InitLog(void)
{
    CreateDebugConsole();

    if (!QueryPerformanceFrequency(&g_PerfTimer.Frequency))
        return FALSE;

    if (!QueryPerformanceCounter(&g_PerfTimer.Begin))
        return FALSE;

    return TRUE;
}

static VOID PrintElapsedTime(void)
{
    DOUBLE Interval = (DOUBLE)(g_PerfTimer.End.QuadPart - g_PerfTimer.Begin.QuadPart)
        / g_PerfTimer.Frequency.QuadPart;

    INT iWhole = (INT)Interval;
    INT iFrac = (INT)((Interval - iWhole) * 1000);

    DbgPrintW(L"\n<<<<<<<<<<<<<<<<<<<<<<<<<<<< COMPLETE >>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    DbgPrintW(L"[*] Total Execution Time: %d.%03d seconds\n", iWhole, iFrac);
}

VOID DeinitLog(void)
{
    WCHAR ch[2] = { 0 };
    DWORD cchRead = 0;

    QueryPerformanceCounter(&g_PerfTimer.End);

    PrintElapsedTime();

    DbgPrintW(L"[#] Press Enter to continue...\n");
    ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), ch, 2, &cchRead, NULL);

    CloseDebugConsole();
}