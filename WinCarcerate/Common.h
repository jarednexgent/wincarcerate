#ifndef COMMON_H
#define COMMON_H


#include <windows.h>
#include <winternl.h>

// --- Native NT Definitions ---
#ifndef NT_SUCCESS
#define NT_SUCCESS(STATUS)             (((NTSTATUS)(STATUS)) >= 0x00)
#endif

#define STATUS_SUCCESS                 0x00000000
#define STATUS_INFO_LENGTH_MISMATCH    0xC0000004

// --- Native Accessor Macros (Pseudo Handles) ---
#define NtCurrentProcess()             ((HANDLE)(LONG_PTR)-1)
#define NtCurrentThread()              ((HANDLE)(LONG_PTR)-2)

#ifdef _WIN64
#define FetchPPEB()                    ((PPEB)(__readgsqword(0x60)))
#else
#define FetchPPEB()                    ((PPEB)(__readfsdword(0x30)))
#endif

// --- Keywords & Alignment ---
#define INLINE                         __inline
#define FORCE_INLINE                   __forceinline
#define THREAD                         static __declspec(thread)

// --- Application Config ---
#define ENC_FILE_EXTENSION             L".WCRT$"

// --- Safety & Cleanup Macros ---
#define DELETE_HANDLE(H)                                  \
    do {                                                  \
        if ((H) != NULL && (H) != INVALID_HANDLE_VALUE) { \
            CloseHandle(H);                               \
            (H) = INVALID_HANDLE_VALUE;                   \
        }                                                 \
    } while (0)

#define FREE_HEAP(H, PTR)                        \
    do {                                         \
        if ((PTR) != NULL) {                     \
            HeapFree((H), 0, (PTR));             \
            (PTR) = NULL;                        \
        }                                        \
    } while (0)


// --- Dynamic Function Pointers (NTAPI) ---
typedef NTSTATUS(NTAPI* fnNtQuerySystemInformation)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );

typedef NTSTATUS(NTAPI* fnNtQueryInformationProcess)(
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
    );


// --- Function Declarations ---

void* __cdecl memset(void* dest, int c, size_t count);
void* __cdecl memcpy(void* dest, const void* src, size_t count);

#endif
