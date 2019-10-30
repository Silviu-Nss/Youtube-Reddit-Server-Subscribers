// Nastasescu George-Silviu, 321CB

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <locale>
#include <cstdint>
#include "helpers.h"

using namespace std;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s client_id server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, temp, len;
	char buffer[300], mess[70], buf_to[SUBLEN], *buf_from, type = 7;
	char sign, msg[PAYLEN + 1], topic[51], tip[11];

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 4) {
		usage(argv[0]);
	}
	len = strlen(argv[1]);
	if(len > 10)
		error("ERROR: ID is too long");

	// se deschide socket-ul
	sockfd = open_connection(argv[2], atoi(argv[3]));

	// se goleste multimea de descriptori de citire
	FD_ZERO(&read_fds);

	// se adauga noul socket si stdin-ul in multime
	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd + 1;

	// trimitem serverului ID-ul clientului
	bzero(buffer, sizeof(buffer));
	memcpy(buffer, argv[1], len);
	buffer[50] = 0;
	send_to(sockfd, buffer, SUBLEN);

	while (1) {

		tmp_fds = read_fds;
		type = 7;
				
		if(select(fdmax, &tmp_fds, NULL, NULL, NULL) < 0)
			error("ERROR: select didn't work");

  		// se citeste de la tastatura
		if (FD_ISSET(0, &tmp_fds)) {
			// parsam mesajul
			while(1) {
				bzero(buffer, 300);
				bzero(buf_to, SUBLEN);
				bzero(mess, 70);

				// am primit un mesaj de la tastatura
				if(fgets(buffer, 300, stdin) == NULL)
					error("ERROR: reading from keyboard didn't work");

				buffer[strlen(buffer) - 1] = '\0';

				char *p = strtok(buffer, " \t\n");
				if(p == NULL)
					break;

				// clientul de deconecteaza
				if(strcmp(p, "exit") == 0) {
					type = -1;
					break;
				}
				else if(strcmp(p, "unsubscribe") == 0)
					type = 1;
				else if(strcmp(p, "subscribe") == 0)
					type = 2;
				else {
					printf("The first parameter is incorrect.\n");
					break;
				}

				if((p = strtok(NULL, " \t\n")) == NULL) {
					printf("The second parameter is incorrect.\n");
					break;
				}

				memcpy(buf_to, p, 50);

				if(type == 1)
					sprintf(mess, "Unsubscribed %s.", buf_to);
				else
					sprintf(mess, "Subscribed %s.", buf_to);

				buf_to[50] = type;

				if(type == 2) {
					if((p = strtok(NULL, " \t\n")) == NULL) {
						printf("The third parameter is incorrect.\n");
						break;
					}
					if(strlen(p) != 1 || (*p != '0' && *p != '1')) {
						printf("The third parameter is incorrect.\n");
						break;
					}
					buf_to[51] = *p - '0';
				}
				// se trimite mesajul la server si se printeaza comanda
				send_to(sockfd, buf_to, SUBLEN);
				printf("%s\n", mess);
				break;
			}
			// clientul se deconecteaza
			if(type == -1)
				break;
		}
		// am primit un mesaj de la server
		if (FD_ISSET(sockfd, &tmp_fds)) {

			// serverul s-a inchis
			if((buf_from = recv_from_server(sockfd, len)) == NULL)
				break;
			memcpy(&temp, buf_from, 4);
			if(temp == 0)
				break;

			char num[12] = {0}, msg2[270] = {0};
			uint32_t left;		// partea intreaga
			uint16_t real;		// numarul short_real
			uint8_t right;		// partea fractionara
			int ip;
			short int port;
			struct sockaddr_in cli_addr;

			// parsam mesajul de la server
			bzero(topic, sizeof(topic));
			bzero(tip, sizeof(tip));
			bzero(msg, sizeof(msg));
			memcpy(topic, buf_from, 50);
			memcpy(&ip, buf_from + len - 6, 4);
			memcpy(&port, buf_from + len - 2, 2);
			cli_addr.sin_addr.s_addr = ip;
			port = ntohs(port);
			type = buf_from[50];

			switch(type) {
				case 0:
					strcpy(tip, "INT");
					sign = buf_from[51];
					if(sign == 1)
						msg[0] = '-';
					memcpy(&left, buf_from + 52, 4);
					left = ntohl(left);
					sprintf(msg + sign, "%d%c", left, '\0');
					break;
				case 1:
					strcpy(tip, "SHORT_REAL");
					memcpy(&real, buf_from + 51, 2);
					left = ntohs(real);
					right = left % 100;
					if(right % 10 == 0)
						right /= 10;
					left /= 100;
					sprintf(msg, "%d.%d%c", left, right, '\0');
					break;
				case 2:
					strcpy(tip, "FLOAT");
					sign = buf_from[51];
					if(sign == 1)
						msg[0] = '-';
					memcpy(&left, buf_from + 52, 4);
					left = ntohl(left);
					right = buf_from[56];
					if(right == 0)
						sprintf(msg + sign, "%d%c", left, '\0');
					else {
						sprintf(num, "%d%c", left, '\0');
						int lg = strlen(num);
						// pozitia punctului din numarul float
						int pos = max(lg - right, 1);
						// numarul de zerouri dinaintea numarului dat
						int zeros = max(right - lg + 1, 0);

						memset(msg2, '0', zeros);
						memcpy(msg2 + zeros, num, lg);
						memcpy(msg + sign, msg2, pos);
						msg[pos + sign] = '.';
						memcpy(msg + sign + pos + 1, msg2 + pos,
								lg + zeros - pos + 1);
					}
					break;
				case 3:
					strcpy(tip, "STRING");
					memcpy(msg, buf_from + 51, len - 57);
					msg[len - 57] = '\0';
					break;
				case 4:
					type = -1;
					printf("Topic %s doesn't exist.\n", topic);
					break;
				default:
					type = -1;
					printf("Incorrect message.\n");
					break;
			}
			delete[] buf_from;
			if(type == -1)
				continue;

			printf("%s:%hd - %s - %s - %s\n", inet_ntoa(cli_addr.sin_addr),
					port, topic, tip, msg);
		}
	}

	close(sockfd);

	return 0;
}
