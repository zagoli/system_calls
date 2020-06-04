/// @file server.c
/// @brief Contiene l'implementazione del SERVER.

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "ackmanager.h"
#include "device.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <unistd.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/wait.h>

// Questa variabile è globale in modo che possa essere vista all'interno del signal handler. Le altre sono in define.h perchè servono anche in altri file
pid_t pidAckManager;
pid_t pidDevices[NUM_DEVICES];

// Questa funzione manda una sigterm ad AckManager e ai Device, poi rimuove i semafori e i segmenti di memoria condivisa
void stopServer(int sig);

int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Usage: %s msg_queue_key file_posizioni\nCreato da Jacopo Zagoli e Gianluca Antolini\n", argv[0]);
        return 0;
    }

    errno = 0;
    key_t msgQueueKey = strtoul(argv[1], NULL, 10);
    if (errno == ERANGE || errno == EINVAL)
        errExit("<Server> failed at converting msg_queue key");

    // Blocco i segnali non necessari
    int sig[] = {SIGTERM};
    blockAllSignalsExcept(sig, 1);

    // Creo due set di semafori
    // 1 - semafori per ack list
    unsigned short semAckValues[] = {1};
    semidAckList = semCreate(1, semAckValues);
    // 2 - semafori per device
    unsigned short semBoardValues[NUM_DEVICES] = {0};
    semidBoard = semCreate(NUM_DEVICES, semBoardValues);

    // Creo i due segmenti di memoria condivisa
    // 1 - griglia 10 x 10 per movimento dei device (board)
    boardId = createMemSegment(sizeof(pid_t) * BOARD_SIDE_SIZE * BOARD_SIDE_SIZE);
    // Inizializzo a zero la board
    pid_t *board = (pid_t *) attachSegment(boardId);
    for (int j = 0; j < BOARD_SIDE_SIZE; j++) {
        for (int i = 0; i < BOARD_SIDE_SIZE; i++) {
            board[i + j * BOARD_SIDE_SIZE] = 0;
        }
    }
    if (shmdt(board) == -1)
        errExit("<Server> failed to detach board");
    // 2 - array di acknowledgment per ack manager
    ackListId = createMemSegment(sizeof(Acknowledgment[ACK_MAX]));

    // Fork per ackmanager
    pidAckManager = fork();
    switch (pidAckManager) {
        case 0:
            // AckManager
            ackmanager(msgQueueKey);
        case -1:
            errExit("<Server> fork for ack manager failed");
        default:; // Continuo fuori dallo switch
    }

    // Fork per i device
    for (int i = 0; i < NUM_DEVICES; i++) {
        pidDevices[i] = fork();
        switch (pidDevices[i]) {
            case 0:
                // Device i-esimo
                device(i, argv[2]);
            case -1:
                errExit("<Server> fork for device failed");
            default:; // Continuo la mia vita fuori dallo switch
        }
    }

    // Gestisco la ricezione di SIGTERM
    if (signal(SIGTERM, stopServer) == SIG_ERR)
        errExit("<Server> setting SIGTERM handler failed");

    // Ciclo infinito in cui faccio muovere i device
    int iteration = 1;
    do {
        // ------------------ Stampa info device --------------------------------------------
        printf("\n# Step %d: device positions ########################\n", iteration);
        // Sblocco il primo semaforo dei device, dovrebbero sbloccarsi gli altri in cascata
        semOp(semidBoard, 0, 1);
        // Aspetto che ogni device stampi le sue informazioni, la riga di chiusura la stampa l'ultimo device
        // ----------------------------------------------------------------------------------
        // Dormo due secondi
        sleep(2);
    } while (++iteration);

    return 0;  // Non ci dovrebbe arrivare, ma lo lascio per simpatia
}

void stopServer(int sig) {
    // Invio un SIGTERM a AckManager
	printf("\nServer dedding ackmanager\n");
    if (kill(pidAckManager, SIGTERM) == -1)
        errExit("<Server> kill AckManager failed");
    // Invio SIGTERM a tutti i device
    for (int i = 0; i < NUM_DEVICES; i++) {
		printf("Server dedding device with pid %d\n", pidDevices[i]);
        if (kill(pidDevices[i], SIGTERM) == -1)
            errExit("<Server> kill device failed");
    }

    // Aspetto che i miei figli terminino
    while (wait(NULL) != -1);
    if (errno != ECHILD){
        errExit("<Server> wait for children termination failed");}
	printf("Server successfully killed all his childrens\n");

	printf("Server removing garbage\n");
    // Rimuovo il segmento di memoria della board
    if (shmctl(boardId, IPC_RMID, NULL) == -1)
        errExit("<Server> remove board failed");
    // Rimuovo il segmento di memoria di ackList
    if (shmctl(ackListId, IPC_RMID, NULL) == -1)
        errExit("<Server> remove ackList failed");
    // Rimuovo il set di semafori della board
    if (semctl(semidBoard, 0, IPC_RMID) == -1)
        errExit("<Server> remove semaphore set board failed");
    // Rimuovo il set di semafori di ackList
    if (semctl(semidAckList, 0, IPC_RMID) == -1)
        errExit("<Server> remove semaphore set ackList failed");

    // Infine, termino
	printf("Server ded\n");
    exit(0);
}