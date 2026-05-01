#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
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

//for shm
int shmid;

// Active clients counter and mutex
int active_clients = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

//pipe for loggers
int pipe_fd[2];

void log_event(const char* event) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "[LOG] %s\n", event);
    write(pipe_fd[1], buffer, strlen(buffer));
}

//Graceful shutdown
volatile int running = 1;

void shutdown_handler(int sig) {
    running = 0;
    msgctl(msgid, IPC_RMID, NULL); // Deletes the message queue from RAM!
    
    // Save to file and clean up shared memory
    if (shared_menu) {
        FILE* menu_file = fopen("menu.txt", "w");
        if (menu_file) {
            for (int i = 0; i < shared_menu->count; i++) {
                fprintf(menu_file, "%s %s %d\n", shared_menu->items[i].name, shared_menu->items[i].category, shared_menu->items[i].quantity);
            }
            fclose(menu_file);
        }
        pthread_mutex_destroy(&shared_menu->lock);
        shmdt(shared_menu);
        shmctl(shmid, IPC_RMID, NULL);
    }

    printf("\nServer shutting down and cleaning up IPC resources...\n");
    exit(0);
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

    // Setup Shared Memory
    key_t shm_key = ftok("progfile", 66);
    shmid = shmget(shm_key, sizeof(SharedMenu), 0666 | IPC_CREAT);
    if (shmid < 0) {
        perror("shmget failed");
        exit(1);
    }
    shared_menu = (SharedMenu*) shmat(shmid, NULL, 0);

    // Initialize mutex as PTHREAD_PROCESS_SHARED
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_menu->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    // Load menu from file
    shared_menu->count = 0;
    FILE* menu_file = fopen("menu.txt", "r");
    if (menu_file) {
        char it[50], cat[20];
        int qty;
        while (fscanf(menu_file, "%s %s %d", it, cat, &qty) != EOF && shared_menu->count < MAX_MENU_ITEMS) {
            strcpy(shared_menu->items[shared_menu->count].name, it);
            strcpy(shared_menu->items[shared_menu->count].category, cat);
            shared_menu->items[shared_menu->count].quantity = qty;
            shared_menu->count++;
        }
        fclose(menu_file);
    }

    pipe(pipe_fd);

    pid_t logger_pid = fork();
    if (logger_pid == 0) {
        // Child: Logger Process
        signal(SIGINT, SIG_IGN); // Ignore Ctrl+C so only the parent cleans up!
        close(pipe_fd[1]); // Close write end
        FILE* log_file = fopen("server_logs.txt", "a");
        if (log_file) {
            char log_buf[1024];
            while (1) {
                int bytes = read(pipe_fd[0], log_buf, sizeof(log_buf) - 1);
                if (bytes > 0) {
                    log_buf[bytes] = '\0';
                    fprintf(log_file, "%s", log_buf);
                    fflush(log_file); // Ensure immediate write
                } else if (bytes == 0) {
                    break; // Parent closed pipe
                }
            }
            fclose(log_file);
        }
        exit(0);
    }
    
    // Parent continues
    close(pipe_fd[0]); // Parent closes read end
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

    // Lock, update, and unlock when a client connects
    pthread_mutex_lock(&client_mutex);
    active_clients++;
    printf("New client connected. Live Clients: %d\n", active_clients);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "New client connected. Total live clients: %d", active_clients);
    log_event(log_msg);
    
    pthread_mutex_unlock(&client_mutex);

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
                snprintf(log_msg, sizeof(log_msg), "User '%s' signed up as %s", user, role);
                log_event(log_msg);
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
                snprintf(log_msg, sizeof(log_msg), "User '%s' logged in as %s", user, session.role);
                log_event(log_msg);
                
                char resp[100];
                snprintf(resp, sizeof(resp), "Login successful as %s\n", session.role);
                write(sock, resp, strlen(resp));
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

            if (strcmp(category, "PACKAGEDFOOD") == 0) {
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
                snprintf(log_msg, sizeof(log_msg), "User '%s' placed order %d (%s)", session.username, order_id, item);
                log_event(log_msg);
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
            char response[1024] = "";
            get_menu(response);
            write(sock, response, strlen(response));
        }
        //logout
        else if (strncmp(buffer, "LOGOUT", 6) == 0) {
            if (session.logged_in) {
                snprintf(log_msg, sizeof(log_msg), "User '%s' logged out", session.username);
                log_event(log_msg);
                session.logged_in = 0;
                write(sock, "Logged out successfully\n", 24);
            } else {
                write(sock, "Not logged in\n", 14);
            }
        }
        //invalid command
        else {
            write(sock, "Invalid command\n", 16);
        }
    }

    // Lock, update, and unlock when client disconnects
    pthread_mutex_lock(&client_mutex);
    active_clients--;
    printf("Client disconnected. Live Clients: %d\n", active_clients);
    
    snprintf(log_msg, sizeof(log_msg), "Client disconnected. Total live clients: %d", active_clients);
    log_event(log_msg);
    
    pthread_mutex_unlock(&client_mutex);

    close(sock);
    return NULL;
}

