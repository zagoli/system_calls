/// @file semaphore.c
/// @brief Contiene l'implementazione delle funzioni specifiche per la gestione dei SEMAFORI.

#include "err_exit.h"
#include "semaphore.h"

#include <sys/sem.h>
#include <sys/stat.h>

void semOp(int semid, unsigned short sem_num, short sem_op) {
    struct sembuf sop = {sem_num, sem_op, 0};

    if (semop(semid, &sop, 1) == -1)
        errExit("<Semaphore> semop failed");
}

int semCreate(int nSems, unsigned short semValuesInit[]) {
    // Creo un set di semafori con nSems semafori
    int semid = semget(IPC_PRIVATE, nSems, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (semid == -1)
        errExit("<Semaphore> semget failed");
    // Inizializzo il set di semafori con semctl
    union semun arg;
    arg.array = semValuesInit;
    if (semctl(semid, 0, SETALL, arg) == -1)
        errExit("<Semaphore> semctl SETALL failed");
    return semid;
}