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
#include <vector>
#include <map>
#include <iterator>
#include "helpers.h"

using namespace std;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int portno, sockfd_udp, sockfd_tcp, newsockfd, yes = 1;
	short int length;
	char type, cmd[200], *buffer, *topic;
	struct sockaddr_in serv_addr, cli_addr;
	int n, i;
	socklen_t clilen = sizeof(sockaddr);
	bool alive = true;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire
	FD_ZERO(&read_fds);

	if((portno = atoi(argv[1])) == 0)
        error("ERROR: incorrect port");

    // se deschid socketii tcp si udp
	if((sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error("ERROR opening socket for tcp");

	if((sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        error("ERROR opening socket for udp");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	memset((char *) &cli_addr, 0, sizeof(cli_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	if(setsockopt(sockfd_tcp, SOL_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0)
		error("ERROR: no_delay didn't work");

	if(bind(sockfd_tcp, (struct sockaddr *) &serv_addr,
		sizeof(struct sockaddr)) < 0)
	{
		error("ERROR: bind for tcp didn't work");
	}

	if(bind(sockfd_udp, (struct sockaddr *) &serv_addr,
		sizeof(struct sockaddr)) < 0)
	{
		error("ERROR: bind for udp didn't work");
	}

	if(listen(sockfd_tcp, BACKLOG) < 0)
		error("ERROR: listen didn't work");

	// se adauga cei doi socketi si stdin-ul
	FD_SET(0, &read_fds);
	FD_SET(sockfd_tcp, &read_fds);
	FD_SET(sockfd_udp, &read_fds);
	fdmax = max(sockfd_tcp, sockfd_udp);

	map<char*, pair<pair<int, int>, vector<pair<int, char*>>>, cmp_str> msgs;
	map<char*, vector<pair<char*, int>>, cmp_str> subs;
	map<int, char*> IDs;
	map<char*, int, cmp_str> sks;
	map<char*, int, cmp_str> all;
	map<int, pair<int, short>> addrs;

	while (alive) {		
		tmp_fds = read_fds; 

		if(select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) < 0)
			error("ERROR: select didn't work");

		for (i = 0; i <= fdmax; i++) {

			if (FD_ISSET(i, &tmp_fds)) {

				// s-a primit o comanda de la tastatura
				if(i == 0) {
					bzero(cmd, sizeof(cmd));

					if(fgets(cmd, sizeof(cmd), stdin) == NULL)
						error("ERROR: reading from keyboard didn't work");

					cmd[strlen(cmd) - 1] = '\0';

					// serverul de inchide
					if(strcmp(cmd, "exit") == 0) {
						alive = false;
						break;
					}
					continue;
				}

				// s-au primit date pe socket-ul udp
				if(i == sockfd_udp) {
					buffer = new char[TCPLEN + 2]();
					if((n = recvfrom(sockfd_udp, buffer + 2, UDPLEN, 0,
									(struct sockaddr*)&cli_addr, &clilen)) < 0)
						error("ERROR: recvfrom didn't work");
					topic = new char[51]();
					memcpy(topic, buffer + 2, 50);

					// nu exista topicul
					if(msgs.find(topic) == msgs.end()) {
						msgs.insert(pair<char*, pair<pair<int, int>,
									vector<pair<int, char*>>>>
								   	(topic, pair<pair<int, int>,
							   		vector<pair<int, char*>>>
							   		(pair<int, int>(0, 0),
								   	vector<pair<int, char*>>())));
						all.insert(pair<char*, int>(topic, 0));
						delete[] buffer;
						continue;
					}
					// topicul nu are subscriberi
					if(all[topic] == 0) {
						delete[] topic;
						delete[] buffer;
						continue;
					}
					// clientii cu sf = 1
					int nr_subs = msgs[topic].first.first;

					// cream mesajul ce urmeaza sa fie trimis
					length = htons(n + 6);
					memcpy(buffer, &length, 2);
					memcpy(buffer + n + 2, &cli_addr.sin_addr.s_addr, 4);
					memcpy(buffer + n + 6, &cli_addr.sin_port, 2);

					for(auto it = subs.begin(); it != subs.end(); ++it) {
						int sock = sks[it->first];

						// subscriberul este activ
						if(sock != -1) {
							auto it2 = it->second.begin();
							for(; it2 != it->second.end(); ++it2) {
								// este abonat la topic
								if(strcmp(it2->first, topic) == 0) {
									if(it2->second != -1) {
										it2->second++;
										nr_subs--;
									}
									send_to(sock, buffer, n + 8);
								}
							}
						}
					}

					if(nr_subs == 0) {
						msgs[topic].first.second++;
					}

					// Avem subscriberi cu sf = 1 deconectati
					else {
						char *tmp_buf = new char[n + 8]();
						memcpy(tmp_buf, buffer, n + 8);
						msgs[topic].second.push_back(pair<int, char*>(nr_subs,
													tmp_buf));
					}
					delete[] topic;
					delete[] buffer;

				} else if (i == sockfd_tcp) {
					// a venit o cerere de conexiune pe socket-ul inactiv,
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					if((newsockfd = accept(sockfd_tcp,
						(struct sockaddr *) &cli_addr, &clilen)) < 0) 
					{
						error("ERROR: accept didn't work");
					}

					// se adauga noul socket intors de accept() la multimea
					// descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax)
						fdmax = newsockfd;

					addrs[newsockfd] = pair<int, short>
										(cli_addr.sin_addr.s_addr,
										ntohs(cli_addr.sin_port));
				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					char *msg = recv_from_sub(i);

					if (msg == NULL) {
						// conexiunea s-a inchis
						printf("Client %s disconnected.\n", IDs[i]); 
						close(i);
						sks[IDs[i]] = -1;
						IDs[i] = NULL;
						
						// se scoate din multimea de citire socket-ul inchis
						FD_CLR(i, &read_fds);
						if(fdmax == i)
							fdmax--;
					} else {
						topic = new char[51]();
						memcpy(topic, msg, 50);
						type = msg[50];
						char sf = msg[51];
						delete[] msg;

						// topic reprezinta ID-ul
						if(type == 0) {
							int port = addrs[i].second;
							cli_addr.sin_addr.s_addr = addrs[i].first;

							// clientul este deja conectat
							if(sks.find(topic) != sks.end() && sks[topic]!= -1)
							{
								close(i);
								if(fdmax == i)
									fdmax--;
								continue;
							}

							IDs[i] = topic;
							sks[topic] = i;

							printf("New client %s connected from %s:%hd.\n",
								topic, inet_ntoa(cli_addr.sin_addr), port);

							// este un subscriber nou
							if(subs.find(topic) == subs.end()) {
								subs.insert(pair<char*,vector<pair<char*,int>>>
										(topic, vector<pair<char*, int>>()));
								continue;
							}

							auto it = subs[topic].begin();
							for(; it != subs[topic].end(); ++it) {
								char *topic2 = it->first;
								int sf = it->second;

								// trebuie sa primeasca mesajele pastrate
								if(sf != -1) {
									int index = msgs[topic2].first.second;
									auto it2 = msgs[topic2].second.begin();
									it2 = it2 + sf - index;
									auto temp = it2 - 1;

									if((long int)(msgs[topic2].second.end()
													- it2) <= 0)
										continue;

									while(it2 != msgs[topic2].second.end())
									{
										if(temp == it2) {
											++it2;
											continue;
										}
										temp = it2;
										it->second++;
										it2->first--;
										short int len = 0;
										memcpy(&len, it2->second, 2);
										len = ntohs(len) + 2;
										send_to(i, it2->second, len);

										// mesajul a fost trimis tuturor
										if(it2->first == 0) {
											delete[] it2->second;
											msgs[topic2].first.second++;
											it2 = msgs[topic2].second.erase(it2);
											if(msgs[topic2].second.size() == 0)
												break;
										}
										else
											++it2;
									}
								}
							}
							continue;
						}
						
						// nu exista topicul
						if(msgs.find(topic) == msgs.end()) {
							char mess[SUBLEN + 1];
							short int len = htons(SUBLEN - 1);
							memcpy(mess, &len, 2);
							memcpy(mess + 2, topic, 50);
							mess[52] = 4;
							send_to(i, mess, SUBLEN + 1);
							continue;
						}

						bool ok = true;
						auto it = subs[IDs[i]].begin();
						
						for(; it != subs[IDs[i]].end(); ++it) {
							if(strcmp(it->first, topic) == 0) {
								// unsubscribe
								if(type == 1) {
									all[topic]--;
									// avea sf == 1
									if(it->second != -1)
										msgs[topic].first.first--;
									delete[] it->first;
									subs[IDs[i]].erase(it);
								}
								// daduse deja subscribe si verificam daca sf
								// are aceeasi valoare ca cea anterioara
								else if((sf == 0 && it->second != -1) ||
										(sf == 1 && it->second == -1)) {
									if(sf == 0) {
										msgs[topic].first.first--;
										it->second = -1;
									}
									else {
										msgs[topic].first.first++;
										it->second = msgs[topic].first.second;
									}
								}
								ok = false;
								delete[] topic;
								break;
							}
						}
						// subscribe 
						if(ok && type == 2) {
							all[topic]++;
							int val = -1;
							if(sf == 1) {
								msgs[topic].first.first++;
								val = msgs[topic].first.second;
							}
							subs[IDs[i]].push_back(pair<char*, int>
													(topic, val));
						}
					}
				}
			}
		}
	}

	close(sockfd_tcp);

	return 0;
}
