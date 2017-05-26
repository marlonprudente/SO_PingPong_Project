# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -g -I.
	
join: pingpong.o queue.o pingpong-join.o
	$(CC) -o join pingpong.c queue.c pingpong-join.c
	
clean:
	rm *.o join