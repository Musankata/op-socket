#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Definitions */
#define DEFAULT_BUFLEN 512
#define PORT 6673
#define MAX_USERS 5
#define MAX_FILES 10

/* User and Password information */
struct UserInfo {
    char username[50];
    char password[50];
};

/* File information */
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
            return 200;  // User found and authenticated
        }
    }

    return 400;  // User not found
}

// Function to handle LIST command
void handleListCommand(int clientSocket, const char* directory) {
    DIR* dir = opendir(directory);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent* entry;
    struct stat st;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            stat(entry->d_name, &st);
            char response[DEFAULT_BUFLEN];
            sprintf(response, "%s %ld\n", entry->d_name, st.st_size);
            send(clientSocket, response, strlen(response), 0);
        }
    }

    closedir(dir);
    // Terminate the list with "."
    send(clientSocket, ".", 1, 0);
}

// Function to handle GET command
void handleGetCommand(int clientSocket, const char* directory, char* filename) {
    char filePath[100];
    sprintf(filePath, "%s/%s", directory, filename);

    FILE* file = fopen(filePath, "r");
    if (file != NULL) {
        char buffer[DEFAULT_BUFLEN];
        size_t bytesRead;

        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            send(clientSocket, buffer, bytesRead, 0);
        }

        fclose(file);
    } else {
        // Unable to open the file
        send(clientSocket, "404 File not found\n", 19, 0);
    }
}

// Function to handle PUT command
void handlePutCommand(int clientSocket, const char* directory, char* filename) {
    char filePath[100];
    sprintf(filePath, "%s/%s", directory, filename);

    FILE* file = fopen(filePath, "w");
    if (file != NULL) {
        char buffer[DEFAULT_BUFLEN];
        int totalBytesReceived = 0;

        while (1) {
            ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) {
                break;
            }

            fwrite(buffer, 1, bytesRead, file);
            totalBytesReceived += bytesRead;

            if (strstr(buffer, "\r\n.\r\n") != NULL) {
                break;
            }
        }

        fclose(file);

        char response[DEFAULT_BUFLEN];
        sprintf(response, "200 %d Bytes %s retrieved by server and was saved.\n", totalBytesReceived, filename);
        send(clientSocket, response, strlen(response), 0);
    } else {
        // Unable to open the file for writing
        send(clientSocket, "400 File cannot be saved on server side.\n", 41, 0);
    }
}

// Function to handle DEL command
void handleDelCommand(int clientSocket, const char* directory, char* filename) {
    char filePath[100];
    sprintf(filePath, "%s/%s", directory, filename);

    if (remove(filePath) == 0) {
        char response[DEFAULT_BUFLEN];
        sprintf(response, "200 File %s deleted.\n", filename);
        send(clientSocket, response, strlen(response), 0);
    } else {
        char response[DEFAULT_BUFLEN];
        sprintf(response, "404 File %s is not on the server.\n", filename);
        send(clientSocket, response, strlen(response), 0);
    }
}

int main() {
    int server, client;
    struct sockaddr_in local_addr, remote_addr;
    socklen_t length;

    // User and password information
    struct UserInfo users[MAX_USERS] = {
        {"Luc", "2020"},
        {"Lukas", "2021"},
        {"Lucas", "2022"},
        {"Musa", "2023"},
        {"Nkulu", "2024"}
    };

    // File information
    struct FileInfo files[MAX_FILES] = {
        {"file1.txt", 100},
        {"file2.txt", 200},
        {"file3.txt", 300}
    };

    const char* directory = "/home/musa/Desktop/Op-socket";

    // Create socket
    server = socket(AF_INET, SOCK_STREAM, 0);

    // Prepare the sockaddr_in structure
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(PORT);

    // Bind
    if (bind(server, (struct sockaddr*)&local_addr, sizeof(local_addr)) == -1) {
        perror("bind");
        return 1;
    }

    // Listen
    if (listen(server, 5) == -1) {
        perror("listen");
        return 1;
    }

    printf("Waiting for connection on localhost port %d\n", PORT);

    while (1) {
        length = sizeof remote_addr;

        if ((client = accept(server, (struct sockaddr*)&remote_addr, &length)) == -1) {
            perror("Accept Problem!");
            continue;
        }

        printf("Server: got connection from %s\n", inet_ntoa(remote_addr.sin_addr));

        char recvbuf[DEFAULT_BUFLEN];
        ssize_t rcnt;

        // Declare bmsg here
        char bmsg[DEFAULT_BUFLEN];

        // Receive until the peer shuts down the connection
        do {
            // Clear Receive buffer
            memset(&recvbuf, '\0', sizeof(recvbuf));
            rcnt = recv(client, recvbuf, sizeof(recvbuf), 0);
            if (rcnt > 0) {
                printf("Bytes received: %d\n", (int)rcnt);

                // Authenticate user
                int authResult = authenticateUser(recvbuf, users);

                // Respond based on authentication result
                if (authResult == 200) {
                    // Display welcome message for user "Luc"
                    if (strcmp(users[0].username, "Luc") == 0) {
                        sprintf(bmsg, "Welcome %s!\nPlease enter your password: ", users[0].username);
                        send(client, bmsg, strlen(bmsg), 0);
                    }

                    // Receive password
                    memset(&recvbuf, '\0', sizeof(recvbuf));
                    rcnt = recv(client, recvbuf, sizeof(recvbuf), 0);
                    if (rcnt > 0) {
                        printf("Password received: %s\n", recvbuf);

                        // Check the password received against the stored password for user "Luc"
                        if (strcmp(users[0].password, recvbuf) == 0) {
                            // Password is correct
                            sprintf(bmsg, "200 Password correct. Access granted.\n");
                            send(client, bmsg, strlen(bmsg), 0);

                            // Continue processing other commands like LIST, GET, PUT, DEL, QUIT...
                            // For simplicity, you can add function calls here to handle these commands.
                        } else {
                            // Password is incorrect
                            sprintf(bmsg, "401 Incorrect password. Access denied.\n");
                            send(client, bmsg, strlen(bmsg), 0);
                        }
                    } else {
                        perror("Receive failed:");
                        close(client);
                        break;
                    }
                } else {
                    // Respond with a user not found message
                    sprintf(bmsg, "400 User not found. Please try with another user.\n");
                    send(client, bmsg, strlen(bmsg), 0);
                }

                printf("Bytes sent: %d\n", (int)strlen(bmsg));

                // Handle other commands based on the received data...
            } else if (rcnt == 0) {
                printf("Connection closing...\n");
            } else {
                perror("Receive failed:");
                close(client);
                break;
            }
        } while (rcnt > 0);
    }

    // Final Cleanup
    close(server);

    return 0;
}
