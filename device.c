/// @file device.c
/// @brief Contiene l'implementazione dei DEVICE.

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

pid_t mypid;
int fifoFD;

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

void checkVicini(double dist, pid_t **board, pid_t vicini[], int x, int y) {

    //Controllo tutte le celle della board, se trovo un device vicino lo aggiungo alla lista dei vicini
    int c = 0;
    for (int i = 0; i < BOARD_SIDE_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIDE_SIZE; ++j) {
            //Trovo device
            if (board[i][j] != 0) {
                //Controllo se device è vicino
                if (sqrt(((x - i) * (x - i)) + ((y - j) * (y - j))) <= (double) dist) {
                    vicini[c] = board[i][j];
                    c++;
                }
            }
        }
    }

}

void stopDevice(int signal) {
    if (close(fifoFD))
        errExit("<Device> close FIFO failed");
    // TODO eliminare la fifo non solo chiuderla
    // TODO dis-attach memoria condivisa
}

_Noreturn int device(int nProcesso, char path[]) {
    // TODO MANCANO LE STAMPE!!! penso da fare alla fine di tutto
    mypid = getpid();
    //Creo una la FIFO del device
    char fifoPath[PATH_MAX];
    createFIFO(mypid, fifoPath);
    // apro la FIFO
    fifoFD = open(fifoPath, O_RDONLY | O_NONBLOCK);
    if (fifoFD == -1)
        errExit("<Device> open FIFO failed");
    // attach memoria condivisa
    pid_t **board = attachSegment(boardId);
    Acknowledgment *ackList = attachSegment(ackListId);

    // inizializzo il mio storage dei messaggi
    Message messaggi[MESS_DEV_MAX];
    for (int k = 0; k < MESS_DEV_MAX; ++k) {
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

        //invio
        if (vuoto == false) {
            //Trovo i vicini
            pid_t vicini[NUM_DEVICES - 1] = {(pid_t) -1};
            for (int i = 0; i < MESS_DEV_MAX; ++i) {
                if (messaggi[i].message_id != -1) {
                    double dist = messaggi[i].max_distance;
                    checkVicini(dist, board, vicini, x, y);

                    bool giaRicevutoTutti = true;

                    //una volta trovati i vicini mando ad ognuno il messaggio in questione, se non l'hanno già ricevuto
                    for (int j = 0; j < NUM_DEVICES - 1; ++j) {
                        giaRicevutoTutti = true;
                        if (vicini[i] != -1) {

                            //Controllo se vicino ha già ricevuto il messaggio nella lista delle ack
                            bool giaRicevuto = false;
                            for (int k = 0; k < ACK_MAX; ++k) {
                                if (ackList[k].message_id == messaggi[k].message_id &&
                                    ackList[k].pid_sender == messaggi[k].pid_receiver) {
                                    giaRicevuto = true;
                                    break;
                                } else {
                                    giaRicevutoTutti = false;
                                }
                            }

                            //Se non l'ha gia ricevuto, glielo posso mandare
                            if (giaRicevuto == false) {
                                //Apro la fifo del device vicino
                                char deviceVicinoFIFO[PATH_MAX];
                                sprintf(deviceVicinoFIFO, "/tmp/dev_fifo.%d", vicini[i]);
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

                    //Hanno gia ricevuto tutti il messaggio, lo cancello
                    if (giaRicevutoTutti) {
                        messaggi[i].message_id = -1;
                    }

                    //Svuoto la lista dei vicini per passare al prossimo messaggio
                    for (int k = 0; k < NUM_DEVICES - 1; ++k) {
                        vicini[k] = (pid_t) -1;
                    }
                }
            }
        }

        //RICEZIONE MESSAGGI
        //Leggo eventuali messaggi
        Message messaggio;
        Acknowledgment ack;
        int nm;
        while (read(fifoFD, &messaggio, sizeof(Message)) != -1) {
            //Letto un messaggio, creo un ack
            // TODO penso che il messaggio bisogna metterlo nel primo posto dove c'è un -1 e non a caso, altrimenti si potrebbero sovrascrivere altri messaggi
            messaggi[nm] = messaggio;
            ack.message_id = messaggio.message_id;
            ack.pid_sender = messaggio.pid_sender;
            ack.pid_receiver = mypid;
            ack.timestamp = time(NULL);

            //Aggiungo l'ack al segmento di memoria apposito
            for (int i = 0; i < ACK_MAX; ++i) {
                if (ackList[i].message_id == -1) {
                    // Scrivo messaggio in acklist ed esco dal ciclo
                    ackList[i] = ack;
                    break;
                }
            }

            nm++;
        }


        //MOVIMENTO

        //Se casella è libera mi inserisco
        if (board[x][y] == 0) {
            board[x][y] = mypid;
        }

        //Libero il semaforo del prossimo processo se non sono l'ultimo
        if (nProcesso != NUM_DEVICES - 1) {
            semOp(semidBoard, nProcesso + 1, 1);
        }
    }

}