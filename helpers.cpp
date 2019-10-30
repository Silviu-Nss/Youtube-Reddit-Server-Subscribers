// Nastasescu George-Silviu, 321CB

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "helpers.h"

using namespace std;

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int open_connection(char *host_ip, int portno)
{
    struct sockaddr_in serv_addr;
    int sockfd;

    if(portno == 0)
        error("ERROR: incorrect port");

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error("ERROR opening socket");

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    if(inet_aton(host_ip, &serv_addr.sin_addr) == 0)
        error("ERROR: incorrect IP");

    /* connect the socket */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    int yes = 1;
    if(setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0)
        error("ERROR: no_delay didn't work");

    return sockfd;
}

void close_connection(int sockfd)
{
    close(sockfd);
}

void send_to(int sockfd, char *message, int total)
{
    int bytes, sent = 0;

    do
    {
        bytes = write(sockfd, message + sent, total - sent);
        if (bytes < 0)
            error("ERROR writing message to socket");
        if (bytes == 0)
            break;
        sent += bytes;
    } while (sent < total);
}

char *recv_from_server(int sockfd, int& length)
{
    char *response;
    short int len = 0;		// lungimea mesajului
    int total = 2;
    int received = 0;

    do {
        int bytes = read(sockfd, &len + received, total - received);
        if (bytes < 0)
            error("ERROR reading response from socket");
        if (bytes == 0)
        {
            break;
        }
        received += bytes;
    } while (received < total);

    if(received < total)
        return NULL;
    
    length = ntohs(len);
    total = length;
    received = 0;
    response = new char[length + 1]();

    do {
        int bytes = read(sockfd, response + received, total - received);
        if (bytes < 0)
            error("ERROR reading response from socket");
        if (bytes == 0)
            break;
        received += bytes;
    } while (received < total);

    // socket-ul este inchis
    if(received < total) {
        delete[] response;
        return NULL;
    }

    return response;
}


char *recv_from_sub(int sockfd)
{
    char resp[SUBLEN + 1] = {0};
    int total = SUBLEN;
    int received = 0;

    do {
        int bytes = read(sockfd, resp + received, total - received);
        if (bytes < 0)
            error("ERROR reading response from socket");
        if (bytes == 0)
            break;
        received += bytes;
    } while (received < total);

    // socket-ul este inchis
    if(received < total)
        return NULL;

    char *response = new char[SUBLEN + 1];
    memcpy(response, resp, SUBLEN + 1);

    return response;
}
