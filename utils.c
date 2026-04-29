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