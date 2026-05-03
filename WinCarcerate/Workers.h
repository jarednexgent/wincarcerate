// Workers.h
#ifndef WORKERS_H
#define WORKERS_H

#include <windows.h>

DWORD WINAPI DirectoryWorkerThread(OPTIONAL LPVOID Context);
DWORD WINAPI FileConsumerThread(OPTIONAL LPVOID Context);

#endif 