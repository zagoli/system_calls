/// @file server.c
/// @brief Contiene l'implementazione del SERVER.

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"

#include <stdio.h>
#include <sys/param.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/stat.h>


int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: %s msg_queue_key file_posizioni\n", argv[0]);
        return 0;
    }

    key_t msgQueueKey = atoi(argv[1]);

    // Creiamo due set di semafori
    // 1 - semafori per ack list
    unsigned short semAckValues = {1};
    int semidAck = semCreate(1, semAckValues);
    // 2 - semafori per griglia posizioni
    unsigned short semGridValues = {1, 0, 0, 0};
    int semidGrid = semCreate(4, semGridValues);

    return 0;
}