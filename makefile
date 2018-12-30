CC=gcc
CFLAGS=-std=c99 -Wall -pedantic -g3

farthing: farthing.c
	${CC} ${CFLAGS} -o $@ $^