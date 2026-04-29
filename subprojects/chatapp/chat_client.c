#define _POSIX_C_SOURCE 200809L

#include <hackode/lib.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int parse_port(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (!s || !*s || !end || *end || v <= 0 || v > 65535) return -1;
    return (int)v;
}

int main(int argc, char **argv) {
    const char *dict_path = "/usr/share/hackode/hackode.map";
    const char *host = "127.0.0.1";
    int port = 9099;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--dict") == 0) && i + 1 < argc) {
            dict_path = argv[++i];
        } else if ((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0) && i + 1 < argc) {
            host = argv[++i];
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            port = parse_port(argv[++i]);
            if (port < 0) {
                fprintf(stderr, "invalid port\n");
                return 2;
            }
        } else {
            fprintf(stderr, "usage: %s [-D dict.{map,txt}] [-h host] [-p port]\n", argv[0]);
            return 2;
        }
    }

    int hc_err = 0;
    hacker_cipher_t *hc = hc_create_from_dictsource(dict_path, 4, &hc_err);
    if (!hc) {
        fprintf(stderr, "hc_create_from_dictsource failed: %d\n", hc_err);
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        hc_destroy(hc);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid host: %s\n", host);
        close(fd);
        hc_destroy(hc);
        return 2;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("connect");
        close(fd);
        hc_destroy(hc);
        return 1;
    }

    char plain[4096];
    printf("message> ");
    if (!fgets(plain, sizeof(plain), stdin)) {
        close(fd);
        hc_destroy(hc);
        return 1;
    }
    plain[strcspn(plain, "\r\n")] = '\0';

    char *enc = NULL;
    if (hc_encrypt_str(hc, plain, &enc) != HC_OK || !enc) {
        fprintf(stderr, "encrypt failed\n");
        close(fd);
        hc_destroy(hc);
        return 1;
    }
    dprintf(fd, "%s\n", enc);
    free(enc);

    char inbuf[8192];
    ssize_t n = read(fd, inbuf, sizeof(inbuf) - 1);
    if (n <= 0) {
        perror("read");
        close(fd);
        hc_destroy(hc);
        return 1;
    }
    inbuf[n] = '\0';
    inbuf[strcspn(inbuf, "\r\n")] = '\0';

    char *reply = NULL;
    if (hc_decrypt_str(hc, inbuf, &reply) != HC_OK || !reply) {
        fprintf(stderr, "decrypt failed\n");
        close(fd);
        hc_destroy(hc);
        return 1;
    }
    printf("server reply: %s\n", reply);
    free(reply);

    close(fd);
    hc_destroy(hc);
    return 0;
}
