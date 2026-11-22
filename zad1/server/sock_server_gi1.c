#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEF_PORT     "8888"
#define DEF_DELAY_US 5000
#define BSIZE        4096

static volatile sig_atomic_t running = 1;

static void stop_server(int sig) {
    (void)sig;
    running = 0;
}

static int init_server(const char *port) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int optval = 1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int err = getaddrinfo(NULL, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == -1) {
        perror("opening stream socket");
        freeaddrinfo(res);
        return -1;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (bind(sock, res->ai_addr, res->ai_addrlen) == -1) {
        perror("binding stream socket");
        close(sock);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    if (listen(sock, 5) == -1) {
        perror("listen");
        close(sock);
        return -1;
    }

    return sock;
}

static void handle_client(int msgsock, int delay_us) {
    char buf[BSIZE];
    struct timespec req = { delay_us / 1000000, (delay_us % 1000000) * 1000L };
    printf("Client connected\n");
    fflush(stdout);

    while (1) {
        ssize_t rval = read(msgsock, buf, BSIZE);
        if (rval == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("reading stream message");
            break;
        }
        if (rval == 0) {
            printf("Ending connection\n");
            break;
        }
        if (delay_us > 0) {
            nanosleep(&req, NULL);
        }
    }
}

int main(int argc, char *argv[]) {
    const char *port = (argc >= 2) ? argv[1] : DEF_PORT;
    int delay_us = (argc >= 3) ? atoi(argv[2]) : DEF_DELAY_US;
    signal(SIGINT, stop_server);
    signal(SIGTERM, stop_server);
    int sock = init_server(port);
    if (sock == -1) {
        return 1;
    }

    while (running) {
        int msgsock = accept(sock, NULL, NULL);
        if (msgsock == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        handle_client(msgsock, delay_us);
        close(msgsock);
        fflush(stdout);
    }

    close(sock);
    return 0;
}
