// Queue.h
#ifndef QUEUE_H
#define QUEUE_H

#include <windows.h>

typedef struct _QUEUE_ENTRY {
    struct _QUEUE_ENTRY *Next;
    WCHAR *Path;
} QUEUE_ENTRY, * PQUEUE_ENTRY;

typedef struct _QUEUE {
    QUEUE_ENTRY *Head; 
    QUEUE_ENTRY *Tail;
    SRWLOCK Lock;
    CONDITION_VARIABLE NonEmpty;
} QUEUE, * PQUEUE;


VOID CreateQueue(QUEUE *Queue);
VOID Enqueue(QUEUE *Queue, QUEUE_ENTRY *Entry);
PQUEUE_ENTRY WaitAndDequeue(QUEUE *Queue);

#define QUEUE_IS_EMPTY(Q)   ((Q)->Head == NULL)

#define QUEUE_DETACH_HEAD(N, Q)                      \
    do {                                             \
        if ((N = (Q)->Head) != NULL) {               \
            if (!((Q)->Head = (N)->Next)) {          \
                (Q)->Tail = NULL;                    \
            }                                        \
        }                                            \
    } while(0)


#endif

