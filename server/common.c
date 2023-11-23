
#include "common.h"

void error_handling(const char *msg) {
    perror(msg);
    exit(1);
}

int set_non_blocking(int fd) {
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        error_handling("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        error_handling("fcntl");
        return -1;
    }

    return 0;
}

void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            printf("Graceful shutdown initiated.\n");
            // Perform cleanup tasks here
            exit(0);
            break;
        default:
            break;
    }
}

int create_server_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, SOMAXCONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

int connect_to_server(const char* server, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "inet_pton error for %s: %s\n", server, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

void make_socket_non_blocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }
}


void create_request(Request* request, int argc __attribute__((unused)), char* argv[]) {
    strncpy(request->verb, argv[2], sizeof(request->verb) - 1);
    if (strcmp(request->verb, "PUT") ==  0) {
        strncpy(request->filename, argv[4], MAX_FILENAME_LENGTH);
        request->filename[MAX_FILENAME_LENGTH] = '\0';
        FILE *file = fopen(argv[3], "rb");
        if (file == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        fseek(file, 0, SEEK_END);
        request->file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        fclose(file);
    } else if (strcmp(request->verb, "GET") == 0 || strcmp(request->verb, "DELETE") == 0) {
        strncpy(request->filename, argv[3], MAX_FILENAME_LENGTH);
        request->filename[MAX_FILENAME_LENGTH] = '\0';
    }
}

void parse_request(Request* request, const char* raw_request) {
            sscanf(raw_request, "%s %s", request->verb, request->filename);
            if (strcmp(request->verb, "PUT") == 0) {
            memcpy(&request->file_size, raw_request + strlen(request->verb) + 1 + strlen(request->filename) + 1, sizeof(size_t));
            }
            }

void serialize_request(const Request* request, char* raw_request) {
    sprintf(raw_request, "%s %s\n", request->verb, request->filename);
    if (strcmp(request->verb, "PUT") == 0) {
        memcpy(raw_request + strlen(request->verb) + 1 + strlen(request->filename) + 1, &request->file_size, sizeof(size_t));
    }
}

void parse_response(Response* response, const char* raw_response) {
    char response_code[8];
    sscanf(raw_response, "%s", response_code);
    response->error = (strcmp(response_code, "ERROR") == 0);
    if (response->error) {
        strcpy(response->error_message, raw_response + strlen(response_code) + 1);
    } else {
        sscanf(raw_response + strlen(response_code) + 1, "%zu", &response->file_size);
    }
}
void serialize_response(const Response* response, char* raw_response) {
    const char* response_code = response->error ? "ERROR" : "OK";
    sprintf(raw_response, "%s", response_code);
    if (response->error) {
        sprintf(raw_response + strlen(response_code), "\n%s", response->error_message);
    } else {
        memcpy(raw_response + strlen(response_code) + 1, &response->file_size, sizeof(size_t));
    }
}
