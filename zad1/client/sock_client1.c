#define _POSIX_C_SOURCE 200809L

#include <netinet/tcp.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define TEMP_FAILURE_RETRY(exp) ({         \
    typeof(exp) _rc;                       \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && errno == EINTR); \
    _rc;                                   \
})

static int connect_to_server(const char *host, const char *port) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "%s: %s\n", host, gai_strerror(err));
        return -1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == -1) {
        perror("opening stream socket");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connecting stream socket");
        close(sock);
        freeaddrinfo(res);
        return -1;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    freeaddrinfo(res);
    return sock;
}

static int write_all(int sock, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = TEMP_FAILURE_RETRY(write(sock, buf + sent, len - sent));
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }

    return 0;
}

static int send_data(int sock, const char *buf, size_t buf_size, size_t total_bytes) {
    size_t sent = 0;
    size_t calls = 0;
    double min_us = 0.0, max_us = 0.0, sum_us = 0.0;
    while (sent < total_bytes) {
        size_t chunk = total_bytes - sent;
        if (chunk > buf_size) {
            chunk = buf_size;
        }

        struct timespec t1, t2;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        if (write_all(sock, buf, chunk) == -1) {
            perror("writing on stream socket");
            return -1;
        }

        clock_gettime(CLOCK_MONOTONIC, &t2);
        double dt = (t2.tv_sec - t1.tv_sec) * 1e6 + (t2.tv_nsec - t1.tv_nsec) * 1e-3;
        if (calls == 0) {
            min_us = max_us = dt;
        } else {
            if (dt < min_us) min_us = dt;
            if (dt > max_us) max_us = dt;
        }

        sum_us += dt;
        calls++;
        sent += chunk;
        if (calls <= 5 || sent % 262144 == 0) {
            printf("  [write #%zu] sent=%zu B | time=%.2f us\n", calls, sent, dt);
            fflush(stdout);
        }
    }

    double mb = sent / (1024.0 * 1024.0);
    double total_sec = sum_us / 1e6;
    printf("\n--- Results (buffer = %zu B) ---\n", buf_size);
    printf("Sent:          %zu B (%.2f MB)\n", sent, mb);
    printf("Write calls:   %zu\n", calls);
    printf("Total time:    %.3f s\n", total_sec);
    printf("Throughput:    %.3f MB/s\n", mb / total_sec);
    printf("write() MIN:   %.2f us\n", min_us);
    printf("write() AVG:   %.2f us\n", sum_us / calls);
    printf("write() MAX:   %.2f us\n", max_us);
    printf("--------------------------------\n\n");

    shutdown(sock, SHUT_WR);
    char end_byte;
    TEMP_FAILURE_RETRY(read(sock, &end_byte, 1));
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <host> <port> <buf_size_B> <total_MB>\n", argv[0]);
        return 1;
    }

    int buf_size = atoi(argv[3]);
    int total_mb = atoi(argv[4]);
    if (buf_size <= 0 || total_mb <= 0) {
        fprintf(stderr, "Error: buffer size and total MB must be > 0\n");
        return 1;
    }

    size_t total_bytes = (size_t)total_mb * 1024 * 1024;
    char *buf = malloc(buf_size);
    if (!buf) {
        perror("malloc");
        return 1;
    }
    memset(buf, 'A', buf_size);

    int sock = connect_to_server(argv[1], argv[2]);
    if (sock == -1) {
        free(buf);
        return 1;
    }

    printf("Connected to %s:%s | buffer=%d B | target=%d MB\n", argv[1], argv[2], buf_size, total_mb);
    fflush(stdout);
    int status = send_data(sock, buf, buf_size, total_bytes);
    free(buf);
    close(sock);
    return (status == 0) ? 0 : 1;
}
