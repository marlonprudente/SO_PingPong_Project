# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -g -I.
	
contabilizacao: pingpong.o queue.o pingpong-contab.o
	$(CC) -o contab pingpong.c queue.c pingpong-contab.c
	
clean:
	rm *.o contab