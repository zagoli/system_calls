/// @file shared_memory.h
/// @brief Contiene la definizioni di variabili e funzioni
///         specifiche per la gestione della MEMORIA CONDIVISA.

#pragma once

#include <sys/types.h>

int createMemSegment(size_t size);

void *attachSegment(int shmid);