#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[1024];

    // 1. Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    // 2. Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 3. Connect
    connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

    while (1) {
        printf("Enter message: ");
        fgets(buffer, sizeof(buffer), stdin);

        // send to server
        write(sock, buffer, strlen(buffer));

        // read response
        memset(buffer, 0, sizeof(buffer));
        read(sock, buffer, sizeof(buffer));

        printf("Server: %s\n", buffer);
    }

    close(sock);
    return 0;
}