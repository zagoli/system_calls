/// @file device.h
/// @brief Contiene la definizioni di variabili e funzioni
///         specifiche per la gestione di DEVICE.

#pragma once

#include <sys/types.h>

int device();
int readNextY(int nCiclo);
int readNextX(int nCiclo);
void createFifo();
void getNProcesso(pid_t id);