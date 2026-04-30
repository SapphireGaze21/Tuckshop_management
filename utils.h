#ifndef UTILS_H
#define UTILS_H

int signup(char* username, char* password, char* role);
int login(char* username, char* password, char* role);

int add_order(char* username, char* item);
void get_order_status(int order_id, char* result);
void update_order_status(int order_id, char* new_status);

void get_item_category(char* item, char* category);
int get_msg_type(char* category);
void add_menu_item(char* item, char* category, int quantity);
int check_and_update_stock(char* item);

#endif