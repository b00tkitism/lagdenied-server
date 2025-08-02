#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 12345

int main() {
    int sock1, sock2;
    int optval = 1;
    struct sockaddr_in addr;

    // Create first socket
    sock1 = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock1 < 0) {
        perror("socket 1");
        return 1;
    }

    // Set SO_REUSEPORT on first socket
    if (setsockopt(sock1, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        perror("setsockopt 1");
        close(sock1);
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    // Bind first socket
    if (bind(sock1, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind 1");
        close(sock1);
        return 1;
    }

    printf("First socket bound successfully\n");

    // Create second socket
    sock2 = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock2 < 0) {
        perror("socket 2");
        close(sock1);
        return 1;
    }

    // Set SO_REUSEPORT on second socket
    if (setsockopt(sock2, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        perror("setsockopt 2");
        close(sock1);
        close(sock2);
        return 1;
    }

    // Bind second socket to the same port
    if (bind(sock2, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind 2");
        close(sock1);
        close(sock2);
        return 1;
    }

    printf("Second socket bound successfully. SO_REUSEPORT works!\n");

    close(sock1);
    close(sock2);
    return 0;
}
