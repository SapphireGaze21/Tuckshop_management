#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

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
    FILE* file = fopen("orders.txt", "r");
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
    remove("orders.txt");
    rename("temp.txt", "orders.txt");
}

void get_item_category(char* item, char* category) {

    if (strstr(item, "maggi") != NULL)
        strcpy(category, "MAGGI");

    else if (strstr(item, "dosa") != NULL)
        strcpy(category, "DOSA");

    else if (strstr(item, "coke") != NULL || strstr(item, "drink") != NULL)
        strcpy(category, "DRINK");

    else
        strcpy(category, "OTHER");
}

int get_msg_type(char* category) {

    if (strcmp(category, "MAGGI") == 0) return 1;
    if (strcmp(category, "DOSA") == 0) return 2;
    if (strcmp(category, "DRINK") == 0) return 3;

    return 4; // OTHER
}