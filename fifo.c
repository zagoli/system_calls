/// @file fifo.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione delle FIFO.

#include "err_exit.h"
#include "fifo.h"

#include <sys/stat.h>
#include <stdio.h>


void createFIFO(pid_t id, char deviceFifoPath[]) {
    sprintf(deviceFifoPath, "/tmp/dev_fifo.%d", id);
    int fifoFD = mkfifo(deviceFifoPath, S_IRUSR | S_IWUSR);
    if (fifoFD == -1)
        errExit("<Fifo> mkfifo failed");
}