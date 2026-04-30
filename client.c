#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int logged_in = 0;
    char role[20] = "";

    int sock;
    struct sockaddr_in server_addr;
    char buffer[1024];

    printf("\n===== Tuck Shop System =====\n");
    printf("1. LOGIN\n");
    printf("2. SIGNUP\n");
    printf("Choose option: ");
    
    // 1. Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    // 2. Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 3. Connect
    connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

    while (1) {

        if (!logged_in) {
            printf("\nCommands:\n");
            printf("LOGIN username password\n");
            printf("SIGNUP username password role\n");
        }
        else if (strcmp(role, "ADMIN") == 0) {
            printf("\n[ADMIN MENU]\n");
            printf("ADD_ITEM item category qty\n");
            printf("VIEW_MENU\n");
            printf("LOGOUT\n");
        }
        else if (strcmp(role, "USER") == 0) {
            printf("\n[USER MENU]\n");
            printf("PLACE_ORDER item\n");
            printf("VIEW_STATUS id\n");
            printf("VIEW_MENU\n");
            printf("LOGOUT\n");
        }
        else if (strcmp(role, "GUEST") == 0) {
            printf("\n[GUEST MENU]\n");
            printf("VIEW_MENU\n");
            printf("VIEW_STATUS id\n");
            printf("LOGOUT\n");
        }
        printf("Enter message: ");
        fgets(buffer, sizeof(buffer), stdin);
        // send to server
        write(sock, buffer, strlen(buffer));
        // read response
        memset(buffer, 0, sizeof(buffer));
        read(sock, buffer, sizeof(buffer));
        printf("Server: %s\n", buffer);
        // detect login success and role
        if (strstr(buffer, "Login successful")) {
            logged_in = 1;
            if (strstr(buffer, "ADMIN")) strcpy(role, "ADMIN");
            else if (strstr(buffer, "USER")) strcpy(role, "USER");
            else if (strstr(buffer, "GUEST")) strcpy(role, "GUEST");
        }
        // detect logout
        if (strstr(buffer, "Logged out")) {
            logged_in = 0;
            strcpy(role, "");
        }
    }

    close(sock);
    return 0;
}