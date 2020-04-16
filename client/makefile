#makefile for client

myftp: myftp.o stream.o token.o
	gcc myftp.o stream.o token.o -o myftp

myftp.o: myftp.c
	gcc -c myftp.c

stream.o: stream.c stream.h
	gcc -c stream.c

token.o: token.c token.h
	gcc -c token.c

clean:
	rm *.o
