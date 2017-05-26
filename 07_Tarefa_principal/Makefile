# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -g -I.
	
maintask: pingpong.o queue.o pingpong-maintask.o
	$(CC) -o maintask pingpong.c queue.c pingpong-maintask.c
	
clean:
	rm *.o maintask