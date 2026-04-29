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
    int port = 9099;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--dict") == 0) && i + 1 < argc) {
            dict_path = argv[++i];
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            port = parse_port(argv[++i]);
            if (port < 0) {
                fprintf(stderr, "invalid port\n");
                return 2;
            }
        } else {
            fprintf(stderr, "usage: %s [-D dict.{map,txt}] [-p port]\n", argv[0]);
            return 2;
        }
    }

    int hc_err = 0;
    hacker_cipher_t *hc = hc_create_from_dictsource(dict_path, 4, &hc_err);
    if (!hc) {
        fprintf(stderr, "hc_create_from_dictsource failed: %d\n", hc_err);
        return 1;
    }

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        hc_destroy(hc);
        return 1;
    }
    int on = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(srv);
        hc_destroy(hc);
        return 1;
    }
    if (listen(srv, 1) != 0) {
        perror("listen");
        close(srv);
        hc_destroy(hc);
        return 1;
    }

    printf("chat-server listening on :%d\n", port);
    int cli = accept(srv, NULL, NULL);
    if (cli < 0) {
        perror("accept");
        close(srv);
        hc_destroy(hc);
        return 1;
    }

    char inbuf[8192];
    ssize_t n = read(cli, inbuf, sizeof(inbuf) - 1);
    if (n <= 0) {
        perror("read");
        close(cli);
        close(srv);
        hc_destroy(hc);
        return 1;
    }
    inbuf[n] = '\0';
    inbuf[strcspn(inbuf, "\r\n")] = '\0';

    char *plain = NULL;
    if (hc_decrypt_str(hc, inbuf, &plain) != HC_OK || !plain) {
        fprintf(stderr, "decrypt failed\n");
        close(cli);
        close(srv);
        hc_destroy(hc);
        return 1;
    }
    printf("received plaintext: %s\n", plain);

    char ack[8192];
    snprintf(ack, sizeof(ack), "ACK: %s", plain);
    free(plain);

    char *enc = NULL;
    if (hc_encrypt_str(hc, ack, &enc) != HC_OK || !enc) {
        fprintf(stderr, "encrypt failed\n");
        close(cli);
        close(srv);
        hc_destroy(hc);
        return 1;
    }

    dprintf(cli, "%s\n", enc);
    free(enc);
    close(cli);
    close(srv);
    hc_destroy(hc);
    return 0;
}
