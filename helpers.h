#ifndef _HELPERS_
#define _HELPERS_

#define UDPLEN 1551
#define TCPLEN 1557
#define PAYLEN 1500
#define SUBLEN 52
#define BACKLOG 10000

struct cmp_str
{
    bool operator()(char const *a, char const *b) const
    {
        return strcmp(a, b) < 0;
    }
};

void error(const char *msg);
int open_connection(char *host_ip, int portno);
void close_connection(int sockfd);
void send_to(int sockfd, char *message, int total);
char *recv_from_sub(int sockfd);
char *recv_from_server(int sockfd, int& len);

#endif
