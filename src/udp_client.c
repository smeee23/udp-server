#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>

#define SERVER_IP "192.168.1.3"  
#define SERVER_PORT 9979
#define BUFFER_SIZE 1024
int sockfd;

void handle_sigint(int sig) {
    printf("\nClosing client socket...\n");
    close(sockfd);
    exit(EXIT_SUCCESS);
}

void send_udp_message(const char *message) {
    int sock;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char recv_buffer[BUFFER_SIZE];
    int enable_broadcast = 1;
    signal(SIGINT, handle_sigint);

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Optional: Enable broadcasting
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &enable_broadcast, sizeof(enable_broadcast)) < 0) {
        perror("Failed to set broadcast option");
    }

    // Set up the server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported \n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 2;  // 2 seconds timeout
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed");
    }

    // Send message
    ssize_t sent_bytes = sendto(sock, message, strlen(message), 0, 
                               (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (sent_bytes < 0) {
        printf("Failed to send message. Error: %s\n", strerror(errno));
    } else {
        printf("Sent %zd bytes: %s\n", sent_bytes, message);
        
        // Optional: Wait for acknowledgment from server
        ssize_t received_bytes = recvfrom(sock, recv_buffer, BUFFER_SIZE, 0,
                                        (struct sockaddr*)&server_addr, &addr_len);
        if (received_bytes > 0) {
            recv_buffer[received_bytes] = '\0';
            printf("Received acknowledgment: %s\n", recv_buffer);
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("No response from server (timeout)\n");
        }
    }
    
    close(sock);
}

void get_cpu_usage(double *cpu_usage) {
    FILE *fp;
    char line[256];
    unsigned long long user, nice, system, idle, iowait, irq, softirq;
    static unsigned long long prev_total = 0, prev_idle = 0;
    
    fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("Failed to open /proc/stat");
        return;
    }
    
    fgets(line, sizeof(line), fp);
    fclose(fp);

    sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu", &user, &nice, &system, &idle, &iowait, &irq, &softirq);
    
    unsigned long long total = user + nice + system + idle + iowait + irq + softirq;
    unsigned long long total_diff = total - prev_total;
    unsigned long long idle_diff = idle - prev_idle;
    
    *cpu_usage = (total_diff - idle_diff) * 100.0 / total_diff;

    prev_total = total;
    prev_idle = idle;
}

void get_ram_usage(double *ram_usage) {
    FILE *fp;
    char line[256];
    unsigned long total_mem, free_mem;

    fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("Failed to open /proc/meminfo");
        return;
    }

    fgets(line, sizeof(line), fp);
    sscanf(line, "MemTotal: %lu kB", &total_mem);

    fgets(line, sizeof(line), fp);
    sscanf(line, "MemFree: %lu kB", &free_mem);

    fclose(fp);

    *ram_usage = 100.0 * (1.0 - ((double)free_mem / total_mem));
}

void get_wrong_head_votes(char *status) {
    FILE *fp;
    char buffer[128];
    fp = popen("sudo journalctl -u prysmvalidator --since '1 day ago' | grep -ic correctlyVotedHead=false", "r");
    if (!fp) {
        strcpy(status, "Error");
        return;
    }

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strcpy(status, buffer);
    } else {
        strcpy(status, "Not Syncing");
    }

    pclose(fp);
}

int main() {
    double cpu_usage, ram_usage;
    char message[512];
    char eth_status[128];

    while (1) {
        get_cpu_usage(&cpu_usage);
        get_ram_usage(&ram_usage);
        get_wrong_head_votes(eth_status);

        snprintf(message, sizeof(message), "CPU: %.2f%%, RAM: %.2f%%, wrongHeadVotes: %s", cpu_usage, ram_usage, eth_status);
        send_udp_message(message);

        sleep(5);
    }

    return 0;
}