/// @file semaphore.h
/// @brief Contiene la definizioni di variabili e funzioni
///         specifiche per la gestione dei SEMAFORI.

#pragma once

// definition of the union semun
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/* semOp is a support function to manipulate a semaphore's value
 * of a semaphore set. semid is a semaphore set identifier, sem_num is the
 * index of a semaphore in the set, sem_op is the operation performed on sem_num
 */
void semOp(int semid, unsigned short sem_num, short sem_op);

/* semCreate è una funzione che crea un set di semafori con parametri prefissati e lo inizializza
 * con i valori passati nell'array semValuesInit
 */
int semCreate(int nSems, unsigned short semValuesInit[]);