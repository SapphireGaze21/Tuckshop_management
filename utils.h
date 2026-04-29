#ifndef UTILS_H
#define UTILS_H

int signup(char* username, char* password, char* role);
int login(char* username, char* password, char* role);

int add_order(char* username, char* item);
void get_order_status(int order_id, char* result);

#endif