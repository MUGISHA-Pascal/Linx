#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>
#include <ifaddrs.h>
#include <netdb.h>

// ANSI color codes
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_USERNAME 32

// Structure to pass multiple arguments to the thread
typedef struct {
    int socket;
    char username[MAX_USERNAME];
} client_info_t;

// Function declarations
void *handle_client(void *client_info);
void log_message(const char *color, const char *format, ...);
char* get_current_time();

// Get current timestamp
char* get_current_time() {
    static char buffer[64];
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return buffer;
}

// Print colored log messages
void log_message(const char *color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    printf("%s[%s]%s ", COLOR_CYAN, get_current_time(), COLOR_RESET);
    printf("%s", color);
    vprintf(format, args);
    printf("%s\n", COLOR_RESET);
    
    va_end(args);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t thread_id;

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_message(COLOR_RED, "Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        log_message(COLOR_RED, "setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_message(COLOR_RED, "Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        log_message(COLOR_RED, "Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Get and display server IP addresses
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];
    
    log_message(COLOR_GREEN, "Server started and listening on port %d", PORT);
    log_message(COLOR_GREEN, "Available network interfaces:");
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
    } else {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            
            int family = ifa->ifa_addr->sa_family;
            if (family == AF_INET) {  // IPv4
                int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                                  host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                if (s == 0 && strcmp(host, "127.0.0.1") != 0) {
                    log_message(COLOR_GREEN, "- %s: %s", ifa->ifa_name, host);
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    while (1) {
        // Accept new connection
        new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (new_socket < 0) {
            log_message(COLOR_RED, "Accept failed");
            continue;
        }

        // Allocate memory for client info
        client_info_t *client_info = malloc(sizeof(client_info_t));
        if (!client_info) {
            log_message(COLOR_RED, "Memory allocation failed");
            close(new_socket);
            continue;
        }
        
        client_info->socket = new_socket;
        
        // Start a new thread for the client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client_info) < 0) {
            log_message(COLOR_RED, "Could not create thread");
            free(client_info);
            close(new_socket);
        } else {
            pthread_detach(thread_id); // Detach the thread
            log_message(COLOR_YELLOW, "New client connected (Socket: %d)", new_socket);
        }
    }

    close(server_fd);
    return 0;
}

void *handle_client(void *client_info_ptr) {
    client_info_t *client_info = (client_info_t *)client_info_ptr;
    int sock = client_info->socket;
    char buffer[BUFFER_SIZE];
    ssize_t read_size;
    
    // Get client's username
    if ((read_size = recv(sock, client_info->username, MAX_USERNAME - 1, 0)) <= 0) {
        log_message(COLOR_RED, "Failed to get username from client");
        goto cleanup;
    }
    client_info->username[read_size] = '\0';
    
    log_message(COLOR_GREEN, "%s has joined the chat", client_info->username);
    
    // Main message loop
    while ((read_size = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        
        // Log the received message
        log_message(COLOR_RESET, "%s: %s", client_info->username, buffer);
        
        // Send acknowledgment back to client
        char ack_msg[BUFFER_SIZE + MAX_USERNAME + 10];
        snprintf(ack_msg, sizeof(ack_msg), "[%s] %s", client_info->username, buffer);
        if (send(sock, ack_msg, strlen(ack_msg), 0) < 0) {
            log_message(COLOR_RED, "Failed to send message to client");
            break;
        }
    }
    
    // Client disconnected
    if (read_size == 0) {
        log_message(COLOR_YELLOW, "%s has left the chat", client_info->username);
    } else if (read_size == -1) {
        log_message(COLOR_RED, "Error receiving data from %s", client_info->username);
    }
    
cleanup:
    close(sock);
    free(client_info);
    return NULL;
}