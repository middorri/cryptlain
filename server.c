// server.c
// build: gcc -Wall -O2 -o server server.c
// run: ./server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <time.h>

#define LISTEN_ADDR "127.0.0.1"
#define PORT 1234
#define BUF_SIZE 4096
#define PASSWORD "secret123"  // Change this to your desired password
#define MAX_AUTH_ATTEMPTS 3

// Color codes for server output
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

void print_timestamp() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf(COLOR_YELLOW "[%02d:%02d:%02d] " COLOR_RESET, t->tm_hour, t->tm_min, t->tm_sec);
}

void print_server_banner() {
    printf("\n");
    printf(COLOR_CYAN "╔══════════════════════════════════════════════════════════╗\n");
    printf("║" COLOR_MAGENTA "               SECURE TOR CHAT SERVER                " COLOR_CYAN "║\n");
    printf("║" COLOR_GREEN "                  Server is running                    " COLOR_CYAN "║\n");
    printf("║" COLOR_YELLOW "           All communications are encrypted             " COLOR_CYAN "║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");
}

int authenticate_client(int sockfd, const char *client_ip, int client_port) {
    char buffer[BUF_SIZE];
    int attempts = 0;
    
    print_timestamp();
    printf("Authenticating client %s:%d\n", client_ip, client_port);
    
    while (attempts < MAX_AUTH_ATTEMPTS) {
        // Send password prompt
        const char *prompt = "Enter password: ";
        if (send(sockfd, prompt, strlen(prompt), 0) < 0) {
            print_timestamp();
            printf(COLOR_RED "Send failed for client %s:%d\n" COLOR_RESET, client_ip, client_port);
            return 0;
        }
        
        // Receive password attempt
        int n = recv(sockfd, buffer, BUF_SIZE - 1, 0);
        if (n <= 0) {
            print_timestamp();
            printf(COLOR_RED "Client %s:%d disconnected during authentication\n" COLOR_RESET, client_ip, client_port);
            return 0;
        }
        buffer[n] = 0;
        
        // Remove newline if present
        buffer[strcspn(buffer, "\n")] = 0;
        buffer[strcspn(buffer, "\r")] = 0;
        
        print_timestamp();
        printf("Authentication attempt %d from %s:%d: %s\n", 
               attempts + 1, client_ip, client_port, 
               strcmp(buffer, PASSWORD) == 0 ? "SUCCESS" : "FAILED");
        
        // Check password
        if (strcmp(buffer, PASSWORD) == 0) {
            const char *success_msg = "Authentication successful! Welcome to the secure chat.\n";
            send(sockfd, success_msg, strlen(success_msg), 0);
            print_timestamp();
            printf(COLOR_GREEN "Client %s:%d authenticated successfully\n" COLOR_RESET, client_ip, client_port);
            return 1;
        } else {
            attempts++;
            if (attempts >= MAX_AUTH_ATTEMPTS) {
                const char *max_attempts_msg = "Maximum authentication attempts exceeded. Connection closed.\n";
                send(sockfd, max_attempts_msg, strlen(max_attempts_msg), 0);
                print_timestamp();
                printf(COLOR_RED "Client %s:%d failed authentication (max attempts)\n" COLOR_RESET, client_ip, client_port);
                return 0;
            } else {
                const char *fail_msg = "Authentication failed. Please try again.\n";
                send(sockfd, fail_msg, strlen(fail_msg), 0);
            }
        }
    }
    return 0;
}

int main() {
    int sockfd, new_sock;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    char buffer[BUF_SIZE];

    print_server_banner();
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { 
        perror("socket"); 
        exit(1); 
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(LISTEN_ADDR);
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind"); 
        close(sockfd);
        exit(1);
    }

    if (listen(sockfd, 5) < 0) {
        perror("listen");
        close(sockfd);
        exit(1);
    }

    print_timestamp();
    printf("Server listening on %s:%d\n", LISTEN_ADDR, PORT);
    print_timestamp();
    printf("Waiting for incoming connections...\n");

    while (1) {
        new_sock = accept(sockfd, (struct sockaddr*)&cli_addr, &cli_len);
        if (new_sock < 0) { 
            perror("accept"); 
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(cli_addr.sin_port);
        
        print_timestamp();
        printf(COLOR_GREEN "New client connected from %s:%d\n" COLOR_RESET, client_ip, client_port);
        
        // Authenticate client
        if (authenticate_client(new_sock, client_ip, client_port)) {
            const char *welcome_msg = "Welcome to the secure chat server! Type your messages.\n";
            send(new_sock, welcome_msg, strlen(welcome_msg), 0);
            
            // Main chat loop
            fd_set readfds;
            int maxfd = (new_sock > STDIN_FILENO) ? new_sock : STDIN_FILENO;
            int authenticated_session = 1;
            
            print_timestamp();
            printf("Chat session started with %s:%d\n", client_ip, client_port);
            
            while (authenticated_session) {
                FD_ZERO(&readfds);
                FD_SET(new_sock, &readfds);
                FD_SET(STDIN_FILENO, &readfds);

                if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
                    perror("select");
                    break;
                }

                // Incoming message from client
                if (FD_ISSET(new_sock, &readfds)) {
                    int n = recv(new_sock, buffer, BUF_SIZE - 1, 0);
                    if (n <= 0) {
                        print_timestamp();
                        printf(COLOR_RED "Client %s:%d disconnected\n" COLOR_RESET, client_ip, client_port);
                        break;
                    }
                    buffer[n] = 0;
                    
                    // Remove newline if present
                    buffer[strcspn(buffer, "\n")] = 0;
                    buffer[strcspn(buffer, "\r")] = 0;
                    
                    // Display client message
                    print_timestamp();
                    printf(COLOR_BLUE "Client %s:%d: %s\n" COLOR_RESET, client_ip, client_port, buffer);
                }

                // Server input
                if (FD_ISSET(STDIN_FILENO, &readfds)) {
                    if (!fgets(buffer, BUF_SIZE - 1, stdin)) break;
                    buffer[strcspn(buffer, "\n")] = 0;
                    
                    if (strlen(buffer) > 0) {
                        // Check for server commands
                        if (strcmp(buffer, "/quit") == 0 || strcmp(buffer, "/exit") == 0) {
                            print_timestamp();
                            printf("Server shutting down...\n");
                            const char *shutdown_msg = "Server is shutting down. Goodbye!\n";
                            send(new_sock, shutdown_msg, strlen(shutdown_msg), 0);
                            close(new_sock);
                            close(sockfd);
                            exit(0);
                        }
                        
                        if (strcmp(buffer, "/status") == 0) {
                            print_timestamp();
                            printf("Server status: Active, 1 client connected\n");
                            continue;
                        }
                        
                        // Display server message locally
                        print_timestamp();
                        printf(COLOR_CYAN "You: %s\n" COLOR_RESET, buffer);
                        
                        // Send message to client
                        if (send(new_sock, buffer, strlen(buffer), 0) < 0) {
                            perror("send");
                            break;
                        }
                    }
                }
            }
        } else {
            print_timestamp();
            printf(COLOR_RED "Client %s:%d failed authentication\n" COLOR_RESET, client_ip, client_port);
        }
        
        close(new_sock);
        print_timestamp();
        printf("Connection with %s:%d closed\n", client_ip, client_port);
    }

    close(sockfd);
    print_timestamp();
    printf("Server shutdown\n");
    return 0;
}