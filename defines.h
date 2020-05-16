/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once

#include <sys/param.h>

// Numero di device
#define NUM_DEVICES 5
// Massimo numero di ack in AckList
#define ACK_MAX 100
// Massimo numero di messaggi in un device
#define MESS_DEV_MAX 25
// Dimensione lato board
#define BOARD_SIDE_SIZE 10

typedef struct {
    pid_t pid_sender;
    pid_t pid_receiver;
    int message_id;
    char message[256];
    double max_distance;
} Message;

typedef struct {
    pid_t pid_sender;
    pid_t pid_receiver;
    int message_id;
    time_t timestamp;
} Acknowledgment;

typedef struct {
    long mtype;
    Acknowledgment acks[NUM_DEVICES];
} AckReportClient;

// Questa funzione blocca tutti i segnali eccetto quelli contenuti nell'array signals, con size elementi
void blockAllSignalsExcept(int signals[], int size);