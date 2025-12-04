#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <termios.h>
#include <pthread.h>

// ANSI color codes
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_USERNAME 32

// Global variables
volatile bool running = true;
int sock = 0;
char username[MAX_USERNAME];

// Function declarations
void *receive_handler(void *arg);
void set_terminal_mode(int enable);
void print_help();
void replace_emojis(char *message);

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
            if (strncasecmp(pos, emoji_map[i][0], emoji_len) == 0) {
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

// Set terminal to raw mode or back to normal
void set_terminal_mode(int enable) {
    static struct termios oldt, newt;
    
    if (enable) {
        // Save old terminal settings
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        
        // Set terminal to raw mode
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        // Restore old terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

// Print help message
void print_help() {
    printf("\n" COLOR_YELLOW "=== Linx Chat Commands ===" COLOR_RESET "\n");
    printf("/help - Show this help message\n");
    printf("/exit - Exit the chat\n");
    printf("/clear - Clear the screen\n");
    printf("/users - Show connected users\n");
    printf("/msg <username> <message> - Send private message\n");
    printf("\n" COLOR_YELLOW "=== Emoji Shortcuts ===" COLOR_RESET "\n");
    printf(":) 😊  :( 😞  :D 😃  ;) 😉  :P 😛\n");
    printf(":O 😮  :/ 😕  <3 ❤️  lol 😂  rofl 🤣\n");
    printf("wink 😉  cool 😎  cry 😢  :thumbsup: 👍\n");
    printf("\n");
}

// Thread function to handle incoming messages
void *receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    ssize_t read_size;
    
    while ((read_size = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("\r\33[2K"); // Clear the current line
        printf(COLOR_BLUE "%s" COLOR_RESET "\n", buffer);
        printf("%s: ", username);
        fflush(stdout);
    }
    
    if (read_size == 0) {
        printf("\r\33[2K" COLOR_RED "Server disconnected" COLOR_RESET "\n");
    } else {
        perror("\r\33[2K" COLOR_RED "Receive error" COLOR_RESET);
    }
    
    running = false;
    return NULL;
}

int main(int argc, char *argv[]) {
    char server_ip[16] = "127.0.0.1";  // Default to localhost
    
    // Check for command-line arguments
    if (argc > 1) {
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
        server_ip[sizeof(server_ip) - 1] = '\0';
    }
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    pthread_t recv_thread;
    
    // Get username
    printf("Enter your username: ");
    fflush(stdout);
    
    if (fgets(username, MAX_USERNAME, stdin) == NULL) {
        perror(COLOR_RED "Failed to read username" COLOR_RESET);
        return EXIT_FAILURE;
    }
    username[strcspn(username, "\n")] = '\0';
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror(COLOR_RED "Socket creation error" COLOR_RESET);
        return EXIT_FAILURE;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, COLOR_RED "Invalid address / Address not supported" COLOR_RESET "\n");
        close(sock);
        return EXIT_FAILURE;
    }

    // Connect to server
    printf("Connecting to %s...\n", server_ip);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror(COLOR_RED "Connection failed" COLOR_RESET);
        close(sock);
        return EXIT_FAILURE;
    }

    // Send username to server
    if (send(sock, username, strlen(username), 0) < 0) {
        perror(COLOR_RED "Failed to send username" COLOR_RESET);
        close(sock);
        return EXIT_FAILURE;
    }

    // Start receive thread
    if (pthread_create(&recv_thread, NULL, receive_handler, NULL) != 0) {
        perror(COLOR_RED "Failed to create receive thread" COLOR_RESET);
        close(sock);
        return EXIT_FAILURE;
    }
    pthread_detach(recv_thread);

    // Set terminal to non-canonical mode
    set_terminal_mode(1);
    
    // Clear screen and show welcome message
    printf("\033[H\033[J"); // Clear screen
    printf(COLOR_GREEN "=== Welcome to Linx Chat ===" COLOR_RESET "\n");
    printf("Type /help for a list of commands\n\n");
    
    // Main input loop
    while (running) {
        printf("%s: ", username);
        fflush(stdout);
        
        // Read input
        int pos = 0;
        int ch;
        
        while ((ch = getchar()) != '\n' && ch != EOF && pos < BUFFER_SIZE - 1) {
            if (ch == 127) { // Backspace
                if (pos > 0) {
                    pos--;
                    printf("\b \b");
                }
            } else {
                message[pos++] = (char)ch;
                putchar(ch);
            }
            fflush(stdout);
        }
        
        if (ch == EOF || !running) break;
        
        message[pos] = '\0';
        
        // Handle commands
        if (message[0] == '/') {
            if (strcmp(message, "/exit") == 0) {
                printf("\nDisconnecting...\n");
                break;
            } else if (strcmp(message, "/help") == 0) {
                print_help();
                continue;
            } else if (strcmp(message, "/clear") == 0) {
                printf("\033[H\033[J");
                continue;
            } else if (strncmp(message, "/nick ", 6) == 0) {
                // Handle nickname change
                strncpy(username, message + 6, MAX_USERNAME - 1);
                username[MAX_USERNAME - 1] = '\0';
                printf("\r\33[2K" COLOR_GREEN "Changed username to: %s" COLOR_RESET "\n", username);
                continue;
            } else if (strncmp(message, "/msg ", 5) == 0) {
                // Process private message (server will handle the rest)
                replace_emojis(message);
            } else if (strcmp(message, "/users") == 0) {
                printf("\r\33[2K" COLOR_YELLOW "User list is shown when users join/leave" COLOR_RESET "\n");
                continue;
            } else {
                printf("\r\33[2K" COLOR_YELLOW "Unknown command. Type /help for a list of commands." COLOR_RESET "\n");
                continue;
            }
        } else {
            // Process emojis in regular messages
            replace_emojis(message);
        }
        
        // Send message to server
        if (strlen(message) > 0) {
            if (send(sock, message, strlen(message), 0) < 0) {
                perror(COLOR_RED "\r\33[2KSend failed" COLOR_RESET);
                break;
            }
        }
    }
    
    // Cleanup
    running = false;
    set_terminal_mode(0);
    close(sock);
    printf(COLOR_GREEN "\nGoodbye!\n" COLOR_RESET);
    return 0;
}
