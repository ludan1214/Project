CFLAGS = -pthread -Wall -g
CC     = g++ $(CFLAGS)

all : client server
	$(CC) -c *.cc
client : client.o gc.h
	$(CC) -o client client.o
server : server.o gc.h
	$(CC) -o server server.o
clean:
	rm -f client server userinfo/* downloads/* *.o
	rmdir userinfo downloads
