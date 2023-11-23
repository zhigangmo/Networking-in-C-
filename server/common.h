
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif __APPLE__
#include <sys/event.h>
#include <sys/time.h>
#endif

#define MAX_FILENAME_LENGTH 255
#define MAX_HEADER_LENGTH 1024

#define MAXEVENTS 64
#define BUFFERSIZE 1024
#define MEMORYLIMIT (10 * 1024 * 1024) // 10 MB
// Error handling
void error_handling(const char *msg);

// Set non-blocking socket
int set_non_blocking(int fd);

// Signal handling
void signal_handler(int sig);

// Structures
typedef struct {
    char verb[8];
    char filename[MAX_FILENAME_LENGTH + 1];
    size_t file_size;
} Request;

typedef struct {
    int error;
    char error_message[MAX_HEADER_LENGTH + 1];
    size_t file_size;
} Response;

// Function declarations
int create_server_socket(int port);
int connect_to_server(const char* server, int port);
void make_socket_non_blocking(int sockfd);
void create_request(Request* request, int argc, char* argv[]);
void parse_request(Request* request, const char* raw_request);
void serialize_request(const Request* request, char* raw_request);
void parse_response(Response* response, const char* raw_response);
void serialize_response(const Response* response, char* raw_response);

#endif // COMMON_H
