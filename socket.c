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

/* Socket API headers */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Definitions */
#define DEFAULT_BUFLEN 512
#define PORT 6673
#define MAX_USERS 2

/* User and Password information */
struct UserInfo {
    char username[50];
    char password[50];
};

/* Function to perform user authentication */
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

int main() {
    int server, client;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int length, fd, rcnt, optval;
    char recvbuf[DEFAULT_BUFLEN], bmsg[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    /* User information */
    struct UserInfo users[MAX_USERS] = { {"alice", "qwerty"}, {"bob", "2021.sockets"} };

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
        if ((fd = accept(server, (struct sockaddr*)&remote_addr, \
            &length)) == -1) {
            perror("Accept Problem!");
            continue;
        }

        printf("Server: got connection from %s\n", \
            inet_ntoa(remote_addr.sin_addr));

        // Receive until the peer shuts down the connection
        do {
            // Clear Receive buffer
            memset(&recvbuf, '\0', sizeof(recvbuf));
            rcnt = recv(fd, recvbuf, recvbuflen, 0);
            if (rcnt > 0) {
                printf("Bytes received: %d\n", rcnt);

                // Authenticate user
                int authResult = authenticateUser(recvbuf, users);

                // Respond based on authentication result
                if (authResult == 200) {
                    sprintf(bmsg, "200 User %s granted to access.\n", users[0].username);
                } else {
                    sprintf(bmsg, "400 User not found. Please try with another user.\n");
                }

                // Echo the response back to the sender
                rcnt = send(fd, bmsg, strlen(bmsg), 0);
                if (rcnt < 0) {
                    printf("Send failed:\n");
                    close(fd);
                    break;
                }
                printf("Bytes sent: %d\n", rcnt);
            } else if (rcnt == 0)
                printf("Connection closing...\n");
            else {
                printf("Receive failed:\n");
                close(fd);
                break;
            }
        } while (rcnt > 0);
    }

    // Final Cleanup
    close(server);
    return 0;
}
