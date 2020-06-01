/// @file fifo.h
/// @brief Contiene la definizioni di variabili e
///         funzioni specifiche per la gestione delle FIFO.

#pragma once

#include <sys/param.h>

// crea la FIFO e salva nel buffer il path della FIFO
void createFIFO(pid_t id, char deviceFifoPath[]);