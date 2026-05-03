// Crypto.h
#ifndef CRYPTOR_H
#define CRYPTOR_H

#include <windows.h>

VOID GenRandomBytes(IN OUT PBYTE buffer, IN DWORD length);
BOOL EncryptSingleFileW(IN CONST PWSTR wOriginalPath);
BOOL DecryptSingleFileW(IN CONST PWSTR wEncryptedPath);

#endif 