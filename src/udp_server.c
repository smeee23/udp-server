#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#define PORT 9979
#define BUFFER_SIZE 1024

int sockfd;

void handle_sigint(int sig) {
    printf("\nShutting down server...\n");
    close(sockfd);
    exit(EXIT_SUCCESS);
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);
    signal(SIGINT, handle_sigint);

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to the port
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Server listening on port %d...\n", PORT);

    // Listen for incoming messages
    while (1) {
        ssize_t received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
                                    (struct sockaddr *)&client_addr, &addr_len);
        if (received < 0) {
            perror("Receive failed");
            continue;
        }

        if (received >= BUFFER_SIZE) {
            buffer[BUFFER_SIZE - 1] = '\0'; // Ensure null-termination
        } else {
            buffer[received] = '\0';
        }
        printf("Received from %s:%d - %s\n",
        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

        // Send acknowledgment back to client
        const char *ack_message = "Received";
        ssize_t sent = sendto(sockfd, ack_message, strlen(ack_message), 0, 
                              (struct sockaddr *)&client_addr, addr_len);
        if (sent < 0) {
            perror("Send failed");
        }
    }

    // Close socket (though it never reaches here)
    close(sockfd);
    return 0;
}