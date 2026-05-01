#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "models.h"

SharedMenu* shared_menu = NULL;

int user_exists(char* username){
    FILE* file = fopen("users.txt", "r");
    if (!file) return 0;
    char u[50], p[50], r[20];
    while (fscanf(file, "%s %s %s", u, p, r) != EOF) {
        if (strcmp(u, username) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
} //check if exists in db(file)

int login(char* username, char* password, char* role) {
    FILE* file = fopen("users.txt", "r");
    if (!file) return 0;

    char u[50], p[50], r[20];

    while (fscanf(file, "%s %s %s", u, p, r) != EOF) {
        if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
            strcpy(role, r);
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

int signup(char* username, char* password, char* role) {
    if (user_exists(username)) {
        return 0;
    }
    int fd = open("users.txt", O_WRONLY | O_APPEND);
    if (fd < 0) return -1;
    struct flock lock;
    lock.l_type = F_WRLCK;   // write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;          // whole file
    // acquire lock
    fcntl(fd, F_SETLKW, &lock);
    // write safely
    char buffer[200];
    snprintf(buffer, sizeof(buffer), "%s %s %s\n", username, password, role);
    write(fd, buffer, strlen(buffer));
    // release lock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return 1;
}

int get_next_order_id() {
    FILE* file = fopen("orders.txt", "r");
    if (!file) return 1;

    int id = 0, last_id = 0;
    char user[50], item[50], status[20];

    while (fscanf(file, "%d %s %s %s", &id, user, item, status) != EOF) {
        last_id = id;
    }

    fclose(file);
    return last_id + 1;
}

int add_order(char* username, char* item) {
    int fd = open("orders.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return -1;

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    // lock file
    fcntl(fd, F_SETLKW, &lock);
    int order_id = get_next_order_id();
    char buffer[200];
    snprintf(buffer, sizeof(buffer), "%d %s %s PLACED\n", order_id, username, item);
    write(fd, buffer, strlen(buffer));
    // unlock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);

    close(fd);
    return order_id;
}

int check_and_update_stock(char* item) {
    if (!shared_menu) return 0;
    int success = 0;
    pthread_mutex_lock(&shared_menu->lock);
    for (int i = 0; i < shared_menu->count; i++) {
        if (strcmp(shared_menu->items[i].name, item) == 0 && strcmp(shared_menu->items[i].category, "PACKAGEDFOOD") == 0) {
            if (shared_menu->items[i].quantity > 0) {
                shared_menu->items[i].quantity--;
                success = 1;
            }
            break;
        }
    }
    pthread_mutex_unlock(&shared_menu->lock);
    return success;
}

void get_order_status(int order_id, char* result) {
    FILE* file = fopen("orders.txt", "r");
    if (!file) {
        strcpy(result, "No orders found\n");
        return;
    }
    int id;
    char user[50], item[50], status[20];
    while (fscanf(file, "%d %s %s %s", &id, user, item, status) != EOF) {
        if (id == order_id) {
            sprintf(result, "Order %d: %s\n", order_id, status);
            fclose(file);
            return;
        }
    }
    fclose(file);
    strcpy(result, "Order not found\n");
}

void update_order_status(int order_id, char* new_status) {
    int fd = open("orders.txt", O_RDWR);
    if (fd < 0) return;
    //lock file
    struct flock lock;
    lock.l_type = F_WRLCK;   // write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;          // whole file
    fcntl(fd, F_SETLKW, &lock);  // wait until lock acquired
    // critical section starts
    FILE* file = fdopen(fd, "r");
    FILE* temp = fopen("temp.txt", "w");
    int id;
    char user[50], item[50], status[20];
    while (fscanf(file, "%d %s %s %s", &id, user, item, status) != EOF) {
        if (id == order_id) {
            fprintf(temp, "%d %s %s %s\n", id, user, item, new_status);
        } else {
            fprintf(temp, "%d %s %s %s\n", id, user, item, status);
        }
    }
    fclose(file);
    fclose(temp);
    // overwrite original file
    remove("orders.txt");
    rename("temp.txt", "orders.txt");
    //critical section ends

    //file unlock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);

    close(fd);
}


int get_msg_type(char* category) {

    if (strcmp(category, "MAGGI") == 0) return 1;
    if (strcmp(category, "CHINESE") == 0) return 2;
    if (strcmp(category, "PACKAGEDFOOD") == 0) return 3;

    return 4; // OTHER
}

void get_item_category(char* item, char* category) {
    if (!shared_menu) {
        strcpy(category, "OTHER");
        return;
    }
    pthread_mutex_lock(&shared_menu->lock);
    for (int i = 0; i < shared_menu->count; i++) {
        if (strcmp(shared_menu->items[i].name, item) == 0) {
            strcpy(category, shared_menu->items[i].category);
            pthread_mutex_unlock(&shared_menu->lock);
            return;
        }
    }
    pthread_mutex_unlock(&shared_menu->lock);
    strcpy(category, "OTHER");
}


void add_menu_item(char* item, char* category, int quantity) {
    if (!shared_menu) return;
    pthread_mutex_lock(&shared_menu->lock);
    int found = 0;
    for (int i = 0; i < shared_menu->count; i++) {
        if (strcmp(shared_menu->items[i].name, item) == 0) {
            shared_menu->items[i].quantity += quantity;
            found = 1;
            break;
        }
    }
    if (!found && shared_menu->count < MAX_MENU_ITEMS) {
        strcpy(shared_menu->items[shared_menu->count].name, item);
        strcpy(shared_menu->items[shared_menu->count].category, category);
        shared_menu->items[shared_menu->count].quantity = quantity;
        shared_menu->count++;
    }
    pthread_mutex_unlock(&shared_menu->lock);
}

void get_menu(char* response) {
    if (!shared_menu || shared_menu->count == 0) {
        strcpy(response, "Menu empty\n");
        return;
    }
    response[0] = '\0';
    pthread_mutex_lock(&shared_menu->lock);
    for (int i = 0; i < shared_menu->count; i++) {
        char line[100];
        sprintf(line, "%s %s %d\n", shared_menu->items[i].name, shared_menu->items[i].category, shared_menu->items[i].quantity);
        strcat(response, line);
    }
    pthread_mutex_unlock(&shared_menu->lock);
}

void print_stock(char* item) {
    if (!shared_menu) return;
    pthread_mutex_lock(&shared_menu->lock);
    for (int i = 0; i < shared_menu->count; i++) {
        if (strcmp(shared_menu->items[i].name, item) == 0) {
            printf("[STOCK] %s remaining in stock: %d\n", item, shared_menu->items[i].quantity);
            break;
        }
    }
    pthread_mutex_unlock(&shared_menu->lock);
}