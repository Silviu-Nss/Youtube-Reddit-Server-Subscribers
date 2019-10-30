CC=g++
CFLAGS=-Wall -Wextra -g -O3

# Portul pe care asculta serverul
PORT_SERVER=5000

# Adresa IP a serverului
IP_SERVER=127.0.0.1

# ID-ul clientului
ID_CLIENT=ana

all: server subscriber

server: server.cpp
		$(CC) $(CFLAGS) -o server server.cpp helpers.cpp

subscriber: subscriber.cpp helpers.cpp
		$(CC) $(CFLAGS) -o subscriber subscriber.cpp helpers.cpp

.PHONY: clean run_server run_client

run_server:
		./server ${PORT_SERVER}

run_subscriber:
		./subscriber ${ID_CLIENT} ${IP_SERVER} ${PORT_SERVER}

clean:
		rm -f *.o server subscriber
