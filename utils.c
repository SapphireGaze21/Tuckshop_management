#include <stdio.h>
#include <string.h>

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
        return 0; // already exists
    }
    FILE* file = fopen("users.txt", "a");
    if (!file) return 0;
    fprintf(file, "%s %s %s\n", username, password, role);
    fclose(file);
    return 1;
}