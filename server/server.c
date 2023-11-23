
#include "common.h"
#include <dirent.h>
#include <signal.h>
void send_response(int sockfd, Response *response);

void handle_put(int sockfd, Request *request);
void handle_get(int sockfd, Request *request);
void handle_delete(int sockfd, Request *request);
//void handle_list(int sockfd, Request *request);
void handle_list(int sockfd, Request *request __attribute__((unused)));

int sockfd_global; // Add this line to declare sockfd as a global variable
//void sigint_handler(int sig) {
void sigint_handler(int sig __attribute__((unused))) {
    // Close the connection and clean up resources
    close(sockfd_global); // Make sure sockfd is accessible in this scope
    //exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int server_sockfd = create_server_socket(port);
    int optval = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    sa.sa_handler = sigint_handler; // Change this line to use sigint_handler
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        error_handling("sigaction");
    return 1;
    }
        //signal(SIGINT, sigint_handler);
    while (1) {
        int client_sockfd = accept(server_sockfd, NULL, NULL);
        if (client_sockfd < 0) {
            perror("accept");
            continue;
        }

        sockfd_global = client_sockfd; // Add this line to set the global sockfd value
                                       //
        char raw_request[MAX_HEADER_LENGTH];
        ssize_t received_bytes = recv(client_sockfd, raw_request, sizeof(raw_request) - 1, 0);
        if (received_bytes < 0) {
            perror("recv");
            close(client_sockfd);
            continue;
        }

        raw_request[received_bytes] = '\0';

        Request request;
        parse_request(&request, raw_request);

        if (strcmp(request.verb, "PUT") == 0) {
            handle_put(client_sockfd, &request);
        } else if (strcmp(request.verb, "GET") == 0) {
            handle_get(client_sockfd, &request);
        } else if (strcmp(request.verb, "DELETE") == 0) {
            handle_delete(client_sockfd, &request);
        } else if (strcmp(request.verb, "LIST") == 0) {
            handle_list(client_sockfd, &request);
        } else {
            fprintf(stderr, "Unknown request: %s\n", request.verb);
        }
        //handle_client_request(client_sockfd);
        shutdown(client_sockfd, SHUT_WR);
        //shutdown(client_sockfd, SHUT_RDWR);
        close(client_sockfd);
        printf("Finished handling client request\n");
    }

    return 0;
}



//void send_response(int sockfd, Response *response);

void handle_put(int sockfd, Request *request) {
    printf("Entering handle_put\n");
    FILE *file = fopen(request->filename, "wb");
    if (file == NULL) {
        Response response = {1, "Error opening file for writing", 0};
        send_response(sockfd, &response);
        close(sockfd); // close the socket before returning
        return;
    }Response ready_response = {0, "Ready to receive file", 0};
    send_response(sockfd, &ready_response);

    char buffer[4096];
    size_t remaining = request->file_size;
    printf("Starting file transfer loop\n"); //
    while (remaining > 0) {
        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            perror("recv");
            break;
        }

        //printf("Received %zd bytes\n", bytes_received);
        fwrite(buffer, 1, bytes_received, file);
        remaining -= bytes_received;
    }
    printf("File transfer loop finished\n"); //

    fclose(file);

    if (remaining == 0) {
        Response response = {0, "", 0};
        printf("Sending success response\n");
        send_response(sockfd, &response);
    } else {
        Response response = {1, "Error receiving file data", 0};
        printf("Sending error response\n");
        send_response(sockfd, &response);
    }
}
//

void handle_get(int sockfd, Request *request) {
    printf("Entering handle_get\n");
    FILE *file = fopen(request->filename, "rb");
    if (!file) {
        Response response;
        response.error = 1;
        response.file_size = 0;
        send_response(sockfd, &response);
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    printf("Server: File size: %ld\n", file_size);

    Response response;
    response.error = 0;
    response.file_size = file_size;
    send_response(sockfd, &response);

    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        ssize_t sent_bytes = send(sockfd, buffer, bytes_read, 0);
        printf("Server: Bytes read: %zu, Bytes sent: %zd\n", bytes_read, sent_bytes);
        if (sent_bytes < 0) {
            perror("send");
            fclose(file);
            return;
        }
    }

    fclose(file);
}
//

void handle_delete(int sockfd, Request *request) {
    if (remove(request->filename) == 0) {
        Response response = {0, "", 0};
        send_response(sockfd, &response);
    } else {
        Response response = {1, "Error deleting file", 0};
        send_response(sockfd, &response);
    }
}

//void handle_list(int sockfd, Request *request) {
void handle_list(int sockfd, Request *request __attribute__((unused))) {
    DIR *dir = opendir(".");
    if (dir == NULL) {
        Response response = {1, "Error opening directory", 0};
        send_response(sockfd, &response);
        return;
    }

    struct dirent *entry;
    size_t total_size = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            total_size += strlen(entry->d_name) + 1;
        }
    }

    Response response = {0, "", total_size};
    send_response(sockfd, &response);

    rewinddir(dir);
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            printf("Sending file name: %s\n", entry->d_name); // Debug print
            send(sockfd, entry->d_name, strlen(entry->d_name), 0);
            send(sockfd, "\n", 1, 0);
        }
    }

    closedir(dir);
}

void send_response(int sockfd, Response *response) {
    char raw_response[MAX_HEADER_LENGTH];
    serialize_response(response, raw_response);
    send(sockfd, raw_response,     strlen(raw_response), 0);
    send(sockfd, "\n", 1, 0);  // Add this line to send a newline character after the header
}

void handle_client_request(int sockfd) {
    char raw_request[MAX_HEADER_LENGTH];
    recv(sockfd, raw_request, sizeof(raw_request), 0);

    Request request;
    parse_request(&request, raw_request);

    if (strcmp(request.verb, "PUT") == 0) {
        handle_put(sockfd, &request);
    } else if (strcmp(request.verb, "GET") == 0) {
        handle_get(sockfd, &request);
    } else if (strcmp(request.verb, "DELETE") == 0) {
        handle_delete(sockfd, &request);
    } else if (strcmp(request.verb, "LIST") == 0) {
        handle_list(sockfd, &request);
    } else {
        Response response = {1, "Unknown command", 0};
        send_response(sockfd, &response);
    }
}


