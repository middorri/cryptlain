// client.c
// build: gcc -Wall -O2 -o client client.c
// run: ./client <onion-hostname> <port>
// example: ./client abcdefghijklmnopqrstuvwxyz.onion 12345

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PROXY_HOST "127.0.0.1"
#define PROXY_PORT 9050
#define BUF_SIZE 4096

// Color codes for better UI
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

static int connect_tcp(const char *host, int port) {
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &sa.sin_addr) <= 0) {
        fprintf(stderr, COLOR_RED "Error: Invalid proxy address\n" COLOR_RESET);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    
    printf(COLOR_YELLOW "Connecting to Tor proxy at %s:%d..." COLOR_RESET "\n", host, port);
    if (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    
    printf(COLOR_GREEN "✓ Connected to Tor proxy\n" COLOR_RESET);
    return fd;
}

static int socks5_connect_domain(int proxy_fd, const char *domain, int port) {
    unsigned char buf[512];
    
    printf(COLOR_YELLOW "Establishing SOCKS5 connection to %s:%d..." COLOR_RESET "\n", domain, port);
    
    // 1) greeting: no auth
    buf[0] = 0x05; buf[1] = 0x01; buf[2] = 0x00;
    if (send(proxy_fd, buf, 3, 0) != 3) {
        fprintf(stderr, COLOR_RED "SOCKS5: Failed to send greeting\n" COLOR_RESET);
        return -1;
    }
    
    if (recv(proxy_fd, buf, 2, 0) != 2) {
        fprintf(stderr, COLOR_RED "SOCKS5: Failed to receive greeting response\n" COLOR_RESET);
        return -1;
    }
    
    if (buf[0] != 0x05 || buf[1] != 0x00) {
        fprintf(stderr, COLOR_RED "SOCKS5: Authentication not supported by proxy\n" COLOR_RESET);
        return -1;
    }

    // 2) CONNECT request with domain name
    size_t dlen = strlen(domain);
    if (dlen > 255) {
        fprintf(stderr, COLOR_RED "SOCKS5: Domain name too long\n" COLOR_RESET);
        return -1;
    }
    
    size_t off = 0;
    buf[off++] = 0x05; // version
    buf[off++] = 0x01; // CONNECT
    buf[off++] = 0x00; // RSV
    buf[off++] = 0x03; // ATYP = DOMAINNAME
    buf[off++] = (unsigned char)dlen;
    memcpy(buf + off, domain, dlen); off += dlen;
    buf[off++] = (unsigned char)((port >> 8) & 0xff);
    buf[off++] = (unsigned char)(port & 0xff);

    if (send(proxy_fd, buf, off, 0) != (ssize_t)off) {
        fprintf(stderr, COLOR_RED "SOCKS5: Failed to send connection request\n" COLOR_RESET);
        return -1;
    }

    // 3) read reply
    ssize_t r = recv(proxy_fd, buf, 4, 0);
    if (r != 4) {
        fprintf(stderr, COLOR_RED "SOCKS5: Failed to receive connection response\n" COLOR_RESET);
        return -1;
    }
    
    if (buf[0] != 0x05) {
        fprintf(stderr, COLOR_RED "SOCKS5: Invalid version in response\n" COLOR_RESET);
        return -1;
    }
    
    if (buf[1] != 0x00) {
        const char *error_msg = "Unknown error";
        switch(buf[1]) {
            case 0x01: error_msg = "General failure"; break;
            case 0x02: error_msg = "Connection not allowed"; break;
            case 0x03: error_msg = "Network unreachable"; break;
            case 0x04: error_msg = "Host unreachable"; break;
            case 0x05: error_msg = "Connection refused"; break;
            case 0x06: error_msg = "TTL expired"; break;
            case 0x07: error_msg = "Command not supported"; break;
            case 0x08: error_msg = "Address type not supported"; break;
        }
        fprintf(stderr, COLOR_RED "SOCKS5: Connection failed - %s\n" COLOR_RESET, error_msg);
        return -1;
    }

    // Read the rest of the response based on address type
    unsigned char atyp = buf[3];
    if (atyp == 0x01) { // IPv4
        if (recv(proxy_fd, buf, 4 + 2, 0) != 6) return -1;
    } else if (atyp == 0x03) { // domain
        if (recv(proxy_fd, buf, 1, 0) != 1) return -1;
        int len = buf[0];
        if (recv(proxy_fd, buf, len + 2, 0) != len + 2) return -1;
    } else if (atyp == 0x04) { // IPv6
        if (recv(proxy_fd, buf, 16 + 2, 0) != 18) return -1;
    } else {
        fprintf(stderr, COLOR_RED "SOCKS5: Unsupported address type\n" COLOR_RESET);
        return -1;
    }

    printf(COLOR_GREEN "✓ SOCKS5 connection established\n" COLOR_RESET);
    return 0;
}

void print_banner() {
    printf("\n");
    printf(COLOR_CYAN "╔══════════════════════════════════════════════════════════╗\n");
    printf("║" COLOR_MAGENTA "               SECURE TOR CHAT CLIENT                " COLOR_CYAN "║\n");
    printf("║" COLOR_GREEN "                 Connection Established                  " COLOR_CYAN "║\n");
    printf("║" COLOR_YELLOW "           All communications are encrypted             " COLOR_CYAN "║\n");
    printf("║" COLOR_BLUE "      Commands: /quit (exit), /help (show commands)     " COLOR_CYAN "║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");
}

void print_help() {
    printf(COLOR_YELLOW "\nAvailable commands:\n" COLOR_RESET);
    printf("  " COLOR_CYAN "/quit" COLOR_RESET " - Exit the chat\n");
    printf("  " COLOR_CYAN "/help" COLOR_RESET " - Show this help message\n");
    printf("  " COLOR_CYAN "/clear" COLOR_RESET " - Clear the screen\n");
    printf("  Just type and press Enter to send a message\n\n");
}

int authenticate_with_server(int sockfd) {
    char buffer[BUF_SIZE];
    int auth_complete = 0;
    int attempts = 0;
    const int max_attempts = 3;
    
    printf(COLOR_YELLOW "Starting authentication..." COLOR_RESET "\n");
    
    while (!auth_complete && attempts < max_attempts) {
        // Wait for server prompt
        int n = recv(sockfd, buffer, BUF_SIZE - 1, 0);
        if (n <= 0) {
            printf(COLOR_RED "Server disconnected during authentication\n" COLOR_RESET);
            return 0;
        }
        buffer[n] = 0;
        
        // Remove any trailing newlines for display
        char *clean_msg = buffer;
        while (*clean_msg && (clean_msg[strlen(clean_msg)-1] == '\n' || clean_msg[strlen(clean_msg)-1] == '\r')) {
            clean_msg[strlen(clean_msg)-1] = 0;
        }
        
        printf(COLOR_BLUE "Server: %s" COLOR_RESET "\n", clean_msg);
        
        // Check if authentication was successful
        if (strstr(buffer, "Authentication successful") != NULL) {
            return 1;
        }
        
        // Check if authentication failed completely
        if (strstr(buffer, "Maximum authentication attempts") != NULL) {
            return 0;
        }
        
        // If we see password prompt, send the password
        if (strstr(buffer, "password") != NULL || strstr(buffer, "Password") != NULL) {
            attempts++;
            printf(COLOR_YELLOW "Enter password (attempt %d/%d): " COLOR_RESET, attempts, max_attempts);
            fflush(stdout);
            
            // Read password securely (no echo)
            if (!fgets(buffer, BUF_SIZE - 1, stdin)) {
                return 0;
            }
            // Remove newline
            buffer[strcspn(buffer, "\n")] = 0;
            
            // Send password to server
            if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
                perror("send");
                return 0;
            }
            printf("\n");
        }
    }
    
    if (attempts >= max_attempts) {
        printf(COLOR_RED "Too many authentication attempts\n" COLOR_RESET);
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) { 
        printf(COLOR_RED "Usage: %s <onion-hostname> <port>\n" COLOR_RESET, argv[0]); 
        printf(COLOR_YELLOW "Example: %s abcdefghijklmnop.onion 12345\n" COLOR_RESET, argv[0]);
        return 1; 
    }

    char *hostname = argv[1];
    int port = atoi(argv[2]);

    if (port <= 0 || port > 65535) {
        fprintf(stderr, COLOR_RED "Error: Invalid port number\n" COLOR_RESET);
        return 1;
    }

    printf(COLOR_CYAN "Tor Chat Client - Connecting to %s:%d\n" COLOR_RESET, hostname, port);
    printf(COLOR_YELLOW "Make sure Tor is running on %s:%d\n" COLOR_RESET, PROXY_HOST, PROXY_PORT);

    int sockfd;
    char buffer[BUF_SIZE];

    // Connect to Tor proxy
    sockfd = connect_tcp(PROXY_HOST, PROXY_PORT);
    if (sockfd < 0) { 
        fprintf(stderr, COLOR_RED "Failed to connect to Tor proxy\n" COLOR_RESET);
        exit(1); 
    }

    if (socks5_connect_domain(sockfd, hostname, port) < 0) {
        fprintf(stderr, COLOR_RED "SOCKS5 handshake failed\n" COLOR_RESET);
        close(sockfd);
        exit(1);
    }

    printf(COLOR_GREEN "✓ Connected to %s:%d via Tor\n" COLOR_RESET, hostname, port);

    // Authenticate with server
    if (!authenticate_with_server(sockfd)) {
        fprintf(stderr, COLOR_RED "Authentication failed\n" COLOR_RESET);
        close(sockfd);
        exit(1);
    }

    // Print banner after successful authentication
    print_banner();
    print_help();

    // Main chat loop
    fd_set readfds;
    int maxfd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

    printf(COLOR_GREEN "You are now connected! Start chatting...\n\n" COLOR_RESET);

    while (1) {
        printf(COLOR_CYAN "You: " COLOR_RESET);
        fflush(stdout);

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // Check for incoming data from server
        if (FD_ISSET(sockfd, &readfds)) {
            int n = recv(sockfd, buffer, BUF_SIZE - 1, 0);
            if (n <= 0) {
                printf(COLOR_RED "\nServer disconnected\n" COLOR_RESET);
                break;
            }
            buffer[n] = 0;
            
            // Clean the message for display
            char *msg = buffer;
            while (*msg && (msg[strlen(msg)-1] == '\n' || msg[strlen(msg)-1] == '\r')) {
                msg[strlen(msg)-1] = 0;
            }
            
            // Clear the current "You: " line and show server message
            printf("\r\033[K"); // Move to beginning of line and clear
            printf(COLOR_BLUE "Server: %s\n" COLOR_RESET, msg);
        }

        // Check for user input
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (!fgets(buffer, BUF_SIZE - 1, stdin)) break;
            buffer[strcspn(buffer, "\n")] = 0;
            
            if (strlen(buffer) > 0) {
                // Handle commands
                if (strcmp(buffer, "/quit") == 0 || strcmp(buffer, "/exit") == 0) {
                    printf(COLOR_YELLOW "Disconnecting...\n" COLOR_RESET);
                    break;
                } else if (strcmp(buffer, "/help") == 0) {
                    print_help();
                    continue;
                } else if (strcmp(buffer, "/clear") == 0) {
                    printf("\033[2J\033[H"); // Clear screen
                    print_banner();
                    continue;
                }
                
                // Clear the "You: " line and show what we sent
                printf("\r\033[K"); // Move to beginning of line and clear
                printf(COLOR_CYAN "You: %s\n" COLOR_RESET, buffer);
                
                // Send message to server
                if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
                    perror("send");
                    break;
                }
            }
        }
    }

    close(sockfd);
    printf(COLOR_GREEN "Connection closed.\n" COLOR_RESET);
    return 0;
}