#ifndef MODELS_H
#define MODELS_H

typedef struct {
    int sock;
    char username[50];
    char role[20];
    int logged_in;
} Session;

// Simple structure to pass socket
typedef struct {
    int client_sock;
} client_arg_t;

typedef struct {
    long msg_type;
    int order_id;
    char username[50];
    char item[50];
    char item_category[20];
} OrderMessage;

#endif