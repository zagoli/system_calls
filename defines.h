/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once

#include <sys/param.h>

#define NUM_DEVICES 5

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
} AckClient;