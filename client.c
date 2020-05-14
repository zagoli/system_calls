/// @file client.c
/// @brief Contiene l'implementazione del client.

#include "defines.h"
#include "err_exit.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/msg.h>
#include <string.h>

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: %s msg_queue_key\n", argv[0]);
        return 0;
    }

    int msgQueueKey = atoi(argv[1]);

    // Creo il messaggio e aspetto l'input dell'utente
    Message message;
    message.pid_sender = getpid();
    printf("Inserire pid del device a cui inviare il messaggio: ");
    scanf("%d", &message.pid_receiver);
    printf("Inserire id messaggio: ");
    scanf("%d", &message.message_id);
    printf("Inserire messaggio: ");
    scanf("%255s", &message.message);
    printf("Inserire massima distanza comunicazione per il messaggio: ");
    scanf("%lf", &message.max_distance);

    // Apro la FIFO del device
    char fifoPath[PATH_MAX];
    sprintf(fifoPath, "/tmp/dev_fifo.%d", message.pid_receiver);
    int fifoFd = open(fifoPath, O_WRONLY);
    if (fifoFd == -1)
        errExit("<Client> open fifo failed");

    // Scrivo Message sulla FIFO e la chiudo
    int bW = write(fifoFd, &message, sizeof(message));
    if (bW == 0 || bW == -1)
        errExit("<Client> write on fifo failed");
    if (close(fifoFd) == -1)
        errExit("<Client> close fifo failed");

    // Mi metto in attesa di ricevere gli ack sulla message queue
    int msqid = msgget(msgQueueKey, S_IRUSR | S_IWUSR);
    AckReportClient ackToPrint;
    // Leggo il messaggio a me destinato, cioè quello con msgtype = mio pid
    if (msgrcv(msqid, &ackToPrint, sizeof(ackToPrint), getpid(), 0) == -1)
        errExit("<Client> receive message from Message Queue failed");

    // Apro file di output
    char outPath[PATH_MAX];
    sprintf(outPath, "out_%d.txt", message.message_id);
    int outFd = open(outPath, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (outFd == -1)
        errExit("<Client> open output file failed");

    // Scrivo ack su file di output
    char buffer[1024];
    for (int i = 0; i < NUM_DEVICES; i++) {
        Acknowledgment currentAck = ackToPrint.acks[i];
        sprintf(buffer,
                "sender: %d | receiver: %d | msg_id: %d | timestamp: %ld\n",
                currentAck.pid_sender, currentAck.pid_receiver, currentAck.message_id, currentAck.timestamp);
        bW = write(outFd, buffer, strlen(buffer));
        if (bW == -1 || bW == 0)
            errExit("<Client> writing on output file failed");
    }

    // Chiudo file di output
    if (close(outFd) == -1)
        errExit("<Client> closing out file failed");

    // Termino

    return 0;
}