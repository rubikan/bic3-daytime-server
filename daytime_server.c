/**
 *
 * @file daytime_server.c
 * RFC 867 Daytime Server
 *
 * @author Andreas Rubik <andreas.rubik@technikum-wien.at>
 * @author Anna Glock <ic14b070@technikum-wien.at>
 *
 * @version 1.0
 * @date 2016/01/11
 *
 */

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BACKLOG 5
#define MIN_PORT 0
#define MAX_PORT 65535
#define DEFAULT_PORT 13
#define MBUFF_START_SIZE 8
#define MBUFF_STEP_SIZE 2

static void error(char* message, char* argv0);

/**
 * \brief Server application that returns a current timestamp if connected via telnet
 *
 * \param argc Argument count
 * \param argv[] Argument vector
 */
int main(int argc, char* argv[]) {
    char* checkvar;
    // First, check if there is either no argument or 2
    if (argc != 1 && argc != 3) {
        error("Usage: daytime_server [-p port]\n", argv[0]);
    }
    // Check option arguments for port, use default if no port is given
    int o;
    int port = -1;
    while ((o = getopt(argc, argv, "p:")) != -1) {
        switch(o) {
            case 'p':
                errno = 0;
                port = strtol(optarg, &checkvar, 10);
                if (errno == ERANGE || errno == EINVAL) {
                    error("Invalid input!\n", argv[0]);
                }
                if (checkvar[0] != '\0') {
                    error("Only numerical input allowed for port!\n", argv[0]);
                }
                if (port < MIN_PORT || port > MAX_PORT) {
                    error("The port has to be in the range 0..65535!\n", argv[0]);
                }
                break;
            default:
                error("Usage: daytime_server [-p port]\n", argv[0]);
        }
    }

    if (port == -1) {
        // No port given, using default port
        port = DEFAULT_PORT;
    }

    int serverSocketID = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocketID == -1) {
        error("Error creating socket!\n", argv[0]);
    }

    struct sockaddr_in serverSocketAdress;
    // Internet addresses
    serverSocketAdress.sin_family = AF_INET;
    // Accept connections from all adresses
    serverSocketAdress.sin_addr.s_addr = htonl(INADDR_ANY);
    // Define serverport
    serverSocketAdress.sin_port = htons(port);

    if (bind(serverSocketID, (struct sockaddr*)&serverSocketAdress, sizeof(serverSocketAdress)) < 0) {
        if (errno == EACCES) {
            close(serverSocketID);
            error("Error: To use ports below 1024, this program needs to be run as superuser (sudo)! The default port used is 13.\n", argv[0]);
        } else {
            close(serverSocketID);
            error("Error binding socket!\n", argv[0]);
        }
    }

    if (listen(serverSocketID, BACKLOG) == -1) {
        close(serverSocketID);
        error("Error listening on socket!\n", argv[0]);
    }

    for(;;) {
        // Accept a connection on the server port
        int c = sizeof(struct sockaddr_in);
        struct sockaddr_in clientSocketAdress;
        int connectedClient = accept(serverSocketID, (struct sockaddr*)&clientSocketAdress, (socklen_t*)&c);
        if (connectedClient < 0) {
            if (errno == EINTR) {
                // accept was interrupted, trying again
                errno = 0;
                continue;
            } else {
                close(serverSocketID);
                error("Error accepting incoming connection!\n", argv[0]);
            }
        }

        char* message;
        int tempsize = MBUFF_START_SIZE;
        message = (char*) malloc(tempsize);
        if (message == NULL) {
            error("Error allocating space for message!\n", argv[0]);
        }
        memset(message, 0, tempsize);
        time_t timer;
        if (time(&timer) == (time_t)(-1)) {
            error("Error getting calendar time!\n", argv[0]);
        }
        struct tm* tm_info;
        tm_info = localtime(&timer);
        if (tm_info == NULL) {
            error("Error getting localtime!\n", argv[0]);
        }

        while(strftime(message, tempsize, "%A, %B %d, %Y %H:%M:%S-%Z\n", tm_info) <= 0) {
            tempsize *= MBUFF_STEP_SIZE;
            if((message = realloc(message, tempsize)) == NULL) {
                error("Error resizing message buffer!\n", argv[0]);
            }
            memset(message, 0, tempsize);
        }

        if (write(connectedClient, message, tempsize) < 0) {
            error("Error writing to socket!\n", argv[0]);
        }

        free(message);
        if (shutdown(connectedClient, SHUT_RDWR) < 0) {
            error("Error shutting down socket!\n", argv[0]);
        }
    }
}

/**
 * \brief Prints error messages and exits, also exit if an error happens
 *
 * \param message The error message that should be printed
 * \param argv0 The content of argv[0] to be included in the error message
 */
static void error(char* message, char* argv0) {
    // Length of all strings, + 3 for zero terminator and ": " in between,
    char* output = (char*) malloc(strlen(message) + strlen(argv0) + 3);
    strcpy(output, argv0);
    // strcat instead of strncat should be okay, since string literals are always zero terminated
    strcat(output, ": ");
    strcat(output, message);
    if (fprintf(stderr, "%s", output) < 0) {
        exit(EXIT_FAILURE);
    }
    if(fflush(stderr) == EOF) {
        exit(EXIT_FAILURE);
    }
    free(output);
    exit(EXIT_FAILURE);
}
