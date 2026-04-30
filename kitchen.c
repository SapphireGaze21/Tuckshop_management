#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include "utils.h"
#include "models.h"

//for mq
int msgid;

//Graceful shutdown
volatile int running = 1;

void shutdown_handler(int sig) {
    running = 0;
    msgctl(msgid, IPC_RMID, NULL); // Deletes the message queue from RAM!
    if (shared_menu) {
        shmdt(shared_menu);
    }
    printf("\nKitchen shutting down and cleaning up IPC resources...\n");
    exit(0);
}

// ---------------------- KITCHEN WORKERS ----------------------

sem_t maggi_sem;
sem_t chin_sem;
sem_t packaged_sem;

void* maggi_worker(void* arg) {
    while (running) {
        OrderMessage batch[3]; // Batch size up to 3
        int batch_count = 0;

        // Wait for first order
        if (msgrcv(msgid, &batch[0], sizeof(OrderMessage) - sizeof(long), 1, 0) < 0) break;
        batch_count = 1;

        sem_wait(&maggi_sem);

        // Check for more orders
        while (batch_count < 3) {
            if (msgrcv(msgid, &batch[batch_count], sizeof(OrderMessage) - sizeof(long), 1, IPC_NOWAIT) > 0) {
                batch_count++;
            } else {
                break;
            }
        }
        printf("[MAGGI] Cooking batch of %d orders!\n", batch_count);
        for(int i = 0; i < batch_count; i++) {
            update_order_status(batch[i].order_id, "PROCESSING");
        }
        sleep(10); // simulate cooking for the whole batch
        for(int i = 0; i < batch_count; i++) {
            update_order_status(batch[i].order_id, "COMPLETED");
            printf("[MAGGI] Order %d (%s) Completed!\n", batch[i].order_id, batch[i].item);
            print_stock(batch[i].item);
        }
        sem_post(&maggi_sem);
    }
    return NULL;
}

void* chin_worker(void* arg) {
    while (running) {
        OrderMessage batch[2]; // Batch size up to 2
        int batch_count = 0;
        if (msgrcv(msgid, &batch[0], sizeof(OrderMessage) - sizeof(long), 2, 0) < 0) break;
        batch_count = 1;

        sem_wait(&chin_sem);

        while (batch_count < 2) {
            if (msgrcv(msgid, &batch[batch_count], sizeof(OrderMessage) - sizeof(long), 2, IPC_NOWAIT) > 0) {
                batch_count++;
            } else {
                break;
            }
        }
        printf("[CHINESE] Cooking batch of %d orders!\n", batch_count);
        for(int i = 0; i < batch_count; i++) {
            update_order_status(batch[i].order_id, "PROCESSING");
        }
        sleep(20);
        for(int i = 0; i < batch_count; i++) {
            update_order_status(batch[i].order_id, "COMPLETED");
            printf("[CHINESE] Order %d (%s) Completed!\n", batch[i].order_id, batch[i].item);
            print_stock(batch[i].item);
        }
        sem_post(&chin_sem);
    }
    return NULL;
}

void* packaged_worker(void* arg) {
    while (running) {
        OrderMessage batch[5]; // Batch size up to 5
        int batch_count = 0;

        if (msgrcv(msgid, &batch[0], sizeof(OrderMessage) - sizeof(long), 3, 0) < 0) break;
        batch_count = 1;

        sem_wait(&packaged_sem);

        while (batch_count < 5) {
            if (msgrcv(msgid, &batch[batch_count], sizeof(OrderMessage) - sizeof(long), 3, IPC_NOWAIT) > 0) {
                batch_count++;
            } else {
                break;
            }
        }
        printf("[PACKAGED FOOD] Processing batch of %d orders!\n", batch_count);
        for(int i = 0; i < batch_count; i++) {
            update_order_status(batch[i].order_id, "PROCESSING");
        }
        sleep(5);
        for(int i = 0; i < batch_count; i++) {
            update_order_status(batch[i].order_id, "COMPLETED");
            printf("[PACKAGED FOOD] Order %d (%s) Completed!\n", batch[i].order_id, batch[i].item);
            print_stock(batch[i].item);
        }
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

    key_t shm_key = ftok("progfile", 66);
    int shmid = shmget(shm_key, sizeof(SharedMenu), 0666);
    if (shmid >= 0) {
        shared_menu = (SharedMenu*) shmat(shmid, NULL, 0);
    } else {
        perror("Kitchen couldn't connect to shared memory");
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