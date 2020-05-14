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
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>


int main(int argc, char *argv[]) {

    // TODO: bloccare i segnali non necessari
    // TODO: gestire la terminazione con SIGTERM

    if (argc != 2) {
        printf("Usage: %s msg_queue_key file_posizioni\n", argv[0]);
        return 0;
    }

    errno = 0;
    key_t msgQueueKey = strtoul(argv[1], NULL, 10);
    if (errno == ERANGE)
        errExit("<Server> failed at converting msg_queue key");

    // Creo due set di semafori
    // 1 - semafori per ack list
    unsigned short semAckValues[] = {1};
    int semidAck = semCreate(1, semAckValues);
    // 2 - semafori per griglia posizioni
    unsigned short semGridValues[] = {0, 0, 0, 0};
    int semidGrid = semCreate(4, semGridValues);

    // Creo i due segmenti di memoria condivisa
    // 1 - griglia 10 x 10 per movimento dei device (board)
    int boardId = createMemSegment(sizeof(pid_t[10][10]));
    // 2 - array di acknowledgment per ack manager
    int ackListId = createMemSegment(sizeof(Acknowledgment[100]));

    // Fork per ackmanager
    pid_t pidAckManager = fork();
    switch (pidAckManager) {
        case 0:
            // AckManager
            exit(0);
        case -1:
            errExit("<Server> fork for ack manager failed");
        default:; // Continuo al di fuori dello switch
    }

    // Fork per i device
    for (int i = 0; i < NUM_DEVICES; i++) {
        pid_t pidDevice = fork();
        switch (pidDevice) {
            case 0:
                // Device i-esimo
                exit(0);
            case -1:
                errExit("<Server> fork for device failed");
            default:; // Continuo la mia vita fuori dallo switch
        }
    }

    // Ciclo infinito in cui faccio muovere i device
    // queste merde pragma sono da togliere ma per adesso servono a non evidenziare tutto il ciclo in giallo mentre lo scrivo
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    do {
        sleep(2);

    } while (true);
#pragma clang diagnostic pop

    return 0;
}