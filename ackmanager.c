/// @file ackmanager.c
/// @brief Contiene il codice per ack manager

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"

#include <sys/msg.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>

//Globale per eliminare tutto dopo
int msqid;
Acknowledgment *ackList;

void quit(int sig);

_Noreturn void ackmanager(int msgQueueKey) {

    // Blocco tutti i segnali eccetto SIGTERM
    int sig[] = {SIGTERM};
    blockAllSignalsExcept(sig, 1);

    //Gestisco SIGTERM
    if (signal(SIGTERM, quit) == SIG_ERR)
        errExit("<Ackmanager> setting SIGTERM handler failed");

    // Creo la coda di messaggi
    msqid = msgget(msgQueueKey, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (msqid == -1)
        errExit("<Ackmanager> create message queue failed");

    // Attacco la memoria per la lista e la inizializzo
    ackList = attachSegment(ackListId);
    semOp(semidAckList, 0, -1);
    for (int i = 0; i < ACK_MAX; i++) { ackList[i].message_id = -1; }
    semOp(semidAckList, 0, 1);

    // Ogni 5 secondi controllo la lista di messaggi
    while (true) {
        sleep(5);
        // Aspetto che il semaforo sia libero
        semOp(semidAckList, 0, -1);

        // Cerco 5 ack con lo stesso message id
        for (int i = 0; i < ACK_MAX - (NUM_DEVICES - 1); i++) {
            // Se message id = -1 non mi interessa
            if (ackList[i].message_id != -1) {
                int positions[NUM_DEVICES] = {0}, found = 0;
                for (int j = i; j < ACK_MAX && found < NUM_DEVICES; j++) {
                    if (ackList[j].message_id == ackList[i].message_id) {
                        positions[found++] = j;
                    }
                }
                // Se ho trovato 5 ack con lo stesso message id
                if (found == NUM_DEVICES) {
                    // Creo il messaggio contenente gli ack da mandare al client
                    AckReportClient ackReportClient;
                    ackReportClient.mtype = ackList[i].message_id;
                    // Mi salvo gli ack da inviare al client
                    for (int j = 0; j < NUM_DEVICES; j++) {
                        ackReportClient.acks[j] = ackList[positions[j]];
                        // Marco allo stesso tempo l'ack come riscrivibile
                        ackList[positions[j]].message_id = -1;
                    }
                    // Invio il messaggio sulla msg queue
                    if (msgsnd(msqid, &ackReportClient, sizeof(ackReportClient) - sizeof(ackReportClient.mtype), 0) ==
                        -1)
                        errExit("<Ackmanager> send ackreport on message queue failed");
                }
            }
        }

        // Ora che ho finito libero il semaforo
        semOp(semidAckList, 0, 1);
    }
}

void quit(int sig){
    if (shmdt(ackList) == -1)
        errExit("<Ackmanager> detach acklist failed");
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        errExit("<Ackmanager> failed to remove message queue");
    }

    printf("[Ackmanager] I'm dead\n");
    exit(0);
}
