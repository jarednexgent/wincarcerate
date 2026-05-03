/* Common.c */
#include "Common.h"

#pragma function(memset, memcpy)

void* __cdecl memset(void* dest, int c, size_t count)
{
    unsigned char* d = (unsigned char*)dest;

    while (count--)
        *d++ = (unsigned char)c;

    return dest;
}

void* __cdecl memcpy(void* dest, const void* src, size_t count)
{
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    while (count--)
        *d++ = *s++;

    return dest;
}

/* tls_nocrt.c
   Minimal implicit TLS support for x64 MSVC no-CRT builds.
   Enables file-scope/static __declspec(thread) variables.
*/
//
//#include <windows.h>
//#include <winnt.h>
//
//#ifndef _M_X64
//#error This file is written for x64 only.
//#endif
//
//#pragma section(".tls",     long, read, write)
//#pragma section(".tls$ZZZ", long, read, write)
//#pragma section(".CRT$XLA", long, read)
//#pragma section(".CRT$XLZ", long, read)
//#pragma section(".rdata$T", long, read)
//
//__declspec(allocate(".tls"))
//char _tls_start = 0;
//
//__declspec(allocate(".tls$ZZZ"))
//char _tls_end = 0;
//
///* Required storage for the loader-written module TLS index. */
//ULONG _tls_index = 0;
//
///* Empty TLS callback array sentinels. */
//__declspec(allocate(".CRT$XLA"))
//PIMAGE_TLS_CALLBACK __xl_a = 0;
//
//__declspec(allocate(".CRT$XLZ"))
//PIMAGE_TLS_CALLBACK __xl_z = 0;
//
///* PE TLS directory for x64. */
//__declspec(allocate(".rdata$T"))
//const IMAGE_TLS_DIRECTORY64 _tls_used =
//{
//    (ULONGLONG)(ULONG_PTR)&_tls_start,
//    (ULONGLONG)(ULONG_PTR)&_tls_end,
//    (ULONGLONG)(ULONG_PTR)&_tls_index,
//    (ULONGLONG)(ULONG_PTR)(&__xl_a + 1),
//    0,
//    0
//};
//
///* Why: force the linker to keep the TLS directory. */
//#pragma comment(linker, "/INCLUDE:_tls_used")
//
//#pragma section(".CRT$XLA", read)
//#pragma section(".CRT$XLZ", read)
//
//EXTERN_C
//__declspec(allocate(".CRT$XLA")) 
//PIMAGE_TLS_CALLBACK __xl_a = 0;
//
//EXTERN_C
//__declspec(allocate(".CRT$XLZ"))
//PIMAGE_TLS_CALLBACK __xl_z = 0;
//
//EXTERN_C
//CONST IMAGE_TLS_DIRECTORY64 _tls_used =
//{
//    (ULONGLONG)&_tls_start,
//    (ULONGLONG)&_tls_end,
//    (ULONGLONG)&_tls_index,
//    (ULONGLONG)&__xl_a,
//    0,
//    0
//};