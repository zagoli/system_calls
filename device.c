/// @file device.c
/// @brief Contiene l'implementazione dei DEVICE.

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"
#include "fifo.c"
#include "server.c"

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>


pid_t id;
int fifoFD;



int readNextX(int fp, int nProcesso){
    char riga[256];


    //Salvo la riga desiderata
    while (fgets(riga, sizeof(riga), fp));

    //Ritorno la x
    int stop=nProcesso*4;
    return riga[stop];
}

int readNextY(int fp, int nProcesso){
    char riga[256];

    //Salvo la riga desiderata
    while (fgets(riga, sizeof(riga), fp));

    //Ritorno la y
    int stop=2+nProcesso*4;
    return riga[stop];
}

void checkVicini(double dist, pid_t **board, pid_t vicini[],int x,int y){

    //Controllo tutte le celle della board, se trovo un device vicino lo aggiungo alla lista dei vicini
    int c=0;
    for (int i = 0; i < BOARD_SIDE_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIDE_SIZE; ++j) {
            //Trovo device
            if(board[i][j]!=0){
                //Controllo se device è vicino
                if(sqrt(((x-i)*(x-i))+((y-j)*(y-j)))<=(double)dist){
                    vicini[c]=board[i][j];
                    c++;
                }
            }
        }
    }

}

void stopDevice(int signal){
    close(fifoFD);
}

_Noreturn int device(int nProcesso, char path[]) {

    id = getpid();
    //Creo una FIFO per ogni device e salvo variabili utili del processo
    fifoFD=createFIFO(id);

    Message messaggi[MESS_DEV_MAX];
    for (int k = 0; k < MESS_DEV_MAX; ++k) {
        messaggi[k].message_id=-1;
    }

    //Blocco segnali che non siano sigterm
    int sig[] = {SIGTERM};
    blockAllSignalsExcept(sig, 1);

    if (signal(SIGTERM, stopDevice) == SIG_ERR)
        errExit("<Device> setting SIGTERM handler failed");

    //Apro il file delle posizioni
    int fp = open(path, O_RDONLY);
    if (fp == -1)
        errExit("open failed");


    //Tutte le azioni del device
    while(true){

        //Leggo la prossima posizione
        int x,y;
        x=readNextX(fp,nProcesso);
        y=readNextY(fp,nProcesso);

        //INVIO MESSAGGI
        //Controllo se ho messaggi
        bool vuoto=true;
        for (int i = 0; i < MESS_DEV_MAX; ++i) {
            if(messaggi[i].message_id!=-1){
                vuoto=false;
                break;
            }
        }

        //invio
        if(vuoto==false) {

            // Aspetto che il semaforo sia libero
            semOp(semidBoard, nProcesso, -1);

            // Accedo alla board
            pid_t **board = attachSegment(boardId);

            //Trovo i vicini
            pid_t vicini[NUM_DEVICES-1]={(pid_t)-1};
            for (int i = 0; i < MESS_DEV_MAX; ++i) {
                if(messaggi[i].message_id!=-1){
                    double dist = messaggi[i].max_distance;
                    checkVicini(dist,board,vicini,x,y);

                    bool giaRicevutoTutti=true;

                    //una volta trovati i vicini mando ad ognuno il messaggio in questione, se non l'hanno già ricevuto
                    for (int j = 0; j <NUM_DEVICES-1 ; ++j) {
                        giaRicevutoTutti=true;
                        if (vicini[i]!=-1){

                            //Controllo se vicino ha già ricevuto il messaggio nella lista delle ack

                            // Aspetto che il semaforo sia libero
                            semOp(semidAckList, 0, -1);
                            // Accedo alla board
                            Acknowledgment *ackList = attachSegment(ackListId);
                            bool giaRicevuto=false;
                            for (int k = 0; k < ACK_MAX; ++k) {
                                if (ackList[k].message_id==messaggi[k].message_id && ackList[k].pid_sender==messaggi[k].pid_receiver){
                                    giaRicevuto=true;
                                    break;
                                }
                                else{
                                    giaRicevutoTutti=false;
                                }
                            }
                            //Se non l'ha gia ricevuto, glielo posso mandare
                            if (giaRicevuto==false){
                                //Apro la fifo del device vicino
                                char deviceVicinoFIFO[PATH_MAX];
                                sprintf(deviceVicinoFIFO, "/tmp/dev_fifo.%d", vicini[i]);
                                int vicinoFD = open(deviceVicinoFIFO, O_WRONLY|O_NONBLOCK);
                                if (vicinoFD == -1)
                                    errExit("open failed");
                                //Scrivo il messaggio
                                write(vicinoFD,&messaggi[i],sizeof(Message));
                            }
                            // Libero il semaforo
                            semOp(semidAckList, 0, 1);

                        }
                    }

                    //Hanno gia ricevuto tutti il messaggio, lo cancello
                    if(giaRicevutoTutti){
                        messaggi[i].message_id=-1;
                    }

                    //Svuoto la lista dei vicini per passare al prossimo messaggio
                    for (int k = 0; k < NUM_DEVICES-1; ++k) {
                        vicini[k]=(pid_t)-1;
                    }

                }
            }

            // Libero il semaforo
            if (nProcesso!=NUM_DEVICES) {
                semOp(semidBoard, nProcesso + 1, 1);
            }
        }

        //RICEZIONE MESSAGGI
        //Leggo eventuali messaggi
        open(fp, O_RDONLY, O_NONBLOCK);
        Message messaggio;
        Acknowledgment ack;
        int nm;
        while(read(fp, &messaggio, sizeof(Message))!=-1){

            //Letto un messaggio, creo un ack
            messaggi[nm]=messaggio;
            ack.message_id=messaggio.message_id;
            ack.pid_sender=messaggio.pid_sender;
            ack.pid_receiver=id;
            ack.timestamp=time(NULL);

            //Aggiungo l'ack al segmento di memoria apposito
            // Aspetto che il semaforo sia libero
            semOp(semidAckList, 0, -1);

            // Accedo alla board
            Acknowledgment *ackList = attachSegment(ackListId);
            for (int i = 0; i < ACK_MAX; ++i) {
                if (ackList[i].message_id==-1){
                    ackList[i]=ack;
                }
            }

            // Libero il semaforo
            semOp(semidAckList, 0, 1);

            nm++;
        }


        //MOVIMENTO
        // Aspetto che il semaforo sia libero
        semOp(semidBoard, nProcesso, -1);

        // Accedo alla board
        pid_t **board = attachSegment(boardId);

        //Se casella è libera mi inserisco
        if(board[x][y]!=0){
            board[x][y]=id;
        }

        //Libero il semaforo
        if (nProcesso!=NUM_DEVICES) {
            semOp(semidBoard, nProcesso + 1, 1);
        }
    }

}