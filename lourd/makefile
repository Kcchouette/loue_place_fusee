Debug: all
Release: all

all:  client.o serveur.o

client.o: client.c
	gcc -std=c99 client.c -o client.o

serveur.o: serveur.c
	gcc -std=c99 serveur.c -o serveur.o

clean:
	  rm -rf *.o all