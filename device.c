/// @file device.c
/// @brief Contiene l'implementazione dei DEVICE.

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"
#include "server.c"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/param.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <fcntl.h>


pid_t id;
int fifoFD;

void createFIFO() {
    id = getpid();
    char deviceFIFO[PATH_MAX];
    sprintf(deviceFIFO, "/tmp/dev_fifo.%d", id);
    fifoFD = mkfifo(deviceFIFO, O_WRONLY);
    if (deviceFIFO == -1)
        errExit("open failed");
}

int readNextX(int nCiclo, int nProcesso){
    int fp = open("/input/file_posizioni.txt", O_RDONLY);
    if (fp == -1)
        errExit("open failed");
    char riga[256];
    char rigaEff[256];

    //Mi sposto alla riga deisderata
    for (int i = 0; i < nCiclo; ++i) {
        while (fgets(riga, sizeof(riga), fp));
    }
    
    //Salvo la riga desiderata
    while (fgets(rigaEff, sizeof(rigaEff), fp));

    //Ritorno la x
    int stop=nProcesso*4;
    return rigaEff[stop];
}

int readNextY(int nCiclo, int nProcesso){
    int fp = open("/input/file_posizioni.txt", O_RDONLY);
    if (fp == -1)
        errExit("open");
    char riga[256];
    char rigaEff[256];

    //Mi sposto alla riga deisderata
    for (int i = 0; i < nCiclo; ++i) {
        while (fgets(riga, sizeof(riga), fp));
    }

    //Salvo la riga desiderata
    while (fgets(rigaEff, sizeof(rigaEff), fp));

    //Ritorno la y
    int stop=2+nProcesso*4;
    return rigaEff[stop];
}


int device(int nProcesso) {

    //Creo una FIFO per ogni device e salvo variabili utili del processo
    createFIFO();

    //N iterazioni ciclo
    int nCiclo=0;


    //Tutte le azioni del device
    while(true){
        int x,y;
        x=readNextX(nCiclo,nProcesso);
        y=readNextY(nCiclo,nProcesso);



        nCiclo++;
    }




    exit(0);
}