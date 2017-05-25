# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -g -I.
	
preempcao: pingpong.o queue.o pingpong-preempcao.o
	$(CC) -o preempcao pingpong.c queue.c pingpong-preempcao.c
	
clean:
	rm *.o preempcao