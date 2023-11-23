#include "common.h"
#define REQUEST_BUFFER_SIZE 1024
#define RESPONSE_BUFFER_SIZE 1024
#define DATA_BUFFER_SIZE 4096

void handle_put(int sockfd, Request *request, const char *local_file);
//void handle_get(int sockfd, const char *remote_filename, const char *local_filename);
void handle_get(int sockfd, Request *request, const char *remote_filename, const char *local_filename) ;
void handle_delete(int sockfd, Request *request);
//void handle_list(int sockfd);
void handle_list(int sockfd, Request *request) ;
void send_request(int sockfd, Request *request);
void recv_response(int sockfd, Response *response);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server IP>:<server port> VERB [remote] [local]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char server[256];
    int port;
    sscanf(argv[1], "%[^:]:%d", server, &port);

    int sockfd = create_client_socket(server, port);
    printf("Client socket created\n"); // Add this line

    const char *remote_filename = argv[3];
    char request_buffer[REQUEST_BUFFER_SIZE];
    Request request;
    create_request(&request, argc, argv);
    serialize_request(&request, request_buffer);
    char raw_request[MAX_HEADER_LENGTH];
    serialize_request(&request, raw_request);

    ssize_t sent_bytes = send(sockfd, raw_request, strlen(raw_request), 0);
    if (sent_bytes < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }
   // if (strcmp(request.verb, "PUT") == 0) {
   //     printf("Handling PUT\n"); // Add this line
   //     char newline = '\n';
   //     send(sockfd, &newline, 1, 0);
   // }

    if (strcmp(request.verb, "PUT") == 0) {
        handle_put(sockfd, &request, argv[3]);

    } else if (strcmp(request.verb, "GET") == 0) {
        printf("Handling GET\n"); // Add this line
        //handle_get(sockfd, remote_filename, argv[4]);
        handle_get(sockfd, &request, remote_filename, argv[4]);
    } else if (strcmp(request.verb, "DELETE") == 0) {
        handle_delete(sockfd, &request);
    } else if (strcmp(request.verb, "LIST") == 0) {
        //handle_list(sockfd);
        handle_list(sockfd, &request);
    } else {
        fprintf(stderr, "Unknown command: %s\n", request.verb);
        fprintf(stderr, "Usage: %s <server IP>:<server port> VERB [remote] [local]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    close(sockfd);
    return 0;
}

// ... (rest of the functions remain unchanged)


void handle_put(int sockfd, Request *request, const char *local_file) {
    FILE *file = fopen(local_file, "rb");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char buffer[4096];
    size_t bytes_read;
    size_t total_sent_bytes = 0; // Add this line
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        ssize_t sent_bytes = send(sockfd, buffer, bytes_read, 0);
        if (sent_bytes < 0) {
            perror("send");
            fclose(file);
            exit(EXIT_FAILURE);
        }
        total_sent_bytes += sent_bytes; // Add this line
    }

    fclose(file);

    char raw_response[MAX_HEADER_LENGTH];
    ssize_t received_bytes = recv(sockfd, raw_response, sizeof(raw_response) - 1, 0);
    if (received_bytes < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    raw_response[received_bytes] = '\0';

    Response response;
    parse_response(&response, raw_response);

    if (response.error) {
        fprintf(stderr, "Error: %s\n", response.error_message);
        exit(EXIT_FAILURE);
    }

    printf("File uploaded successfully\n");
}

// handle get

//void handle_get(int sockfd, const char *remote_filename, const char *local_filename) {
void handle_get(int sockfd, Request *request, const char *remote_filename, const char *local_filename) {
    //send_request(sockfd, &request);
    send_request(sockfd, request);

    char raw_response[MAX_HEADER_LENGTH];
    ssize_t received_bytes = recv(sockfd, raw_response, sizeof(raw_response) - 1, 0);
    if (received_bytes < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    raw_response[received_bytes] = '\0';

    Response response;
    parse_response(&response, raw_response);
    printf("Client: File size: %zu\n", response.file_size); // diagnostic

    if (response.error) {
        fprintf(stderr, "Error: %s\n", response.error_message);
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen(local_filename, "wb");
    if (file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    size_t total_received = 0;
    char buffer[DATA_BUFFER_SIZE];
    while (total_received < response.file_size) {
        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes_received < 0) {
            perror("recv");
            fclose(file);
            exit(EXIT_FAILURE);
        }

        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written != bytes_received) {
            perror("fwrite");
            fclose(file);
            exit(EXIT_FAILURE);
        }

        total_received += bytes_written;
    }

    fclose(file);
    printf("File downloaded successfully\n");
}
// end get

// end get

void handle_delete(int sockfd, Request *request) {
    send_request(sockfd, request);
    char raw_response[MAX_HEADER_LENGTH];
    ssize_t received_bytes = recv(sockfd, raw_response, sizeof(raw_response) - 1, 0);
    if (received_bytes < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    raw_response[received_bytes] = '\0';

    Response response;
    parse_response(&response, raw_response);

    if (response.error) {
        fprintf(stderr, "Error: %s\n", response.error_message);
        exit(EXIT_FAILURE);
    }

    printf("File deleted successfully\n");
}

// list
//void handle_list(int sockfd) {
void handle_list(int sockfd, Request *request) {

    send_request(sockfd, request);
    Response response;
    recv_response(sockfd, &response);

    if (response.error) {
        fprintf(stderr, "Error: %s\n", response.error_message);
    } else {
        size_t remaining = response.file_size;
        char buffer[4096];

        while (remaining > 0) {
            ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received < 0) {
                perror("recv");
                break;
            }
            if (bytes_received == 0) {
                break;
            }
            buffer[bytes_received] = '\0';
            printf("%s", buffer); // This line prints the received file names
            remaining -= bytes_received;
        }
    }
}
//

int create_client_socket(const char *server, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}
void send_request(int sockfd, Request *request) {
    char raw_request[MAX_HEADER_LENGTH];
    serialize_request(request, raw_request);
    ssize_t sent_bytes = send(sockfd, raw_request, strlen(raw_request), 0);
    if (sent_bytes < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }char newline = '\n'; // Add this line
    send(sockfd, &newline, 1, 0); // Add this line
}
void recv_response(int sockfd, Response *response) {
    char raw_response[MAX_HEADER_LENGTH];
    recv(sockfd, raw_response, sizeof(raw_response), 0);
    parse_response(response, raw_response);

    if (response->error == 0) { // Add this block
        char newline;
        recv(sockfd, &newline, 1, 0);
    }
}
