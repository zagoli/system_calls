/// @file shared_memory.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione della MEMORIA CONDIVISA.

#include "err_exit.h"
#include "shared_memory.h"

#include <sys/shm.h>
#include <sys/stat.h>
#include <stddef.h>

int createMemSegment(size_t size) {
    int shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (shmid == -1)
        errExit("<Shared Memory> shmget failed");
    return shmid;
}

void *attachSegment(int shmid) {
    void *startPointer = shmat(shmid, NULL, 0);
    if (startPointer == (void *) -1)
        errExit("<Shared memory> shmat failed");
    return startPointer;
}