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
#include <errno.h>
#include <time.h>

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: %s msg_queue_key\nCreato da Jacopo Zagoli e Gianluca Antolini\n", argv[0]);
        return 0;
    }

    errno = 0;
    key_t msgQueueKey = strtoul(argv[1], NULL, 10);
    if (errno == ERANGE || errno == EINVAL)
        errExit("<Server> failed at converting msg_queue key");

    // Blocco tutti i segnali
    blockAllSignalsExcept(NULL, 0);

    // Creo il messaggio e aspetto l'input dell'utente
    Message message;
    message.pid_sender = getpid();
    printf("Inserire pid del device a cui inviare il messaggio: ");
    scanf("%d", &message.pid_receiver);
    printf("Inserire id messaggio: ");
    scanf("%d", &message.message_id);
    printf("Inserire messaggio: ");
    fgets(message.message, 256, STDIN_FILENO);
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
    if (msqid == -1)
        errExit("<Client> open message queue failed");
    AckReportClient ackToPrint;
    // Leggo il messaggio a me destinato, cioè quello con msgtype = message id inviato da me
    if (msgrcv(msqid, &ackToPrint, sizeof(ackToPrint), message.message_id, 0) == -1)
        errExit("<Client> receive message from Message Queue failed");

    // Apro file di output
    char outPath[PATH_MAX];
    sprintf(outPath, "out_%d.txt", message.message_id);
    int outFd = open(outPath, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (outFd == -1)
        errExit("<Client> open output file failed");

    // TODO: la data non è stampata proprio come la vuole il prof
    // Scrivo ack su file di output
    char buffer[1024];
    // Prima riga dell'output
    sprintf(buffer, "Messaggio %d: %s\n", message.message_id, message.message);
    bW = write(outFd, buffer, strlen(buffer));
    if (bW == -1 || bW == 0)
        errExit("<Client> writing on output file failed");
    // Vari acks
    for (int i = 0; i < NUM_DEVICES; i++) {
        Acknowledgment currentAck = ackToPrint.acks[i];
        sprintf(buffer,
                "%d, %d, %s\n",
                currentAck.pid_sender, currentAck.pid_receiver, ctime(&currentAck.timestamp));
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