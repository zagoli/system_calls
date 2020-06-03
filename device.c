/// @file device.c
/// @brief Contiene l'implementazione dei DEVICE.

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"
#include "server.c"

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/shm.h>

pid_t mypid;
int fifoFD;
char fifoPath[PATH_MAX];
pid_t *board;
Acknowledgment *ackList;

void nextPositions(int posizioniFD, int *x, int *y) {
    char buf[3] = {0};
    int bR = read(posizioniFD, buf, 3);
    // se sono dopo EOF mantengo posizione corrente
    if (bR == 0)
        return;
    if (bR == -1)
        errExit("<Device> read positions failed");
    *x = (int) strtol(&buf[0], NULL, 10);
    if (errno == ERANGE || errno == EINVAL)
        errExit("<Device> failed at converting x");
    *y = (int) strtol(&buf[2], NULL, 10);
    if (errno == ERANGE || errno == EINVAL)
        errExit("<Device> failed at converting y");
    // mi sposto al prossimo passaggio: 16 caratteri più \n
    if (lseek(posizioniFD, 17, SEEK_CUR) == -1)
        errExit("<Device> lseek failed");
}

void checkVicini(double dist, pid_t *board, pid_t vicini[], int x, int y) {
    //Controllo tutte le celle della board, se trovo un device vicino lo aggiungo alla lista dei vicini
    int c = 0;
    for (int i = 0; i < BOARD_SIDE_SIZE; i++) {
        for (int j = 0; j < BOARD_SIDE_SIZE; j++) {
            //Trovo device tranne me stesso
            if (board[j + i * BOARD_SIDE_SIZE] != 0) {
                //Controllo se device è vicino
                double distanza = (double) sqrt(((x - i) * (x - i)) + ((y - j) * (y - j)));
                if (distanza <= (double) dist && distanza != 0) {
                    vicini[c] = board[j + i * BOARD_SIDE_SIZE];
                    c++;
                }
            }
        }
    }

}

void stopDevice(int signal) {
    //Chiudo la fifo, la rimuovo e faccio il detach dei segmenti di memoria di ackList e board
    if (close(fifoFD) == -1)
        errExit("<Device> close FIFO failed");
    if (unlink(fifoPath) == -1) {
        errExit("<Device> unlink FIFO failed");
    }
    if (shmdt(ackList) == -1) {
        errExit("<Device> detach ackList failed");
    }
    if (shmdt(board) == -1) {
        errExit("<Device> detach board failed");
    }
    exit(0);
}

_Noreturn int device(int nProcesso, char path[]) {
    mypid = getpid();
    //Creo una la FIFO del device
    createFIFO(mypid, fifoPath);
    // apro la FIFO
    fifoFD = open(fifoPath, O_RDONLY | O_NONBLOCK);
    if (fifoFD == -1)
        errExit("<Device> open FIFO failed");
    // attach memoria condivisa
    board = attachSegment(boardId);
    ackList = attachSegment(ackListId);

    // inizializzo il mio storage dei messaggi
    Message messaggi[MESS_DEV_MAX];
    for (int k = 0; k < MESS_DEV_MAX; k++) {
        messaggi[k].message_id = -1;
    }

    //Blocco segnali che non siano SIGTERM
    int sig[] = {SIGTERM};
    blockAllSignalsExcept(sig, 1);

    // Gestione SIGTERM
    if (signal(SIGTERM, stopDevice) == SIG_ERR)
        errExit("<Device> setting SIGTERM handler failed");

    //Apro il file delle posizioni
    int posizioniFD = open(path, O_RDONLY);
    if (posizioniFD == -1)
        errExit("open failed");
    // Mi sposto alla posizione corretta
    if (lseek(posizioniFD, 4 * nProcesso, SEEK_SET) == -1)
        errExit("<Device> lseek failed");


    //Tutte le azioni del device
    while (true) {
        // Aspetto che il semaforo sia libero
        semOp(semidBoard, nProcesso, -1);

        //Leggo la prossima posizione
        int x, y;
        nextPositions(posizioniFD, &x, &y);

        //INVIO MESSAGGI
        //Controllo se ho messaggi
        bool vuoto = true;
        for (int i = 0; i < MESS_DEV_MAX; ++i) {
            if (messaggi[i].message_id != -1) {
                vuoto = false;
                break;
            }
        }


        bool giaRicevuto;
        int countRicevuti=0;
        //invio
        if (vuoto == false) {
            //Trovo i vicini CON INIZIALIZZATORE ARRAY STRANO CHE FUNZIONA SOLO SU GCC
            pid_t vicini[NUM_DEVICES - 1] = {[0 ... NUM_DEVICES - 2](pid_t) -1};
            for (int i = 0; i < MESS_DEV_MAX; ++i) {

                if (messaggi[i].message_id != -1) {

                    //Controllo se per caso non hanno già ricevuto tutti il messaggio(countRicevuti=4), in caso lo cancello e passo al prossimo
                    countRicevuti=0;
                    semOp(semidAckList, 0, -1);
                    for (int k = 0; k < NUM_DEVICES; ++k) {//Scorro la lista dei devices
                        for (int l = 0; l < ACK_MAX; ++l) {//Scorro la lista degli ack per ogni device (escluso me stesso)
                            if (ackList[l].message_id == messaggi[i].message_id &&
                                ackList[l].pid_receiver == pidDevices[k]&&
                                pidDevices[k]!=mypid) {
                                countRicevuti++;
                                break;
                            }
                        }
                    }
                    //In queso caso cancello il messaggio e passo al prossimo messaggio usando continue
                    if (countRicevuti==4){
                        messaggi[i].message_id=-1;
                        semOp(semidAckList, 0, 1);
                        continue;
                    }
                    //Altrimenti avanzo e passo all'invio
                    semOp(semidAckList, 0, 1);

                    double dist = messaggi[i].max_distance;
                    checkVicini(dist, board, vicini, x, y);


                    //una volta trovati i vicini mando ad ognuno il messaggio in questione, se non l'hanno già ricevuto
                    for (int j = 0; j < NUM_DEVICES - 1; j++) {
                        if (vicini[j] != -1) {

                            //Controllo se vicino ha già ricevuto il messaggio nella lista delle ack e aspetto il semaforo
                            giaRicevuto = false;
                            semOp(semidAckList, 0, -1);
                            for (int k = 0; k < ACK_MAX; k++) {
                                if (ackList[k].message_id == messaggi[i].message_id &&
                                    vicini[j] == ackList[k].pid_receiver) {
                                    giaRicevuto = true;
                                    break;
                                }
                            }
                            semOp(semidAckList, 0, 1);

                            //Se non l'ha gia ricevuto, glielo posso mandare
                            if (giaRicevuto == false) {
                                //Apro la fifo del device vicino
                                char deviceVicinoFIFO[PATH_MAX];
                                sprintf(deviceVicinoFIFO, "/tmp/dev_fifo.%d", vicini[j]);
                                int vicinoFD = open(deviceVicinoFIFO, O_WRONLY | O_NONBLOCK);
                                if (vicinoFD == -1)
                                    errExit("<Device> open Fifo vicino failed");
                                //Scrivo il messaggio
                                write(vicinoFD, &messaggi[i], sizeof(Message));
                                if (close(vicinoFD) == -1)
                                    errExit("<Device> close Fifo vicino failed");
                            }
                        }
                    }


                    //Svuoto la lista dei vicini per passare al prossimo messaggio
                    for (int k = 0; k < NUM_DEVICES - 1; k++) {
                        vicini[k] = (pid_t) -1;
                    }
                }
            }
        }

        //RICEZIONE MESSAGGI
        //Leggo eventuali messaggi
        Message messaggio;
        Acknowledgment ack;
        while (read(fifoFD, &messaggio, sizeof(Message)) != 0) {
            //Letto un messaggio, creo un ack

            for (int j = 0; j < MESS_DEV_MAX; j++) {
                if (messaggi[j].message_id == -1) {
                    messaggi[j] = messaggio;
                    break;
                }
            }

            ack.message_id = messaggio.message_id;
            ack.pid_sender = messaggio.pid_sender;
            ack.pid_receiver = mypid;
            ack.timestamp = time(NULL);

            //Aggiungo l'ack al segmento di memoria apposito aspettando il semaforo
            semOp(semidAckList, 0, -1);
            for (int i = 0; i < ACK_MAX; i++) {
                if (ackList[i].message_id == -1) {
                    // Scrivo messaggio in acklist ed esco dal ciclo
                    ackList[i] = ack;
                    break;
                }
            }
            semOp(semidAckList, 0, 1);

        }


        //MOVIMENTO

        //Se casella è libera mi inserisco
        if (board[y + x * BOARD_SIDE_SIZE] == 0) {
            board[y + x * BOARD_SIDE_SIZE] = mypid;
        }


        //pidD1 i_D1 j_D1 msgs: lista message_id
        printf("%d %d %d msgs: ", mypid, x, y);
        for (int l = 0; l < MESS_DEV_MAX; ++l) {
            if (messaggi[l].message_id != -1) {
                printf("%d ", messaggi[l].message_id);
            }
        }
        printf("\n");


        //Libero il semaforo del prossimo processo o del server
        semOp(semidBoard, nProcesso + 1, 1);
    }

}