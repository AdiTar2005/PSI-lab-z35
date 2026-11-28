#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define DGRAMSIZE 512

static int send_reliable(int sock, int sid, int seq, const char *type, int max_retries, int *retrans) {
    char buf[DGRAMSIZE];
    char ack[32];
    char want[32];
    struct timeval tv;
    int tries = 0;
    sprintf(want, "ACK %d %d", sid, seq);
    while (1) {
        memset(buf, '*', DGRAMSIZE);
        gettimeofday(&tv, NULL);
        double ts = tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
        int len = snprintf(buf, DGRAMSIZE, "%d %d %.3f %s ", sid, seq, ts, type);
        if (len > 0 && len < DGRAMSIZE) {
            buf[len] = '*';
        }

        if (send(sock, buf, DGRAMSIZE, 0) < 0) {
            perror("send");
            return -1;
        }

        ssize_t n = recv(sock, ack, sizeof(ack) - 1, 0);
        if (n > 0) {
            ack[n] = '\0';
            if (strcmp(ack, want) == 0) {
                return 0;
            }
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            (*retrans)++;
            tries++;
            if (max_retries > 0 && tries >= max_retries) {
                return 1;
            }
        } else {
            perror("recv");
            return -1;
        }
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server;
    struct hostent *hp;
    struct timeval to, t1, t2;

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <host> <port> <count> <timeout_ms>\n", argv[0]);
        return 1;
    }

    int count = atoi(argv[3]);
    int timeout_ms = atoi(argv[4]);
    if (count <= 0 || timeout_ms <= 0) {
        fprintf(stderr, "Invalid count or timeout\n");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    hp = gethostbyname2(argv[1], AF_INET);
    if (!hp) {
        fprintf(stderr, "Unknown host: %s\n", argv[1]);
        close(sock);
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
    server.sin_port = htons(atoi(argv[2]));
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    to.tv_sec = timeout_ms / 1000;
    to.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)) < 0) {
        perror("setsockopt");
        close(sock);
        return 1;
    }

    int sid = (int)getpid();
    printf("Sending %d packets (%d B) to %s:%s (timeout %d ms)\n", count, DGRAMSIZE, argv[1], argv[2], timeout_ms);
    int seq = 0;
    int retrans = 0;
    gettimeofday(&t1, NULL);
    for (int i = 0; i < count; i++) {
        if (send_reliable(sock, sid, seq, "DATA", 0, &retrans) < 0) {
            close(sock);
            return 1;
        }
        seq = 1 - seq;
    }

    if (send_reliable(sock, sid, seq, "FIN", 20, &retrans) < 0) {
        close(sock);
        return 1;
    }

    gettimeofday(&t2, NULL);
    double total_s = (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1e6;
    double throughput = (count * DGRAMSIZE / 1024.0) / total_s;
    printf("Done in %.3f s, throughput: %.2f KB/s, retransmissions: %d\n", total_s, throughput, retrans);
    close(sock);
    return 0;
}