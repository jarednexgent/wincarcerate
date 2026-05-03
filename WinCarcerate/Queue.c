// Queue.c
#include <windows.h>
#include "Queue.h"

extern volatile BOOL g_Finished;

VOID CreateQueue(QUEUE* Queue)
{
    Queue->Head = NULL;
    Queue->Tail = NULL;

    InitializeSRWLock(&Queue->Lock);
    InitializeConditionVariable(&Queue->NonEmpty);
}

VOID Enqueue(QUEUE *Queue, QUEUE_ENTRY *Entry)
{
    AcquireSRWLockExclusive(&Queue->Lock);
    Entry->Next = NULL;

    if (Queue->Tail)
    {
        Queue->Tail->Next = Entry;
    }
    else
    {
        Queue->Head = Entry;
    }

    Queue->Tail = Entry;

    WakeConditionVariable(&Queue->NonEmpty);
    ReleaseSRWLockExclusive(&Queue->Lock);
}

PQUEUE_ENTRY WaitAndDequeue(QUEUE *Queue)
{
    QUEUE_ENTRY *Entry = NULL;
    AcquireSRWLockExclusive(&Queue->Lock);

 
    while (QUEUE_IS_EMPTY(Queue) && !g_Finished)
    {
        SleepConditionVariableSRW(&Queue->NonEmpty, &Queue->Lock, INFINITE, 0);
    }

    if (g_Finished && QUEUE_IS_EMPTY(Queue))
    {
        ReleaseSRWLockExclusive(&Queue->Lock);
        return NULL;
    }

    QUEUE_DETACH_HEAD(Entry, Queue);

    ReleaseSRWLockExclusive(&Queue->Lock);
    return Entry;
}

