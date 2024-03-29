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
#include <sys/shm.h>

// Variabili globali che devono essere lette all'interno del signal handler
pid_t mypid;
int fifoFD;
char fifoPath[PATH_MAX];
pid_t *board;
Acknowledgment *ackList;

// Legge dal file di input le prossime posizioni del device
void nextPositions(int posizioniFD, int *x, int *y);

// Data la mia posizione e la distanza del messaggio, trova i miei vicini e li mette nell'array vicini
void checkVicini(double dist, pid_t vicini[], int x, int y);

// Funzione per gestire SIGTERM
void stopDevice(int signal);

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
    Message messaggi[MESS_DEV_MAX] = {[0 ... MESS_DEV_MAX - 1] = {.message_id = -1}};

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

    // Posizione del device nella board
    int x = -1, y = -1, oldX = -1, oldY = -1;

    // Tutte le azioni del device
    while (true) {

        // Aspetto che il semaforo sia libero
        semOp(semidBoard, nProcesso, -1);

        //Leggo la prima posizione solo se sono al primo giro
        if (x == -1 && y == -1)
            nextPositions(posizioniFD, &x, &y);

        //-------------------------------INVIO MESSAGGI-------------------------------
        //Controllo se ho messaggi
        bool vuoto = true;
        for (int i = 0; i < MESS_DEV_MAX; i++) {
            if (messaggi[i].message_id != -1) {
                vuoto = false;
                break;
            }
        }

        // Se ho almeno un messaggio da inviare
        if (!vuoto) {
            // Per ogni messaggio
            for (int i = 0; i < MESS_DEV_MAX; i++) {
                // Se è valido, perchè se message id = -1 allora non è un messaggio
                if (messaggi[i].message_id != -1) {

                    //Trovo i vicini (CON INIZIALIZZATORE ARRAY STRANO CHE FUNZIONA SOLO SU GCC)
                    pid_t vicini[NUM_DEVICES - 1] = {[0 ... NUM_DEVICES - 2] = (pid_t) -1};
                    double dist = messaggi[i].max_distance;
                    checkVicini(dist, vicini, x, y);


                    //una volta trovati i vicini mando ad ognuno il messaggio in questione, se non l'hanno già ricevuto
                    for (int j = 0; j < NUM_DEVICES - 1; j++) {
                        // Se vicini in posizione j == -1, sono arrivato al punto dove non ho più vicini
                        if (vicini[j] == -1)
                            break;

                        //Controllo se vicino ha già ricevuto il messaggio nella lista delle ack e aspetto il semaforo
                        bool giaRicevuto = false;
                        semOp(semidAckList, 0, -1);
                        // scorro lista ack
                        for (int k = 0; k < ACK_MAX; k++) {
                            if (ackList[k].message_id == messaggi[i].message_id &&
                                vicini[j] == ackList[k].pid_receiver) {
                                giaRicevuto = true;
                                break;
                            }
                        }
                        semOp(semidAckList, 0, 1);

                        //Se non l'ha gia ricevuto, glielo posso mandare
                        if (!giaRicevuto) {
                            //Apro la fifo del device vicino
                            char deviceVicinoFIFO[PATH_MAX];
                            sprintf(deviceVicinoFIFO, "/tmp/dev_fifo.%d", vicini[j]);
                            int vicinoFD = open(deviceVicinoFIFO, O_WRONLY | O_NONBLOCK);
                            if (vicinoFD == -1)
                                errExit("<Device> open Fifo vicino failed");
                            // Il sender è impostato alla ricezione, manca il receiver. Non lo imposto perchè
                            // il device non se ne fa nulla del receiver: alla ricezione manda un ack dicendo che l'ha ricevuto lui
                            //Scrivo il messaggio
                            write(vicinoFD, &messaggi[i], sizeof(Message));
                            if (close(vicinoFD) == -1)
                                errExit("<Device> close Fifo vicino failed");
                            //dopo averlo inviato, elimino il messaggio
                            messaggi[i].message_id = -1;
                        }
                    }

                }
            }
        }
        //--------------------------------------------------------------------------

        //-------------------------------RICEZIONE MESSAGGI-------------------------
        //Leggo eventuali messaggi
        Message messaggio;
        Acknowledgment ack;
        while (read(fifoFD, &messaggio, sizeof(Message)) != 0) {
            //Letto un messaggio, creo un ack
            ack.message_id = messaggio.message_id;
            ack.pid_sender = messaggio.pid_sender;
            ack.pid_receiver = mypid;
            ack.timestamp = time(NULL);

            //Aggiungo l'ack al segmento di memoria apposito aspettando il semaforo, mentre
            // Controllo se questo messaggio ha già 4 ack (me escluso). Se sì, sono stato l'ultimo a riceverlo
            semOp(semidAckList, 0, -1);
            int ackSpediti = 0;
            bool set = false;
            for (int i = 0; i < ACK_MAX; i++) {
                if (ackList[i].message_id == -1 &&
                    !set) { // set serve per evitare di scrivere il messaggio in ogni buco dove c'è -1
                    // Scrivo messaggio in acklist
                    ackList[i] = ack;
                    set = true;
                } else if (ackList[i].message_id == messaggio.message_id) {
                    ackSpediti++;
                }
            }
            semOp(semidAckList, 0, 1);

            // Salvo il messaggio tra i miei messaggi solo se non ero l'ultimo a riceverlo
            if (ackSpediti != NUM_DEVICES - 1) {
                // sarò io a spedire il messaggio, quindi il sender sono io
                messaggio.pid_sender = mypid;
                for (int j = 0; j < MESS_DEV_MAX; j++) {
                    if (messaggi[j].message_id == -1) {
                        messaggi[j] = messaggio;
                        break;
                    }
                }
            }

        }
        //-------------------------------------------------------------------

        //--------------------------MOVIMENTO--------------------------------
        // Leggo le prossime posizioni
        nextPositions(posizioniFD, &x, &y);
        //Se casella è libera mi inserisco
        if (board[y + x * BOARD_SIDE_SIZE] == 0) {
            board[y + x * BOARD_SIDE_SIZE] = mypid;
            // Se non sono al primo giro metto a zero la mia vecchia posizione
            if (oldX != -1 && oldY != -1) {
                board[oldY + oldX * BOARD_SIDE_SIZE] = (pid_t) 0;
            }
        }
        oldX = x;
        oldY = y;
        //-------------------------------------------------------------------

        // Stampa info device in questo stile:
        // pidD1 i_D1 j_D1 msgs: lista message_id
        printf("%d %d %d msgs: ", mypid, x, y);
        for (int i = 0; i < MESS_DEV_MAX; i++) {
            if (messaggi[i].message_id != -1) {
                printf("%d ", messaggi[i].message_id);
            }
        }
        printf("\n");

        //Libero il semaforo del prossimo
        // Se sono l'ultimo processo, invece di liberare un semaforo stampo la riga di chiusura delle info device
        if (nProcesso != NUM_DEVICES - 1) {
            semOp(semidBoard, nProcesso + 1, 1);
        } else {
            printf("#############################################\n");
        }

    }

}

void nextPositions(int posizioniFD, int *x, int *y) {
    char buf[3] = {0};
    int bR = read(posizioniFD, buf, 3);
    // se sono dopo EOF mantengo posizione corrente
    if (bR == 0)
        // qua potrei anche decidere di tornare all'inizio del file volendo
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

void checkVicini(double dist, pid_t vicini[], int x, int y) {
    //Controllo tutte le celle della board, se trovo un device vicino lo aggiungo alla lista dei vicini
    int c = 0;
    for (int i = 0; i < BOARD_SIDE_SIZE; i++) {
        for (int j = 0; j < BOARD_SIDE_SIZE; j++) {
            //Trovo device tranne me stesso
            if (board[j + i * BOARD_SIDE_SIZE] != 0) {
                //Controllo se device è vicino
                double distanza = (double) sqrt(((x - i) * (x - i)) + ((y - j) * (y - j)));
                // Se distanza = 0 sono io!
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
    printf("[Device] %d I'm dead\n", mypid);
    exit(0);
}