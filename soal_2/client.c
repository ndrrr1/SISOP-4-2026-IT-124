#define _POSIX_C_SOURCE 200112L

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT "9000"
#define BUFFER_SIZE 4096

static int send_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static int connect_to_server(const char *host, const char *port) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *rp = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai = getaddrinfo(host, port, &hints, &res);
    if (gai != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai));
        return -1;
    }

    int fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;
}

static void read_response(int fd) {
    char buf[BUFFER_SIZE + 1];
    int got_any = 0;

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        struct timeval tv;
        tv.tv_sec = got_any ? 0 : 2;
        tv.tv_usec = got_any ? 250000 : 0;

        int ready = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            return;
        }
        if (ready == 0) break;

        ssize_t n = recv(fd, buf, BUFFER_SIZE, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            return;
        }
        if (n == 0) {
            printf("\n[server closed connection]\n");
            exit(EXIT_SUCCESS);
        }
        buf[n] = '\0';
        fputs(buf, stdout);
        got_any = 1;
    }

    if (!got_any) printf("[no response]\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    const char *host = (argc > 1) ? argv[1] : DEFAULT_HOST;
    const char *port = (argc > 2) ? argv[2] : DEFAULT_PORT;

    int fd = connect_to_server(host, port);
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%s\n", host, port);
        return EXIT_FAILURE;
    }

    printf("Connected to DB Server on %s:%s\n", host, port);
    printf("Type EXIT to quit.\n");

    char line[BUFFER_SIZE];
    while (1) {
        printf("db> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';

        if (strcmp(line, "EXIT") == 0 || strcmp(line, "exit") == 0 ||
            strcmp(line, "QUIT") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        if (line[0] == '\0') continue;

        char request[BUFFER_SIZE + 2];
        int n = snprintf(request, sizeof(request), "%s\n", line);
        if (n < 0 || (size_t)n >= sizeof(request)) {
            fprintf(stderr, "Command too long\n");
            continue;
        }

        if (send_all(fd, request, (size_t)n) == -1) {
            perror("send");
            close(fd);
            return EXIT_FAILURE;
        }
        read_response(fd);
    }

    close(fd);
    return EXIT_SUCCESS;
}
