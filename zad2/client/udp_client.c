#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define DGRAMSIZE 512

#define bailout(s) { perror(s); exit(1); }
#define Usage() { errx(0, "Usage: %s <host> <port> <count> <timeout_ms>\n", argv[0]); }
#define timeinms(tv) ( (tv.tv_sec) * 1000.0 + (tv.tv_usec) / 1000.0 )

static int send_reliable(int sock, int seq, const char *tag, int *retransmissions) {
    char databuf[DGRAMSIZE];
    char ackbuf[64];
    char expected_ack[32];
    struct timeval tv;
    snprintf(expected_ack, sizeof(expected_ack), "ACK %d", seq);
    while (1) {
        memset(databuf, '*', DGRAMSIZE);
        gettimeofday(&tv, NULL);
        int header_len = snprintf(databuf, DGRAMSIZE, "%d %.3f %s ", seq, timeinms(tv), tag);
        if (header_len > 0 && header_len < DGRAMSIZE) {
            databuf[header_len] = '*';
        }

        if (send(sock, databuf, DGRAMSIZE, 0) < 0) {
            perror("send");
            return -1;
        }

        ssize_t n = recv(sock, ackbuf, sizeof(ackbuf) - 1, 0);
        if (n > 0) {
            ackbuf[n] = '\0';
            if (strncmp(ackbuf, expected_ack, strlen(expected_ack)) == 0) {
                return 0;
            }
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            (*retransmissions)++;
            continue;
        } else {
            perror("recv");
            return -1;
        }
    }
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server;
    struct hostent *hp;
    struct timeval tv_to, t_start, t_end;
    if (argc != 5) {
        Usage();
    }

    int count = atoi(argv[3]);
    int timeout_ms = atoi(argv[4]);
    if (count <= 0 || timeout_ms <= 0) {
        errx(1, "count and timeout_ms must be > 0\n");
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        bailout("opening datagram socket");
    }

    server.sin_family = AF_INET;
    hp = gethostbyname2(argv[1], AF_INET);
    if (hp == NULL) {
        errx(2, "%s: unknown host\n", argv[1]);
    }

    memcpy((char *)&server.sin_addr, (char *)hp->h_addr, hp->h_length);
    server.sin_port = htons(atoi(argv[2]));
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == -1) {
        bailout("connect");
    }

    tv_to.tv_sec = timeout_ms / 1000;
    tv_to.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_to, sizeof(tv_to)) == -1) {
        bailout("setsockopt");
    }

    printf("Sending %d datagrams (%d B) to %s:%s (timeout %d ms)\n", count, DGRAMSIZE, argv[1], argv[2], timeout_ms);
    int seq = 0;
    int retransmissions = 0;
    gettimeofday(&t_start, NULL);
    for (int i = 0; i < count; i++) {
        if (send_reliable(sock, seq, "DATA", &retransmissions) == -1) {
            close(sock);
            exit(1);
        }
        seq = 1 - seq;
    }

    if (send_reliable(sock, seq, "FIN", &retransmissions) == -1) {
        close(sock);
        exit(1);
    }

    gettimeofday(&t_end, NULL);
    double total_s = (timeinms(t_end) - timeinms(t_start)) / 1000.0;
    double total_kb = (count * DGRAMSIZE) / 1024.0;
    double throughput = total_kb / total_s;
    printf("Finished in %.3f s, throughput: %.2f KB/s, retransmissions: %d\n\n", total_s, throughput, retransmissions);
    close(sock);
    return 0;
}
