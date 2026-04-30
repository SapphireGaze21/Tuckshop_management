#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <signal.h>
#include "utils.h"
#include "models.h"
#include <semaphore.h>

#define PORT 8080
#define MAX_CLIENTS 10

// Function declarations
void* handle_client(void* arg);

//for mq
int msgid;

//pipe for loggers
int pipe_fd[2];

//Graceful shutdown
volatile int running = 1;

void shutdown_handler(int sig) {
    running = 0;
}

// ---------------------- MAIN ----------------------

int main() {
    signal(SIGINT, shutdown_handler);
    int server_fd, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    pthread_t tid;

    key_t key = ftok("progfile", 65);
    msgid = msgget(key, 0666 | IPC_CREAT);

    pipe(pipe_fd);
    //  write end non-blocking
    int flags = fcntl(pipe_fd[1], F_GETFL, 0);
    fcntl(pipe_fd[1], F_SETFL, flags | O_NONBLOCK);

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);//IPV4 TCP 0->default tcp
    if (server_fd == 0) {
        perror("Socket Creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Setup address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;// to accept connections frm any network interface
    server_addr.sin_port = htons(PORT);//port conversion for machine

    // 3. Bind socket
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // 4. Listen
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d...\n", PORT);

    // 5. Accept clients loop
    while (running) {
        client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock < 0) {
            if (!running) break;   // exit cleanly
            perror("Accept failed");
            continue;
        }

        printf("New client connected\n");

        // Allocate memory for thread argument
        client_arg_t* client_arg = malloc(sizeof(client_arg_t));
        client_arg->client_sock = client_sock;

        // Create thread for client
        pthread_create(&tid, NULL, handle_client, (void*)client_arg);
        pthread_detach(tid); // Auto cleanup
    }

    close(server_fd);
    return 0;
}

// ---------------------- CLIENT HANDLER ----------------------

void* handle_client(void* arg) {

    client_arg_t* client = (client_arg_t*)arg;
    int sock = client->client_sock;

    free(client);

    Session session;
    session.sock = sock;    
    session.logged_in = 0;

    char buffer[1024];

    while (running) {
        memset(buffer, 0, sizeof(buffer));
        //read client
        int bytes = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) {
            printf("Client disconnected\n");
            break;
        }
        buffer[bytes] = '\0';

        printf("Received: %s\n", buffer);   

        //SIGNUP
        if (strncmp(buffer, "SIGNUP", 6) == 0) {
            char user[50], pass[50], role[20];
            sscanf(buffer, "SIGNUP %s %s %s", user, pass, role);

            if (signup(user, pass, role)) {
                write(sock, "Signup successful\n", 19);
            } else {
                write(sock, "User already exists\n", 21);
            }
        }
        //LOGIN
        else if (strncmp(buffer, "LOGIN", 5) == 0) {
            char user[50], pass[50];
            sscanf(buffer, "LOGIN %s %s", user, pass);
            if (login(user, pass, session.role)) {
                strcpy(session.username, user);
                session.logged_in = 1;
                write(sock, "Login successful\n", 17);
            } else {
                write(sock, "Login failed\n", 13);
            }
        }

        //Place order
        else if (strncmp(buffer, "PLACE_ORDER", 11) == 0) {
            if (!session.logged_in){
                write(sock, "Please login first\n", 20);
                continue;
            }

            if(strcmp(session.role, "GUEST") == 0){
                write(sock, "Guests cannot place orders\n", 27);
                continue;
            }
            char item[50];
            sscanf(buffer, "PLACE_ORDER %s", item);
            char category[20];
            get_item_category(item, category);

            if (strcmp(category, "PACKAGED") == 0) {
                if (!check_and_update_stock(item)) {
                    write(sock, "Item out of stock\n", 18);
                    continue;   // stop order here
                }
            }//now safe to order

            int order_id = add_order(session.username, item);
            //sending msg to kitchen process using mq
            OrderMessage msg;

            strcpy(msg.username, session.username);
            strcpy(msg.item, item);
            strcpy(msg.item_category, category);

            msg.order_id = order_id;
            msg.msg_type = get_msg_type(category);

            msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0);

            if (order_id < 0) {
                write(sock, "Order failed\n", 13);
            } else {
                char msg[100];
                sprintf(msg, "Order placed. ID: %d\n", order_id);
                write(sock, msg, strlen(msg));
            }
        }

        //order status
        else if (strncmp(buffer, "VIEW_STATUS", 11) == 0) {
            if (!session.logged_in) {
                write(sock, "Please login first\n", 20);
                continue;
            }
            int order_id;
            sscanf(buffer, "VIEW_STATUS %d", &order_id);
            char result[100];
            get_order_status(order_id, result);
            write(sock, result, strlen(result));
        }
        //add item
       else if (strncmp(buffer, "ADD_ITEM", 8) == 0) {
            if (!session.logged_in || strcmp(session.role, "ADMIN") != 0) {
                write(sock, "Admin access required\n", 22);
                continue;
            }
            char item[50], category[20];
            int qty = -1;
            // allow optional quantity
            sscanf(buffer, "ADD_ITEM %s %s %d", item, category, &qty);
            add_menu_item(item, category, qty);
            write(sock, "Item added to menu\n", 20);
        }
        //view menu
        else if (strncmp(buffer, "VIEW_MENU", 9) == 0) {
            FILE* file = fopen("menu.txt", "r");
            if (!file) {
                write(sock, "Menu empty\n", 11);
                continue;
            }
            char response[1024] = "";
            char line[100];
            while (fgets(line, sizeof(line), file)) {
                strcat(response, line);
            }
            write(sock, response, strlen(response));
            fclose(file);
        }
    }
    close(sock);
    return NULL;
}

