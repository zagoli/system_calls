/// @file fifo.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione delle FIFO.

#include "err_exit.h"
#include "fifo.h"

#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>


int createFIFO(pid_t id) {
    char deviceFIFO[PATH_MAX];
    sprintf(deviceFIFO, "/tmp/dev_fifo.%d", id);
    int fifoFD = mkfifo(deviceFIFO, O_WRONLY);
    if (fifoFD == -1)
        errExit("open failed");
    return deviceFIFO;
}