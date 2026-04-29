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

#define PORT 8080
#define MAX_CLIENTS 10

    // Function declarations
void* handle_client(void* arg);
void* kitchen_worker(void* arg);

    // Simple structure to pass socket
typedef struct {
        int client_sock;
    } client_arg_t;

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

    // 5. Start kitchen worker threads (example: 3 workers)
    pthread_t kitchen_threads[3];
    for (int i = 0; i < 3; i++) {
        pthread_create(&kitchen_threads[i], NULL, kitchen_worker, (void*)(long)i);
    }

    // 6. Accept clients loop
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
typedef struct {
    int sock;
    char username[50];
    char role[20];
    int logged_in;
} Session;

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

        //LOGIN
        if (strncmp(buffer, "LOGIN", 5) == 0) {
            char user[50], pass[50];
            sscanf(buffer, "LOGIN %s %s", user, pass);

            // TEMP logic (we’ll replace with file later)
            if (strcmp(user, "admin") == 0 && strcmp(pass, "123") == 0) {
                strcpy(session.username, user);
                strcpy(session.role, "ADMIN");
                session.logged_in = 1;
                write(sock, "Login successful\n", 17);
            } else {
                write(sock, "Login failed\n", 13);
            }
        } 

        //Place order
        else if (strncmp(buffer, "PLACE_ORDER", 11) == 0) {
            if (!session.logged_in) {
                write(sock, "Please login first\n", 20);
                continue;
            }
            char item[50];
            sscanf(buffer, "PLACE_ORDER %s", item);

            // TEMP behavior
            printf("User %s ordered %s\n", session.username, item);

            write(sock, "Order placed\n", 13);
        }

        else if (strncmp(buffer, "VIEW_STATUS", 11) == 0) {
            if (!session.logged_in) {
                write(sock, "Please login first\n", 20);
                continue;
            }
            write(sock, "Status: Processing\n", 20);
        }
    }
    close(sock);
    return NULL;
}


// ---------------------- KITCHEN WORKER ----------------------

void* kitchen_worker(void* arg) {
    int worker_id = (int)(long)arg;

    while (running) {
        // TODO:
        // - Check item queues
        // - Dequeue order
        // - Update status
        

        sleep(2); // simulate waiting
    }

    return NULL;
}