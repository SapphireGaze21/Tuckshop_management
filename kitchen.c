#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <signal.h>
#include "utils.h"
#include "models.h"

//for mq
int msgid;

//Graceful shutdown
volatile int running = 1;

void shutdown_handler(int sig) {
    running = 0;
}

// ---------------------- KITCHEN WORKERS ----------------------

sem_t maggi_sem;
sem_t chin_sem;
sem_t packaged_sem;

void* maggi_worker(void* arg) {
    while (running) {
        OrderMessage msg;
        // wait for order from queue
        msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 1, 0);
        sem_wait(&maggi_sem);
        // strated cooking
        printf("[MAGGI] Processing order %d (%s)\n",msg.order_id, msg.item);
        update_order_status(msg.order_id, "PROCESSING");
        sleep(10);// simulate cooking
        update_order_status(msg.order_id, "COMPLETED");
        //free stove
        sem_post(&maggi_sem);
    }
    return NULL;
}

void* chin_worker(void* arg) {
    while (running) {
        OrderMessage msg;
        msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 2, 0);
        sem_wait(&chin_sem);
        printf("[CHINESE] Processing order %d (%s)\n",msg.order_id, msg.item);
        update_order_status(msg.order_id, "PROCESSING");
        sleep(20);
        update_order_status(msg.order_id, "COMPLETED");
        sem_post(&chin_sem);
    }
    return NULL;
}

void* packaged_worker(void* arg) {
    while (running) {
        OrderMessage msg;
        msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 3, 0);
        sem_wait(&packaged_sem);
        printf("[PACKAGED FOOD] Processing order %d (%s)\n",msg.order_id, msg.item);
        update_order_status(msg.order_id, "PROCESSING");
        sleep(5);
        update_order_status(msg.order_id, "COMPLETED");
        sem_post(&packaged_sem);
    }
    return NULL;
}

// ---------------------- MAIN ----------------------

int main() {
    signal(SIGINT, shutdown_handler);

    key_t key = ftok("progfile", 65);
    msgid = msgget(key, 0666 | IPC_CREAT);

    if (msgid < 0) {
        perror("msgget failed");
        exit(1);
    }

    sem_init(&maggi_sem, 0, 2);
    sem_init(&chin_sem, 0, 1);
    sem_init(&packaged_sem, 0, 3);

    printf("Kitchen process started...\n");

    pthread_t maggi_thread, chin_thread, packaged_thread;

    pthread_create(&maggi_thread, NULL, maggi_worker, NULL);
    pthread_create(&chin_thread, NULL, chin_worker, NULL);
    pthread_create(&packaged_thread, NULL, packaged_worker, NULL);

    pthread_join(maggi_thread, NULL);
    pthread_join(chin_thread, NULL);
    pthread_join(packaged_thread, NULL);

    return 0;
}