/// @file defines.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche del progetto.

#include "defines.h"
#include "err_exit.h"

void blockAllSignalsExcept(int signals[], int size) {
    // Creo un set di segnali pieno
    sigset_t allSignals;
    if (sigfillset(&allSignals) == -1)
        errExit("<Define> sigfillset failed");
    // Rimuovo i segnali specificati
    for (int i = 0; i < size; i++) {
        if (sigdelset(&allSignals, signals[i]) == -1)
            errExit("<Define> sigdelset failed");
    }
    if (sigprocmask(SIG_SETMASK, &allSignals, NULL) == -1)
        errExit("<Define> sigprocmask failed");
}