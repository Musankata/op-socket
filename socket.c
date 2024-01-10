/*
  This code represents a basic TCP Echo Server with user and password authentication.
  It's used to teach basic server running.
  For compile code: # gcc prog.name.c -o execfile
  -----------------------Copyright Devrim Seral---------------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_BUFLEN 512
#define PORT 6673
#define MAX_USERS 2
#define MAX_FILES 3

struct UserInfo {
    char username[50];
    char password[50];
};

struct FileInfo {
    char filename[50];
    int size;
};

// Function to perform user authentication
int authenticateUser(char* receivedData, struct UserInfo users[MAX_USERS]) {
    char username[50], password[50];
    sscanf(receivedData, "USER %s %s", username, password);

    for (int i = 0; i < MAX_USERS; ++i) {
        if (strcmp(users[i].username, username) == 0 && strcmp(users[i].password, password) == 0) {
            return 200;  // Successful authentication
        }
    }

    return 400;  // User not found
}

// Function to handle LIST command
void handleListCommand(int clientSocket, struct FileInfo files[MAX_FILES]) {
    char listResponse[DEFAULT_BUFLEN];
    strcpy(listResponse, "Files:\n");

    for (int i = 0; i < MAX_FILES; ++i) {
        char fileInfo[50];
        sprintf(fileInfo, "file%d %d\n", i + 1, files[i].size);
        strcat(listResponse, fileInfo);
    }

    strcat(listResponse, ".\n");

    send(clientSocket, listResponse, strlen(listResponse), 0);
}

// Function to handle GET command
void handleGetCommand(int clientSocket, struct FileInfo files[MAX_FILES], char* filename) {
    int fileIndex = -1;
    for (int i = 0; i < MAX_FILES; ++i) {
        if (strcmp(files[i].filename, filename) == 0) {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex != -1) {
        FILE* file = fopen(files[fileIndex].filename, "r");
        if (file != NULL) {
            char getResponse[DEFAULT_BUFLEN];
            sprintf(getResponse, "GET %s\n", filename);
            send(clientSocket, getResponse, strlen(getResponse), 0);

            char fileContent[DEFAULT_BUFLEN];
            while (fgets(fileContent, sizeof(fileContent), file) != NULL) {
                send(clientSocket, fileContent, strlen(fileContent), 0);
            }

            send(clientSocket, "\r\n.\r\n", 5, 0);

            fclose(file);
        } else {
            send(clientSocket, "500 Internal Server Error\n", 26, 0);
        }
    } else {
        send(clientSocket, "404 File not found\n", 19, 0);
    }
}

// Function to handle PUT command
void handlePutCommand(int clientSocket, struct FileInfo files[MAX_FILES], char* filename) {
    char putResponse[DEFAULT_BUFLEN];

    FILE* file = fopen(filename, "w");
    if (file != NULL) {
        char fileContent[DEFAULT_BUFLEN];
        int totalBytesReceived = 0;

        // Receive file content until termination signal is received
        while (recv(clientSocket, fileContent, sizeof(fileContent), 0) > 0) {
            if (strcmp(fileContent, "\r\n.\r\n") == 0) {
                break;
            }

            // Write received content to file
            fprintf(file, "%s", fileContent);
            totalBytesReceived += strlen(fileContent);
        }

        fclose(file);

        // Send response based on file writing status
        sprintf(putResponse, "200 %d Byte %s retrieved by server and was saved.\n", totalBytesReceived, filename);
    } else {
        // Unable to open the file for writing
        sprintf(putResponse, "400 File cannot be saved on server side.\n");
    }

    send(clientSocket, putResponse, strlen(putResponse), 0);
}

int main() {
    int server, client;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int length, fd, rcnt, optval;
    char recvbuf[DEFAULT_BUFLEN], bmsg[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    /* User information */
    struct UserInfo users[MAX_USERS] = {{"alice", "qwerty"}, {"bob", "2021.sockets"}};

    /* File information */
    struct FileInfo files[MAX_FILES] = {{"file1", 100}, {"file2", 150}, {"file3", 200}};

    /* Open socket descriptor */
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Can't create socket!");
        return (1);
    }

    /* Fill local and remote address structure with zero */
    memset(&local_addr, 0, sizeof(local_addr));
    memset(&remote_addr, 0, sizeof(remote_addr));

    /* Set values to local_addr structure */
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(PORT);

    // set SO_REUSEADDR on a socket to true (1):
    optval = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

    if (bind(server, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        /* could not start server */
        perror("Bind problem!");
        return (1);
    }

    if (listen(server, SOMAXCONN) < 0) {
        perror("Listen");
        exit(1);
    }

    printf("First socket server now starting!\n");
    printf("Wait for connection\n");

    while (1) { // main accept() loop
        length = sizeof remote_addr;
        if ((fd = accept(server, (struct sockaddr*)&remote_addr, &length)) == -1) {
            perror("Accept Problem!");
            continue;
        }

        printf("Server: got connection from %s\n", inet_ntoa(remote_addr.sin_addr));

        // Receive until the peer shuts down the connection
        do {
            memset(&recvbuf, '\0', sizeof(recvbuf));
            rcnt = recv(fd, recvbuf, recvbuflen, 0);
            if (rcnt > 0) {
                printf("Bytes received: %d\n", rcnt);

                int authResult = authenticateUser(recvbuf, users);

                if (authResult == 200) {
                    if (strncmp(recvbuf, "LIST", 4) == 0) {
                        handleListCommand(fd, files);
                    } else if (strncmp(recvbuf, "GET", 3) == 0) {
                        char filename[50];
                        sscanf(recvbuf, "GET %s", filename);
                        handleGetCommand(fd, files, filename);
                    } else if (strncmp(recvbuf, "PUT", 3) == 0) {
                        char filename[50];
                        sscanf(recvbuf, "PUT %s", filename);
                        handlePutCommand(fd, files, filename);
                    } else {
                        sprintf(bmsg, "200 User %s granted access.\n", users[0].username);
                    }
                } else {
                    sprintf(bmsg, "400 User not found. Please try with another user.\n");
                }

                rcnt = send(fd, bmsg, strlen(bmsg), 0);
                if (rcnt < 0) {
                    perror("Send failed:");
                    close(fd);
                    break;
                }
                printf("Bytes sent: %d\n", rcnt);
            } else if (rcnt == 0)
                printf("Connection closing...\n");
            else {
                perror("Receive failed:");
                close(fd);
                break;
            }
        } while (rcnt > 0);
    }

    // Final Cleanup
    close(server);
    return 0;
}
