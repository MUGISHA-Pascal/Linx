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
typedef struct client_node {
    int socket;
    char username[MAX_USERNAME];
    struct client_node *next;
} client_node_t;

client_node_t *clients = NULL;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Emoji mapping
const char *emoji_map[][2] = {
    {":)", "😊"}, {":(", "😞"}, {":D", "😃"}, {";)", "😉"}, {":P", "😛"},
    {":O", "😮"}, {":/", "😕"}, {"<3", "❤️"}, {":heart:", "❤️"}, {":thumbsup:", "👍"},
    {"lol", "😂"}, {"rofl", "🤣"}, {"wink", "😉"}, {"cool", "😎"}, {"cry", "😢"},
    {NULL, NULL}
};

// Function to replace text emojis with Unicode emojis
void replace_emojis(char *message) {
    char temp[BUFFER_SIZE * 2] = {0};
    char *pos = message;
    char *temp_pos = temp;
    int i;

    while (*pos) {
        int replaced = 0;
        for (i = 0; emoji_map[i][0] != NULL; i++) {
            size_t emoji_len = strlen(emoji_map[i][0]);
            if (strncmp(pos, emoji_map[i][0], emoji_len) == 0) {
                size_t emoji_utf8_len = strlen(emoji_map[i][1]);
                strncpy(temp_pos, emoji_map[i][1], emoji_utf8_len);
                temp_pos += emoji_utf8_len;
                pos += emoji_len;
                replaced = 1;
                break;
            }
        }
        if (!replaced) {
            *temp_pos++ = *pos++;
        }
    }
    *temp_pos = '\0';
    strncpy(message, temp, BUFFER_SIZE - 1);
    message[BUFFER_SIZE - 1] = '\0';
}

// Function to broadcast message to all clients except sender
void broadcast_message(const char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    client_node_t *tmp = clients;
    while (tmp != NULL) {
        if (tmp->socket != sender_socket) {
            send(tmp->socket, message, strlen(message), 0);
        }
        tmp = tmp->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Function to send private message to a specific user
void send_private_message(const char *recipient, const char *message, const char *sender) {
    pthread_mutex_lock(&clients_mutex);
    client_node_t *tmp = clients;
    char private_msg[BUFFER_SIZE + MAX_USERNAME + 10];
    
    snprintf(private_msg, sizeof(private_msg), "[PM from %s] %s", sender, message);
    
    while (tmp != NULL) {
        if (strcasecmp(tmp->username, recipient) == 0) {
            send(tmp->socket, private_msg, strlen(private_msg), 0);
            break;
        }
        tmp = tmp->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Function to send list of connected users to a specific client
void send_user_list(int socket) {
    pthread_mutex_lock(&clients_mutex);
    client_node_t *tmp = clients;
    char user_list[BUFFER_SIZE * 2] = {0};
    int count = 0;
    
    strcat(user_list, COLOR_CYAN "=== Connected Users ===" COLOR_RESET "\n");
    
    while (tmp != NULL) {
        strcat(user_list, "- ");
        strcat(user_list, tmp->username);
        strcat(user_list, "\n");
        count++;
        tmp = tmp->next;
    }
    
    char footer[128];
    snprintf(footer, sizeof(footer), COLOR_CYAN "Total: %d user(s)" COLOR_RESET, count);
    strcat(user_list, footer);
    
    send(socket, user_list, strlen(user_list), 0);
    pthread_mutex_unlock(&clients_mutex);
}

// Function to add client to the list
void add_client(client_node_t *client) {
    pthread_mutex_lock(&clients_mutex);
    client->next = clients;
    clients = client;
    pthread_mutex_unlock(&clients_mutex);
}

// Function to remove client from the list
void remove_client(int socket) {
    pthread_mutex_lock(&clients_mutex);
    client_node_t **current = &clients;
    while (*current) {
        client_node_t *entry = *current;
        if (entry->socket == socket) {
            *current = entry->next;
            free(entry);
            break;
        }
        current = &entry->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

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
        client_node_t *client = malloc(sizeof(client_node_t));
        if (!client) {
            log_message(COLOR_RED, "Memory allocation failed");
            close(new_socket);
            continue;
        }
        
        client->socket = new_socket;
        client->next = NULL;
        
        // Start a new thread for the client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) < 0) {
            log_message(COLOR_RED, "Could not create thread");
            free(client);
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
    client_node_t *client = (client_node_t *)client_info_ptr;
    int sock = client->socket;
    char buffer[BUFFER_SIZE];
    ssize_t read_size;
    
    // Get client's username
    if ((read_size = recv(sock, client->username, MAX_USERNAME - 1, 0)) <= 0) {
        log_message(COLOR_RED, "Failed to get username from client");
        goto cleanup;
    }
    client->username[read_size] = '\0';
    
    // Add client to the list
    add_client(client);
    
    // Notify all clients about the new user
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg), "%s%s has joined the chat%s", COLOR_GREEN, client->username, COLOR_RESET);
    broadcast_message(join_msg, sock);
    
    log_message(COLOR_GREEN, "%s has joined the chat", client->username);
    
    // Main message loop
    while ((read_size = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        
        // Check for user list command
        if (strcmp(buffer, "/users") == 0 || strcmp(buffer, "/list") == 0) {
            send_user_list(sock);
        }
        // Check for private message command
        else if (strncmp(buffer, "/msg ", 5) == 0) {
            char *recipient = buffer + 5;
            char *message = strchr(recipient, ' ');
            
            if (message) {
                *message++ = '\0'; // Split recipient and message
                replace_emojis(message);
                send_private_message(recipient, message, client->username);
                
                // Send confirmation to sender
                char confirm_msg[BUFFER_SIZE];
                snprintf(confirm_msg, sizeof(confirm_msg), "[PM to %s] %s", recipient, message);
                send(sock, confirm_msg, strlen(confirm_msg), 0);
            }
        } else {
            // Process emojis in the message
            replace_emojis(buffer);
            
            // Log the received message
            log_message(COLOR_RESET, "%s: %s", client->username, buffer);
            
            // Broadcast to all clients with timestamp
            char broadcast_msg[BUFFER_SIZE + MAX_USERNAME + 32];
            snprintf(broadcast_msg, sizeof(broadcast_msg), "[%s %s] %s", get_current_time(), client->username, buffer);
            broadcast_message(broadcast_msg, sock);
        }
    }
    
    // Client disconnected
    if (read_size == 0) {
        log_message(COLOR_YELLOW, "%s has left the chat", client->username);
        char leave_msg[BUFFER_SIZE];
        snprintf(leave_msg, sizeof(leave_msg), "%s%s has left the chat%s", 
                COLOR_YELLOW, client->username, COLOR_RESET);
        broadcast_message(leave_msg, sock);
    } else if (read_size == -1) {
        log_message(COLOR_RED, "Error receiving data from %s", client->username);
    }
    
cleanup:
    remove_client(sock);
    close(sock);
    free(client);
    return NULL;
}